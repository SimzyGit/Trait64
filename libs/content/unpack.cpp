// SPDX-License-Identifier: AGPL-3.0-or-later
#include "unpack.h"
#include <QFile>
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

QByteArray unpackIfCompressed(const QString &path, qint64 limit) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), file.errorString());
    const auto head = file.peek(6);
    const bool packed = head.startsWith("\x1f\x8b") ||                   // gzip
                        head.startsWith("\x28\xb5\x2f\xfd") ||           // zstd
                        head.startsWith("BZh") ||                        // bzip2
                        head.startsWith(QByteArray("\xfd", 1) + "7zXZ"); // xz
    if (!packed) {
        require(file.size() <= limit, "Fichier trop volumineux.");
        const auto bytes = file.read(limit + 1);
        require(file.error() == QFileDevice::NoError, file.errorString());
        require(bytes.size() <= limit, "Fichier trop volumineux.");
        return bytes;
    }
    file.close();
    std::unique_ptr<struct archive, decltype(&archive_read_free)> reader(archive_read_new(),
                                                                         &archive_read_free);
    require(bool(reader), "Décompression indisponible.");
    archive_read_support_filter_all(reader.get());
    // « raw » : le fichier est un flux compressé nu, sans entrées ni arborescence.
    archive_read_support_format_raw(reader.get());
    require(archive_read_open_filename(reader.get(), QFile::encodeName(path).constData(), 65536) ==
                ARCHIVE_OK,
            "Fichier compressé illisible.");
    struct archive_entry *entry = nullptr;
    require(archive_read_next_header(reader.get(), &entry) == ARCHIVE_OK, "Fichier compressé vide.");
    QByteArray bytes;
    char buffer[65536];
    for (;;) {
        const auto read = archive_read_data(reader.get(), buffer, sizeof(buffer));
        require(read >= 0, "Fichier compressé endommagé.");
        if (read == 0)
            return bytes;
        bytes.append(buffer, int(read));
        require(bytes.size() <= limit, "Fichier trop volumineux une fois déplié.");
    }
}
} // namespace t64

namespace t64 {
QString extractPgnFromZip(const QString &zip, const QString &target) {
    std::unique_ptr<struct archive, decltype(&archive_read_free)> reader(archive_read_new(),
                                                                         &archive_read_free);
    if (!reader)
        return "Extraction indisponible.";
    archive_read_support_format_zip(reader.get());
    if (archive_read_open_filename(reader.get(), QFile::encodeName(zip).constData(), 1 << 16) != ARCHIVE_OK)
        return "Archive illisible.";
    struct archive_entry *item = nullptr;
    while (archive_read_next_header(reader.get(), &item) == ARCHIVE_OK) {
        const auto name = QString::fromUtf8(archive_entry_pathname(item));
        if (archive_entry_filetype(item) != AE_IFREG || !name.endsWith(".pgn", Qt::CaseInsensitive)) {
            archive_read_data_skip(reader.get());
            continue;
        }
        QFile out(target);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return out.errorString();
        char bytes[1 << 16];
        la_ssize_t n;
        qint64 written = 0;
        // Une base élite mensuelle tient quelques centaines de mégaoctets : on borne à 8 Go.
        while ((n = archive_read_data(reader.get(), bytes, sizeof(bytes))) > 0) {
            written += n;
            if (written > 8LL * 1024 * 1024 * 1024 || out.write(bytes, n) != n)
                return "Fichier décompressé trop volumineux ou disque plein.";
        }
        return n == 0 ? QString() : QString("Archive tronquée.");
    }
    return "Aucun fichier PGN dans l’archive.";
}
} // namespace t64
