// SPDX-License-Identifier: AGPL-3.0-or-later
#include "content.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStorageInfo>
#include <QSysInfo>
#include <QUuid>
#include <archive.h>
#include <archive_entry.h>
#include <stdexcept>

namespace t64 {
namespace {
void require(bool ok, const QString &error) {
    if (!ok)
        throw std::runtime_error(error.toStdString());
}
bool nameSafe(const QString &s) {
    return QRegularExpression("^[A-Za-z0-9][A-Za-z0-9._-]{0,120}$").match(s).hasMatch();
}
bool pathSafe(const QString &s) {
    return !s.isEmpty() && !s.startsWith('/') && !s.contains('\\') && !s.contains(':') &&
           !s.split('/').contains("..") && !s.contains(QChar(0));
}
QString hashFile(const QString &path, const std::shared_ptr<std::atomic_bool> &cancel) {
    QFile f(path);
    require(f.open(QIODevice::ReadOnly), f.errorString());
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!f.atEnd()) {
        require(!cancel->load(), "Téléchargement annulé.");
        auto bytes = f.read(256 * 1024);
        require(f.error() == QFileDevice::NoError, f.errorString());
        hash.addData(bytes);
    }
    return QString::fromLatin1(hash.result().toHex());
}
} // namespace
QVariantMap Content::installPackage(const QVariantMap &entry, const QString &file, const QString &root,
                                    const std::shared_ptr<std::atomic_bool> &cancel) {
    const bool engine = entry["kind"] == "engine";
    const QString id = entry["id"].toString(), version = entry["version"].toString();
    require(nameSafe(id) && nameSafe(version), "Identifiant de contenu invalide.");
    require(QFileInfo(file).isFile() && !QFileInfo(file).isSymLink(), "Téléchargement introuvable.");
    const auto size = QFileInfo(file).size();
    require(size > 0 && size <= 256LL * 1024 * 1024, "Taille du téléchargement non autorisée.");
    if (engine) {
        require(compatible(entry), "Ce moteur est proposé pour Linux x86_64. Vous pouvez ajouter un autre "
                                   "exécutable UCI manuellement.");
        require(size == entry["bytes"].toLongLong(), "La taille du moteur ne correspond pas au catalogue.");
        require(QRegularExpression("^[0-9a-f]{64}$").match(entry["sha256"].toString()).hasMatch(),
                "Empreinte du moteur absente.");
    }
    const auto digest = hashFile(file, cancel);
    if (!entry["sha256"].toString().isEmpty())
        require(digest == entry["sha256"].toString(),
                "Échec de vérification SHA-256. Aucun contenu installé.");
    require(QDir().mkpath(root), "Impossible de créer le dossier de contenu.");
    QStorageInfo storage(root);
    require(!storage.isValid() || storage.bytesAvailable() > 600LL * 1024 * 1024,
            "Au moins 600 Mio libres sont nécessaires à l’extraction sûre.");
    QTemporaryDir temporary(root + "/.install-XXXXXX");
    require(temporary.isValid(), "Impossible de préparer l’installation.");
    const auto format = entry["format"].toString();
    QMap<QString, QString> expected;
    const auto files = entry["files"].toMap();
    for (auto it = files.begin(); it != files.end(); ++it) {
        require(pathSafe(it.key()) && nameSafe(it.value().toString()), "Chemin de catalogue invalide.");
        expected[it.key()] = it.value().toString();
    }
    QSet<QString> extracted;
    qint64 total = 0;
    if (format == "binary" || format == "pgn") {
        const auto target = engine ? entry["executable"].toString() : QString("games.pgn");
        require(nameSafe(target), "Nom de fichier invalide.");
        require(QFile::copy(file, temporary.path() + '/' + target),
                "Impossible de conserver le téléchargement.");
        extracted.insert(target);
    } else {
        require(format == "tar.gz" || format == "zip", "Format non pris en charge.");
        std::unique_ptr<struct archive, decltype(&archive_read_free)> archive(archive_read_new(),
                                                                              &archive_read_free);
        require(bool(archive), "Extraction indisponible.");
        if (format == "zip")
            archive_read_support_format_zip(archive.get());
        else {
            archive_read_support_filter_gzip(archive.get());
            archive_read_support_format_tar(archive.get());
        }
        require(archive_read_open_filename(archive.get(), QFile::encodeName(file).constData(), 65536) ==
                    ARCHIVE_OK,
                "Archive illisible.");
        struct archive_entry *item = nullptr;
        int status, count = 0;
        while ((status = archive_read_next_header(archive.get(), &item)) == ARCHIVE_OK) {
            require(!cancel->load(), "Téléchargement annulé.");
            require(++count <= 20000, "Archive contenant trop de fichiers.");
            const auto name = QString::fromUtf8(archive_entry_pathname(item));
            require(pathSafe(name), "Chemin dangereux dans l’archive.");
            require(!archive_entry_symlink(item) && !archive_entry_hardlink(item),
                    "Liens interdits dans l’archive.");
            if (archive_entry_filetype(item) == AE_IFDIR) {
                archive_read_data_skip(archive.get());
                continue;
            }
            require(archive_entry_filetype(item) == AE_IFREG, "Fichier spécial interdit dans l’archive.");
            const auto declared = archive_entry_size(item);
            require(declared >= 0 && declared <= 512LL * 1024 * 1024, "Fichier décompressé trop volumineux.");
            total += declared;
            require(total <= 512LL * 1024 * 1024, "Archive décompressée trop volumineuse.");
            QString target = expected.value(name);
            if (!engine && name.endsWith(".pgn", Qt::CaseInsensitive))
                target = "games.pgn";
            if (target.isEmpty()) {
                require(archive_read_data_skip(archive.get()) == ARCHIVE_OK, "Archive tronquée.");
                continue;
            }
            // Arasan contains the same LICENSE entry twice; only identical duplicates are accepted.
            const auto output = temporary.path() + '/' + target;
            const auto writing = extracted.contains(target) ? output + ".duplicate" : output;
            QFile out(writing);
            require(out.open(QIODevice::WriteOnly | QIODevice::Truncate), out.errorString());
            char bytes[65536];
            qint64 written = 0;
            la_ssize_t n;
            while ((n = archive_read_data(archive.get(), bytes, sizeof(bytes))) > 0) {
                require(!cancel->load(), "Téléchargement annulé.");
                written += n;
                require(written <= declared && written <= 512LL * 1024 * 1024,
                        "Taille décompressée incorrecte.");
                require(out.write(bytes, n) == n, out.errorString());
            }
            out.close();
            require(n == 0 && written == declared, "Archive tronquée ou taille incorrecte.");
            if (extracted.contains(target)) {
                require(engine && hashFile(writing, cancel) == hashFile(output, cancel),
                        "Fichier dupliqué ambigu dans l’archive.");
                require(QFile::remove(writing), "Impossible de terminer l’extraction.");
            }
            extracted.insert(target);
        }
        require(status == ARCHIVE_EOF, "Archive endommagée.");
        for (const auto &target : expected)
            require(extracted.contains(target), "Fichier attendu absent : " + target);
    }
    const auto target = engine ? entry["executable"].toString() : QString("games.pgn");
    require(extracted.contains(target), "Exécutable ou PGN absent de l’archive.");
    QFile payload(temporary.path() + '/' + target);
    require(payload.open(QIODevice::ReadOnly), payload.errorString());
    if (engine) {
        const auto header = payload.read(20);
        require(header.size() == 20 &&
                    header.startsWith(QByteArray("\x7f"
                                                 "ELF",
                                                 4)) &&
                    header[4] == 2 && header[5] == 1 && header[18] == 62 && header[19] == 0,
                "Ce fichier n’est pas un exécutable ELF Linux x86_64.");
    } else {
        const auto header = payload.read(4096).trimmed();
        require(!header.startsWith('<') &&
                    (header.contains("[Event ") || header.contains("[White ") || header.contains("1.")),
                "Le serveur n’a pas fourni de parties PGN.");
    }
    payload.close();
    for (const auto &name : extracted)
        require(QFile::setPermissions(temporary.path() + '/' + name, QFile::ReadOwner | QFile::WriteOwner),
                "Permissions non appliquées.");
    if (engine)
        require(QFile::setPermissions(temporary.path() + '/' + target,
                                      QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner),
                "Exécutable non installable.");
    QFile notice(temporary.path() + "/SOURCE.txt");
    require(notice.open(QIODevice::WriteOnly), notice.errorString());
    const auto attribution =
        (entry["title"].toString() + "\n" + entry["license"].toString() + "\n" + entry["source"].toString() +
         "\n" + entry["sourceCode"].toString() + "\nSHA-256: " + digest + "\n")
            .toUtf8();
    require(notice.write(attribution) == attribution.size(), notice.errorString());
    notice.close();
    if (engine && format == "binary") {
        QFile license(":/assets/content/PlentyChess-LICENSE.txt");
        if (license.exists())
            require(license.copy(temporary.path() + "/LICENSE"), "Licence non copiée.");
    }
    require(!cancel->load(), "Téléchargement annulé.");
    const auto folder = id + '-' + version + '-' + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto destination = root + '/' + folder;
    auto result = entry;
    result["sha256"] = digest;
    result["bytes"] = size;
    result["folder"] = folder;
    result["path"] = destination + '/' + target;
    require(QDir().rename(temporary.path(), destination), "Impossible de finaliser l’installation.");
    temporary.setAutoRemove(false);
    QSaveFile receipt(root + '/' + id + ".json");
    const auto json = QJsonDocument(QJsonObject::fromVariantMap(result)).toJson();
    if (!receipt.open(QIODevice::WriteOnly) || receipt.write(json) != json.size() || !receipt.commit()) {
        QDir(destination).removeRecursively();
        throw std::runtime_error("Impossible d’enregistrer l’installation.");
    }
    return result;
}
} // namespace t64
