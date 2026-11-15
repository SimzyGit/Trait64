// SPDX-License-Identifier: AGPL-3.0-or-later
#include "syzygy_download.h"
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QtConcurrent>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>
using std::size_t;
#include "citycrc.h"

namespace t64 {
namespace syzygy {
QStringList tableNames(int pieces) {
    // L'ordre de Fathom : dame, tour, fou, cavalier, pion. Un camp se note par ses pièces
    // dans cet ordre ; le camp le mieux pourvu vient d'abord, et à nombre égal celui dont
    // la suite est la plus forte.
    static const char order[] = "QRBNP";
    std::vector<std::vector<int>> sides[6];
    for (int len = 0; len < 6; ++len) {
        std::vector<int> t(len, 0);
        while (true) {
            sides[len].push_back(t);
            int i = len - 1;
            while (i >= 0 && t[i] == 4)
                --i;
            if (i < 0)
                break;
            ++t[i];
            for (int k = i + 1; k < len; ++k)
                t[k] = t[i];
        }
    }
    const auto text = [](const std::vector<int> &side) {
        QString s("K");
        for (const int piece : side)
            s += QChar(order[piece]);
        return s;
    };
    QStringList names;
    const int extra = pieces - 2;
    if (extra < 1 || extra > 5)
        return names;
    for (int strong = extra; strong * 2 >= extra; --strong) {
        const int weak = extra - strong;
        for (const auto &white : sides[strong])
            for (const auto &black : sides[weak])
                if (strong > weak || black >= white)
                    names << text(white) + 'v' + text(black);
    }
    return names;
}

bool intact(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || (file.size() & 63) != 16)
        return false;
    const auto size = size_t(file.size()) & ~size_t(63);
    const auto *data = reinterpret_cast<const char *>(file.map(0, file.size()));
    if (!data)
        return false;
    // Le calcul du générateur de Ronald de Man : CityHashCrc256 par tranches de 16 Mio,
    // puis CityHashCrc128 de la suite des résultats.
    constexpr size_t chunk = size_t(1) << 24;
    const size_t chunks = (size + chunk - 1) / chunk;
    std::vector<uint64_t> results(4 * std::max<size_t>(chunks, 1));
    for (size_t i = 0; i < chunks; ++i)
        CityHashCrc256(data + i * chunk, std::min(chunk, size - i * chunk), &results[4 * i]);
    uint64_t sum[2];
    CityHashCrc128(reinterpret_cast<const char *>(results.data()), 32 * chunks, sum);
    return std::memcmp(sum, data + size, 16) == 0;
}

qint64 packBytes(const QString &pack) {
    // 939 Mio et 149,2 Gio, tables WDL et DTZ ensemble.
    return pack == "6" ? qint64(149.2 * 1024 * 1024 * 1024) : qint64(939) * 1024 * 1024;
}
} // namespace syzygy

SyzygyDownload::SyzygyDownload(QObject *parent, QNetworkAccessManager *network)
    : QObject(parent), network_(network ? network : new QNetworkAccessManager(this)) {}

QStringList SyzygyDownload::candidates(const QString &pack, const QString &file) {
    const bool wdl = file.endsWith(".rtbw");
    const QString lichess("https://tablebase.lichess.ovh/tables/standard/"),
        sesse("https://tablebase.sesse.net/syzygy/");
    if (pack == "6")
        return {lichess + (wdl ? "6-wdl/" : "6-dtz/") + file, sesse + (wdl ? "6-WDL/" : "6-DTZ/") + file};
    return {lichess + "3-4-5/" + file, lichess + (wdl ? "3-4-5-wdl/" : "3-4-5-dtz/") + file,
            sesse + "3-4-5/" + file};
}

void SyzygyDownload::start(const QString &folder, const QString &pack) {
    if (busy())
        return;
    folder_ = QDir(folder).filePath(pack);
    pack_ = pack;
    files_.clear();
    for (const int pieces : pack == "6" ? QList<int>{6} : QList<int>{3, 4, 5})
        for (const auto &name : syzygy::tableNames(pieces))
            files_ << name + ".rtbw" << name + ".rtbz";
    index_ = mirror_ = tried_ = 0;
    received_ = 0;
    if (!QDir().mkpath(folder_)) {
        fail(tr("Impossible de créer le dossier %1.").arg(QDir::toNativeSeparators(folder_)));
        return;
    }
    next();
}

void SyzygyDownload::next() {
    // Un fichier déjà là a été vérifié en arrivant : on ne le relit pas, un paquet de six
    // pièces pèse 150 Gio.
    while (index_ < files_.size()) {
        const QFileInfo done(QDir(folder_).filePath(files_[index_]));
        if (!done.exists() || (done.size() & 63) != 16)
            break;
        ++index_;
    }
    emit progress(index_, int(files_.size()), received_);
    if (index_ == files_.size()) {
        emit finished({});
        return;
    }
    fetch();
}

void SyzygyDownload::fetch() {
    const auto target = QDir(folder_).filePath(files_[index_]);
    part_ = std::make_unique<QFile>(target + ".part");
    if (!part_->open(QIODevice::ReadWrite | QIODevice::Append)) {
        fail(part_->errorString());
        return;
    }
    const auto urls = candidates(pack_, files_[index_]);
    QNetworkRequest request(QUrl(urls[mirror_ % urls.size()]));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(60000);
    request.setRawHeader("User-Agent", "Trait64-syzygy/1.0");
    // Une partie déjà reçue : on demande la suite.
    const bool resuming = part_->size() > 0;
    if (resuming)
        request.setRawHeader("Range", "bytes=" + QByteArray::number(part_->size()) + '-');
    reply_ = network_->get(request);
    auto *reply = reply_.data();
    // Écrit ce qui est arrivé ; faux si le disque refuse.
    const auto drain = [this, reply, resuming, first = std::make_shared<bool>(true)] {
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200 && status != 206)
            return true;
        // Le serveur renvoie tout le fichier au lieu de la suite : on repart de zéro.
        if (std::exchange(*first, false) && resuming && status == 200)
            part_->resize(0);
        const auto bytes = reply->readAll();
        if (part_->write(bytes) != bytes.size())
            return false;
        received_ += bytes.size();
        emit progress(index_, int(files_.size()), received_);
        return true;
    };
    connect(reply, &QNetworkReply::readyRead, this, [reply, drain] {
        if (!drain())
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, target, urls, drain] {
        reply->deleteLater();
        if (reply_ != reply)
            return; // annulé
        reply_.clear();
        if (!drain()) {
            fail(part_->errorString());
            return;
        }
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool complete = (status == 200 || status == 206) && reply->error() == QNetworkReply::NoError;
        // 416 : la partie reçue était déjà le fichier entier.
        if (!complete && status != 416) {
            part_->close();
            if (reply->error() == QNetworkReply::OperationCanceledError && part_->error() != QFile::NoError) {
                fail(part_->errorString());
                return;
            }
            // Un miroir qui ne l'a pas : le suivant. Tous ont échoué : on s'arrête.
            if (++tried_ < urls.size()) {
                mirror_ = (mirror_ + 1) % int(urls.size());
                fetch();
                return;
            }
            fail(tr("%1 introuvable sur les miroirs : %2").arg(files_[index_], reply->errorString()));
            return;
        }
        part_->close();
        checking_ = true;
        const auto partPath = part_->fileName();
        auto *watcher = new QFutureWatcher<bool>(this);
        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, partPath, target] {
            watcher->deleteLater();
            checking_ = false;
            if (!watcher->result()) {
                QFile::remove(partPath);
                fail(tr("%1 est arrivé abîmé : relancez le téléchargement pour le reprendre.")
                         .arg(QFileInfo(target).fileName()));
                return;
            }
            QFile::remove(target);
            if (!QFile::rename(partPath, target)) {
                fail(tr("Impossible de nommer %1.").arg(QDir::toNativeSeparators(target)));
                return;
            }
            ++index_;
            tried_ = 0;
            next();
        });
        watcher->setFuture(QtConcurrent::run([partPath] { return syzygy::intact(partPath); }));
    });
}

void SyzygyDownload::fail(const QString &error) {
    part_.reset();
    emit finished(error);
}

void SyzygyDownload::cancel() {
    if (auto *reply = reply_.data()) {
        reply_.clear();
        reply->abort();
        part_.reset(); // la partie reçue reste sur le disque, pour reprendre plus tard
        emit finished(tr("Téléchargement interrompu : il reprendra où il s’est arrêté."));
    }
}
} // namespace t64
