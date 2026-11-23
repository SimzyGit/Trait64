// SPDX-License-Identifier: AGPL-3.0-or-later
#include "syncbundle.h"
#include <QFile>
#include <archive.h>
#include <archive_entry.h>
#include <memory>

namespace t64 {
QString writeSyncBundle(const QString &path, const QMap<QString, QByteArray> &files,
                        const QString &passphrase) {
    if (passphrase.size() < 8)
        return "La phrase de passe doit compter au moins huit caractères.";
    const auto temporary = path + ".part";
    {
        std::unique_ptr<struct archive, decltype(&archive_write_free)> writer(archive_write_new(),
                                                                              &archive_write_free);
        archive_write_set_format_zip(writer.get());
        if (archive_write_set_options(writer.get(), "zip:encryption=aes256") != ARCHIVE_OK ||
            archive_write_set_passphrase(writer.get(), passphrase.toUtf8().constData()) != ARCHIVE_OK)
            return "Chiffrement indisponible sur ce système.";
        if (archive_write_open_filename(writer.get(), QFile::encodeName(temporary).constData()) != ARCHIVE_OK)
            return "Dossier de synchronisation inaccessible en écriture.";
        for (auto it = files.cbegin(); it != files.cend(); ++it) {
            std::unique_ptr<struct archive_entry, decltype(&archive_entry_free)> entry(archive_entry_new(),
                                                                                       &archive_entry_free);
            archive_entry_set_pathname(entry.get(), it.key().toUtf8().constData());
            archive_entry_set_filetype(entry.get(), AE_IFREG);
            archive_entry_set_perm(entry.get(), 0600);
            archive_entry_set_size(entry.get(), it.value().size());
            if (archive_write_header(writer.get(), entry.get()) != ARCHIVE_OK ||
                archive_write_data(writer.get(), it.value().constData(), size_t(it.value().size())) !=
                    it.value().size())
                return QString::fromUtf8(archive_error_string(writer.get()));
        }
        if (archive_write_close(writer.get()) != ARCHIVE_OK)
            return QString::fromUtf8(archive_error_string(writer.get()));
    }
    QFile::remove(path);
    if (!QFile::rename(temporary, path)) {
        QFile::remove(temporary);
        return "Remplacement du paquet impossible.";
    }
    return {};
}

QString readSyncBundle(const QString &path, const QString &passphrase, QMap<QString, QByteArray> *files) {
    std::unique_ptr<struct archive, decltype(&archive_read_free)> reader(archive_read_new(),
                                                                         &archive_read_free);
    archive_read_support_format_zip(reader.get());
    archive_read_add_passphrase(reader.get(), passphrase.toUtf8().constData());
    if (archive_read_open_filename(reader.get(), QFile::encodeName(path).constData(), 1 << 16) != ARCHIVE_OK)
        return "Paquet de synchronisation illisible.";
    struct archive_entry *item = nullptr;
    qint64 total = 0;
    while (archive_read_next_header(reader.get(), &item) == ARCHIVE_OK) {
        const auto name = QString::fromUtf8(archive_entry_pathname(item));
        if (archive_entry_filetype(item) != AE_IFREG || name.contains("..") || name.startsWith('/')) {
            archive_read_data_skip(reader.get());
            continue;
        }
        QByteArray content;
        char bytes[1 << 16];
        la_ssize_t n;
        while ((n = archive_read_data(reader.get(), bytes, sizeof(bytes))) > 0) {
            content.append(bytes, n);
            total += n;
            if (total > 1024LL * 1024 * 1024)
                return "Paquet de synchronisation trop volumineux.";
        }
        if (n < 0)
            return QString::fromUtf8(archive_error_string(reader.get()))
                           .contains("passphrase", Qt::CaseInsensitive)
                       ? QString("Phrase de passe incorrecte.")
                       : QString("Paquet de synchronisation abîmé.");
        files->insert(name, content);
    }
    return {};
}
} // namespace t64
