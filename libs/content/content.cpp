// SPDX-License-Identifier: AGPL-3.0-or-later
#include "content.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSysInfo>
#include <QVersionNumber>
#include <QtConcurrent>
#ifdef __GLIBC__
#include <gnu/libc-version.h>
#endif

namespace t64 {
Content::Content(const QString &root, QObject *parent)
    : QObject(parent), root_(root), canceled_(std::make_shared<std::atomic_bool>(false)) {
    QDir().mkpath(root_);
    QFile file(":/assets/content/catalog.json");
    if (file.open(QIODevice::ReadOnly))
        entries_ = QJsonDocument::fromJson(file.readAll()).array().toVariantList();
    connect(&worker_, &QFutureWatcher<QVariantMap>::finished, this, [this] {
        const auto result = worker_.result();
        file_.reset();
        temporary_.reset();
        busy_ = false;
        if (result.contains("error"))
            status_ = result["error"].toString();
        else {
            status_ = active_["kind"] == "engine"
                          ? "Moteur installé ; profil ajouté. Démarrez-le depuis les paramètres."
                          : "PGN reçu ; import dans la bibliothèque…";
            emit ready(result["path"].toString(), result);
        }
        active_.clear();
        emit catalogChanged();
        emit changed();
    });
}
Content::~Content() {
    canceled_->store(true);
    if (reply_) {
        disconnect(reply_, nullptr, this, nullptr);
        reply_->abort();
    }
    worker_.waitForFinished();
}
bool Content::allowedUrl(const QUrl &url, bool engine) {
    if (url.scheme() != "https" || !url.userInfo().isEmpty() || url.port(443) != 443 || url.hasFragment())
        return false;
    const auto host = url.host().toLower();
    if (engine)
        return host == "github.com" || host == "release-assets.githubusercontent.com" ||
               host == "objects.githubusercontent.com";
    return host == "www.pgnmentor.com" || host == "pgnmentor.com";
}
bool Content::compatible(const QVariantMap &entry) {
    if (entry["kind"] != "engine")
        return true;
    if (QSysInfo::kernelType() != "linux" || QSysInfo::currentCpuArchitecture() != "x86_64")
        return false;
#ifdef __GLIBC__
    return QVersionNumber::compare(QVersionNumber::fromString(QString::fromLatin1(gnu_get_libc_version())),
                                   QVersionNumber::fromString(entry.value("minGlibc", "2.36").toString())) >=
           0;
#else
    return false;
#endif
}
QVariantMap Content::entry(const QString &id) const {
    for (const auto &item : entries_)
        if (item.toMap()["id"] == id)
            return item.toMap();
    return {};
}
QString Content::receiptPath(const QString &id) const {
    return root_ + '/' + id + ".json";
}
QVariantMap Content::receipt(const QString &id) const {
    if (entry(id).isEmpty())
        return {};
    QFile file(receiptPath(id));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 128 * 1024)
        return {};
    const auto result = QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
    const auto folder = result["folder"].toString();
    if (!folder.startsWith(id + '-') || !QRegularExpression("^[A-Za-z0-9._-]+$").match(folder).hasMatch())
        return {};
    const auto directory = root_ + '/' + folder;
    const auto path = result["path"].toString();
    if (QFileInfo(directory).isSymLink() || QFileInfo(path).isSymLink() || !QFileInfo(path).isFile() ||
        QFileInfo(path).absolutePath() != QDir(directory).absolutePath())
        return {};
    return result;
}
QVariantList Content::catalog() const {
    QVariantList result;
    for (const auto &value : entries_) {
        auto row = value.toMap();
        const auto installed = receipt(row["id"].toString());
        row["installed"] = !installed.empty();
        row["installedVersion"] = installed["version"];
        row["path"] = installed["path"];
        row["downloadedBytes"] = installed.value("bytes", 0);
        row["compatible"] = compatible(row);
        row["update"] = !installed.empty() && row["kind"] == "engine" && row["sha256"] != installed["sha256"];
        result << row;
    }
    return result;
}
void Content::fail(const QString &reason) {
    if (reply_) {
        auto old = reply_;
        reply_ = nullptr;
        disconnect(old, nullptr, this, nullptr);
        old->abort();
        old->deleteLater();
    }
    file_.reset();
    temporary_.reset();
    busy_ = false;
    active_.clear();
    status_ = reason;
    emit changed();
}
void Content::install(const QString &id) {
    if (busy_)
        return;
    active_ = entry(id);
    if (active_.empty()) {
        fail("Contenu introuvable dans le catalogue.");
        return;
    }
    if (!compatible(active_)) {
        fail("Ce téléchargement nécessite Linux x86_64 et glibc ≥ " + active_["minGlibc"].toString() + '.');
        return;
    }
    const auto existing = receipt(id);
    if (active_["kind"] == "engine" && !existing.empty() && existing["sha256"] == active_["sha256"]) {
        active_.clear();
        useInstalled(id);
        return;
    }
    canceled_->store(false);
    received_ = 0;
    total_ = active_.value("bytes", 0).toLongLong();
    redirects_ = 0;
    temporary_ = std::make_unique<QTemporaryDir>(root_ + "/.download-XXXXXX");
    if (!temporary_->isValid()) {
        fail("Impossible de préparer le téléchargement.");
        return;
    }
    file_ = std::make_unique<QFile>(temporary_->path() + "/payload");
    if (!file_->open(QIODevice::WriteOnly)) {
        fail(file_->errorString());
        return;
    }
    busy_ = true;
    status_ = "Téléchargement : " + active_["title"].toString();
    progressTimer_.start();
    emit changed();
    request(QUrl(active_["url"].toString()));
}
void Content::request(const QUrl &url) {
    if (!allowedUrl(url, active_["kind"] == "engine")) {
        fail("Adresse de téléchargement non autorisée.");
        return;
    }
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(30000);
    request.setRawHeader("User-Agent", "Trait64-content/1.0");
    request.setRawHeader("Accept-Encoding", "identity");
    reply_ = network_.get(request);
    reply_->setReadBufferSize(256 * 1024);
    auto receive = [this] {
        if (!reply_ || !file_ || reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200)
            return;
        const auto bytes = reply_->readAll();
        const auto declared = reply_->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        const auto expected = active_.value("bytes", 0).toLongLong();
        if (declared > 256LL * 1024 * 1024 || (expected && declared > 0 && expected != declared) ||
            received_ + bytes.size() > (expected ? expected : 256LL * 1024 * 1024)) {
            fail("Taille reçue non conforme au catalogue.");
            return;
        }
        if (file_->write(bytes) != bytes.size()) {
            fail(file_->errorString());
            return;
        }
        received_ += bytes.size();
        total_ = expected ? expected : declared;
        if (progressTimer_.elapsed() >= 100) {
            progressTimer_.restart();
            emit changed();
        }
    };
    connect(reply_, &QNetworkReply::readyRead, this, receive);
    connect(reply_, &QNetworkReply::finished, this, [this, receive] {
        if (!reply_)
            return;
        const auto redirect = reply_->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (!redirect.isEmpty()) {
            const auto target = reply_->url().resolved(redirect);
            reply_->deleteLater();
            reply_ = nullptr;
            if (++redirects_ > 5) {
                fail("Trop de redirections.");
                return;
            }
            this->request(target);
            return;
        }
        if (reply_->error() != QNetworkReply::NoError ||
            reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
            fail("Téléchargement impossible : " + reply_->errorString());
            return;
        }
        receive();
        if (!reply_)
            return;
        reply_->deleteLater();
        reply_ = nullptr;
        if (!file_->flush()) {
            fail(file_->errorString());
            return;
        }
        file_->close();
        status_ = "Vérification et extraction…";
        emit changed();
        const auto entry = active_;
        const auto path = file_->fileName();
        const auto root = root_;
        const auto canceled = canceled_;
        worker_.setFuture(QtConcurrent::run([entry, path, root, canceled] {
            try {
                return installPackage(entry, path, root, canceled);
            } catch (const std::exception &e) {
                return QVariantMap{{"error", QString::fromUtf8(e.what())}};
            }
        }));
    });
}
void Content::cancel() {
    if (!busy_)
        return;
    canceled_->store(true);
    if (reply_)
        fail("Téléchargement annulé.");
    else {
        status_ = "Annulation de l’extraction…";
        emit changed();
    }
}
void Content::useInstalled(const QString &id) {
    if (busy_)
        return;
    const auto installed = receipt(id);
    if (installed.empty()) {
        fail("Contenu local introuvable. Téléchargez-le à nouveau.");
        return;
    }
    emit ready(installed["path"].toString(), installed);
}
void Content::remove(const QString &id) {
    if (busy_)
        return;
    const auto installed = receipt(id);
    if (installed.empty())
        return;
    emit aboutToRemove(installed["path"].toString());
    if (!QDir(root_ + '/' + installed["folder"].toString()).removeRecursively()) {
        fail("Impossible de supprimer les fichiers locaux.");
        return;
    }
    QFile::remove(receiptPath(id));
    emit catalogChanged();
    status_ = "Fichiers téléchargés supprimés. Les parties importées restent dans la bibliothèque.";
    emit changed();
}
} // namespace t64
