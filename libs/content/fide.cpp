// SPDX-License-Identifier: AGPL-3.0-or-later
#include "fide.h"
#include <QFile>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QXmlStreamReader>
#include <algorithm>
#include <archive.h>
#include <archive_entry.h>
#include <memory>
#include <stdexcept>

namespace t64 {
namespace {
void require(bool ok, const QString &error) {
    if (!ok)
        throw std::runtime_error(error.toStdString());
}
} // namespace

bool FideList::validId(const QString &fideId) {
    return !fideId.isEmpty() && fideId.size() <= 12 &&
           std::all_of(fideId.cbegin(), fideId.cend(), [](QChar c) { return c.isDigit(); });
}

QUrl FideList::listUrl(const QString &speed) {
    if (speed != "standard" && speed != "rapid" && speed != "blitz")
        return {};
    return QUrl("https://ratings.fide.com/download/" + speed + "_rating_list_xml.zip");
}

FidePlayer FideList::findInChunks(const std::function<QByteArray()> &next, const QString &fideId) {
    require(validId(fideId), "L’identifiant FIDE ne contient que des chiffres.");
    QXmlStreamReader xml;
    FidePlayer result;
    // Champs du joueur en cours. On les rassemble puis on les examine à la fermeture de
    // <player> : l'ordre des éléments n'a donc pas d'importance, et un champ manquant
    // n'empêche pas de lire les autres.
    QHash<QString, QString> fields;
    QString element;
    bool insidePlayer = false, exhausted = false;
    auto feed = [&] {
        if (exhausted)
            return false;
        const auto chunk = next();
        if (chunk.isEmpty()) {
            // Plus rien à lire : la boucle s'arrête sur ce faux, sans quoi elle
            // attendrait indéfiniment un bloc qui ne viendra pas.
            exhausted = true;
            return false;
        }
        xml.addData(chunk);
        return true;
    };
    while (!result.found) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::Invalid) {
            if (xml.error() != QXmlStreamReader::PrematureEndOfDocumentError)
                require(false, "Liste FIDE illisible : " + xml.errorString());
            if (!feed())
                break;
            continue;
        }
        if (token == QXmlStreamReader::EndDocument)
            break;
        if (token == QXmlStreamReader::StartElement) {
            element = xml.name().toString().toLower();
            if (element == "player") {
                insidePlayer = true;
                fields.clear();
            }
            continue;
        }
        if (token == QXmlStreamReader::Characters && insidePlayer && !element.isEmpty() &&
            element != "player") {
            // Un bloc peut couper un texte en deux : on concatène au lieu d'écraser.
            fields[element] += xml.text().toString();
            continue;
        }
        if (token != QXmlStreamReader::EndElement)
            continue;
        const auto closing = xml.name().toString().toLower();
        if (closing != "player") {
            element.clear();
            continue;
        }
        insidePlayer = false;
        element.clear();
        if (fields.value("fideid").trimmed() != fideId)
            continue;
        result.found = true;
        result.id = fideId;
        result.name = fields.value("name").simplified();
        result.country = fields.value("country").simplified();
        result.title = fields.value("title").simplified();
        result.rating = fields.value("rating").trimmed().toInt();
        result.games = fields.value("games").trimmed().toInt();
        result.birthYear = fields.value("birthday").trimmed().left(4).toInt();
    }
    return result;
}

FidePlayer FideList::findInArchive(const QString &zipPath, const QString &fideId) {
    require(QFile::exists(zipPath), "Liste FIDE téléchargée introuvable.");
    std::unique_ptr<struct archive, decltype(&archive_read_free)> reader(archive_read_new(),
                                                                        &archive_read_free);
    require(bool(reader), "Lecture d’archive indisponible.");
    archive_read_support_filter_all(reader.get());
    archive_read_support_format_all(reader.get());
    require(archive_read_open_filename(reader.get(), QFile::encodeName(zipPath).constData(), 65536) ==
                ARCHIVE_OK,
            "Liste FIDE illisible.");
    struct archive_entry *item = nullptr;
    int entries = 0;
    while (archive_read_next_header(reader.get(), &item) == ARCHIVE_OK) {
        require(++entries <= 64, "Archive FIDE inattendue : trop de fichiers.");
        require(!archive_entry_symlink(item) && !archive_entry_hardlink(item),
                "Liens interdits dans l’archive FIDE.");
        if (archive_entry_filetype(item) != AE_IFREG) {
            archive_read_data_skip(reader.get());
            continue;
        }
        const auto name = QString::fromUtf8(archive_entry_pathname(item));
        if (!name.endsWith(".xml", Qt::CaseInsensitive)) {
            archive_read_data_skip(reader.get());
            continue;
        }
        qint64 read = 0;
        auto next = [&]() -> QByteArray {
            QByteArray buffer(256 * 1024, Qt::Uninitialized);
            const auto n = archive_read_data(reader.get(), buffer.data(), buffer.size());
            require(n >= 0, "Liste FIDE tronquée.");
            read += n;
            // La liste standard pèse quelques centaines de mégaoctets décompressée ; au
            // delà de deux gigaoctets, c'est que le fichier n'est pas celui qu'on croit.
            require(read <= 2LL * 1024 * 1024 * 1024, "Liste FIDE anormalement volumineuse.");
            buffer.resize(int(n));
            return buffer;
        };
        const auto player = findInChunks(next, fideId);
        if (player.found)
            return player;
    }
    return {};
}

FideSync::FideSync(QObject *parent, QNetworkAccessManager *network)
    : QObject(parent), network_(network ? network : new QNetworkAccessManager(this)) {}

FideSync::~FideSync() = default;

bool FideSync::busy() const {
    return !reply_.isNull();
}

void FideSync::cancel() {
    pending_.clear();
    if (reply_)
        reply_->abort();
}

void FideSync::cleanup() {
    if (output_) {
        output_->remove();
        output_.reset();
    }
}

void FideSync::fail(const QString &message) {
    pending_.clear();
    cleanup();
    emit failed(message);
}

void FideSync::fetch(const QString &fideId, const QStringList &speeds, const QString &workDirectory) {
    if (busy())
        return fail(tr("Un téléchargement FIDE est déjà en cours."));
    if (!FideList::validId(fideId))
        return fail(tr("L’identifiant FIDE ne contient que des chiffres."));
    pending_.clear();
    // On garde l'ordre publié plutôt que celui de la demande : le nom et le pays sont
    // lus dans la première liste trouvée, et la liste classique est la plus complète.
    for (const auto &speed : {QStringLiteral("standard"), QStringLiteral("rapid"), QStringLiteral("blitz")})
        if (speeds.contains(speed))
            pending_ << speed;
    if (pending_.isEmpty())
        return fail(tr("Choisissez au moins une cadence."));
    id_ = fideId;
    directory_ = workDirectory;
    found_.clear();
    done_ = 0;
    count_ = int(pending_.size());
    startNext();
}

void FideSync::startNext() {
    if (pending_.isEmpty()) {
        const bool any = found_.contains("standard") || found_.contains("rapid") || found_.contains("blitz");
        found_["found"] = any;
        emit finished(found_);
        return;
    }
    current_ = pending_.takeFirst();
    const auto url = FideList::listUrl(current_);
    if (url.isEmpty())
        return fail(tr("Cadence inconnue : ") + current_);
    const auto path = directory_ + "/fide-" + current_ + ".zip";
    output_ = std::make_unique<QFile>(path);
    if (!output_->open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(tr("Écriture impossible : ") + path);
    QNetworkRequest request(url);
    request.setTransferTimeout(120000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    reply_ = network_->get(request);
    connect(reply_, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        emit progress(current_, done_ + 1, count_, received, total);
    });
    connect(reply_, &QNetworkReply::readyRead, this, [this] {
        if (!reply_ || !output_)
            return;
        const auto bytes = reply_->readAll();
        if (output_->write(bytes) != bytes.size()) {
            const auto message = output_->errorString();
            reply_->abort();
            fail(tr("Écriture impossible : ") + message);
        }
    });
    connect(reply_, &QNetworkReply::finished, this, [this, reply = reply_.data(), path] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool canceled = reply->error() == QNetworkReply::OperationCanceledError;
        const auto error = reply->errorString();
        reply_.clear();
        if (!output_)
            return; // Un échec d'écriture a déjà été signalé.
        if (canceled)
            return fail(tr("Téléchargement FIDE annulé."));
        if (reply->error() != QNetworkReply::NoError || status != 200)
            return fail(tr("Liste FIDE indisponible (%1). La fédération publie ses listes sur "
                           "ratings.fide.com ; l’adresse a peut-être changé.")
                            .arg(status ? QString::number(status) : error));
        output_->close();
        finishList(path);
    });
}

void FideSync::finishList(const QString &path) {
    try {
        const auto player = FideList::findInArchive(path, id_);
        cleanup();
        if (player.found) {
            // Nom, pays et titre viennent de la première liste où le joueur figure.
            if (!found_.contains("name")) {
                found_["name"] = player.name;
                found_["country"] = player.country;
                found_["title"] = player.title;
                found_["birthYear"] = player.birthYear;
            }
            if (player.rating > 0)
                found_[current_] = player.rating;
        }
        ++done_;
        startNext();
    } catch (const std::exception &e) {
        fail(QString::fromUtf8(e.what()));
    }
}

} // namespace t64
