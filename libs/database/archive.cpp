// SPDX-License-Identifier: AGPL-3.0-or-later
#include "archive.h"
#include <QCryptographicHash>
#include <QFile>
#include <QLockFile>
#include <QUuid>
#include <QtEndian>
#include <algorithm>
#include <stdexcept>
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif
namespace t64 {
namespace {
constexpr qint64 block = 4096, maxPayload = 64 * 1024 * 1024;
const QByteArray magic("T64DB\r\n\x1a\n", 9);
void check(bool ok, const QString &message) {
    if (!ok)
        throw std::runtime_error(message.toStdString());
}
void put(QByteArray &b, int p, quint64 value) {
    qToLittleEndian<quint64>(value, b.data() + p);
}
quint64 get(const QByteArray &b, int p) {
    return qFromLittleEndian<quint64>(b.constData() + p);
}
QByteArray hash(const QByteArray &data) {
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}
void sync(QFile &file) {
    check(file.flush(), file.errorString());
#ifdef Q_OS_UNIX
    check(::fsync(file.handle()) == 0, "Échec de synchronisation de l’archive.");
#endif
}
struct Snapshot {
    quint64 generation = 0, offset = 0, size = 0;
    QByteArray data;
};
Snapshot inspect(QFile &file) {
    check(file.seek(0), file.errorString());
    const auto header = file.read(block);
    check(header.size() == block && header.first(9) == magic, "Archive T64 invalide.");
    check(get(header, 16) == 1, "Version de l’archive non prise en charge (profil expérimental 0.1 requis).");
    Snapshot best;
    for (const int slot : {128, 256}) {
        const auto d = header.mid(slot, 96);
        if (d.first(4) != "DIR1" || hash(d.first(64)) != d.mid(64, 32))
            continue;
        const auto gen = get(d, 8), off = get(d, 16), len = get(d, 24);
        if (!gen || off < block || off % block || len > maxPayload || off > quint64(file.size()) ||
            quint64(file.size()) - off < 64 + len)
            continue;
        if (!file.seek(qint64(off)))
            continue;
        const auto chunk = file.read(64);
        if (chunk.first(4) != "PGN1" || get(chunk, 8) != len || get(chunk, 16) != gen)
            continue;
        const auto data = file.read(qint64(len));
        if (quint64(data.size()) != len || hash(data) != chunk.mid(24, 32) ||
            hash(chunk + data) != d.mid(32, 32))
            continue;
        if (gen > best.generation)
            best = {gen, off, len, data};
    }
    check(best.generation > 0, "Aucune génération valide dans l’archive T64.");
    return best;
}
} // namespace
QByteArray Archive::read(const QString &path) {
    QFile file(path);
    check(file.open(QIODevice::ReadOnly), file.errorString());
    return inspect(file).data;
}
void Archive::append(const QString &path, const QByteArray &pgn) {
    check(pgn.size() <= maxPayload, "Une archive est limitée à 64 Mio de PGN par génération.");
    QLockFile lock(path + ".lock");
    check(lock.tryLock(0), "Cette archive est utilisée par un autre processus.");
    QFile file(path);
    check(file.open(QIODevice::ReadWrite), file.errorString());
    quint64 generation = 1;
    if (file.size() == 0) {
        QByteArray header(block, '\0');
        header.replace(0, 9, magic);
        put(header, 16, 1);
        header.replace(32, 16, QUuid::createUuid().toRfc4122());
        check(file.write(header) == header.size(), file.errorString());
        sync(file);
    } else
        generation = inspect(file).generation + 1;
    const auto offset = ((file.size() + block - 1) / block) * block;
    check(file.seek(file.size()), file.errorString());
    if (offset > file.size()) {
        const QByteArray padding(offset - file.size(), '\0');
        check(file.write(padding) == padding.size(), file.errorString());
    }
    QByteArray chunk(64, '\0');
    chunk.replace(0, 4, "PGN1");
    put(chunk, 8, pgn.size());
    put(chunk, 16, generation);
    chunk.replace(24, 32, hash(pgn));
    check(file.write(chunk) == chunk.size() && file.write(pgn) == pgn.size(), file.errorString());
    sync(file);
    QByteArray directory(96, '\0');
    directory.replace(0, 4, "DIR1");
    put(directory, 8, generation);
    put(directory, 16, offset);
    put(directory, 24, pgn.size());
    directory.replace(32, 32, hash(chunk + pgn));
    directory.replace(64, 32, hash(directory.first(64)));
    check(file.seek(generation % 2 ? 128 : 256), file.errorString());
    check(file.write(directory) == directory.size(), file.errorString());
    sync(file);
}
} // namespace t64
