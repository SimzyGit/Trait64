// SPDX-License-Identifier: AGPL-3.0-or-later
#include "puzzles.h"
#include "chess.hpp"
#include <QStringList>
#include <archive.h>
#include <stdexcept>

namespace t64 {
bool parseLichessPuzzle(const QString &line, PuzzleRecord &out) {
    // PuzzleId,FEN,Moves,Rating,RatingDeviation,Popularity,NbPlays,Themes,GameUrl,OpeningTags
    const auto fields = line.split(',');
    if (fields.size() < 8 || fields[0] == "PuzzleId") return false;
    bool ok = false;
    const int rating = fields[3].toInt(&ok);
    if (!ok) return false;
    const auto moves = fields[2].split(' ', Qt::SkipEmptyParts);
    if (moves.size() < 2 || moves.size() > 40) return false;
    try {
        chess::Board board(fields[1].toStdString());
        QString solverFen;
        for (int i = 0; i < moves.size(); ++i) {
            chess::Movelist legal;
            chess::movegen::legalmoves(legal, board);
            chess::Move move = chess::Move::NO_MOVE;
            for (const auto &candidate : legal)
                if (QString::fromStdString(chess::uci::moveToUci(candidate)) == moves[i]) move = candidate;
            if (move == chess::Move::NO_MOVE) return false;
            board.makeMove(move);
            if (i == 0) solverFen = QString::fromStdString(board.getFen());
        }
        out.ref = "lichess:" + fields[0];
        out.fen = solverFen;
        out.solution = QStringList(moves.mid(1)).join(' ');
        out.rating = rating;
        out.popularity = fields[5].toInt();
        out.themes = fields[7];
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

int readLichessPuzzles(const QString &path, int max, int minRating, int maxRating,
                       const std::function<void(std::vector<PuzzleRecord> &&)> &batch,
                       const std::shared_ptr<std::atomic_bool> &canceled) {
    // libarchive's raw format handles plain files and every compression filter alike.
    std::unique_ptr<archive, decltype(&archive_read_free)> reader(archive_read_new(), archive_read_free);
    archive_read_support_filter_all(reader.get());
    archive_read_support_format_raw(reader.get());
    if (archive_read_open_filename(reader.get(), path.toLocal8Bit().constData(), 1 << 16) != ARCHIVE_OK)
        throw std::runtime_error("Fichier de puzzles illisible : " + std::string(archive_error_string(reader.get())));
    archive_entry *entry = nullptr;
    if (archive_read_next_header(reader.get(), &entry) != ARCHIVE_OK)
        throw std::runtime_error("Fichier de puzzles vide ou compression inconnue.");
    std::vector<PuzzleRecord> pending;
    QByteArray buffer, chunk(1 << 16, Qt::Uninitialized);
    int delivered = 0;
    auto flush = [&] {
        if (pending.empty()) return;
        delivered += int(pending.size());
        batch(std::move(pending));
        pending.clear();
    };
    auto line = [&](const QByteArray &text) {
        PuzzleRecord record;
        if (!parseLichessPuzzle(QString::fromUtf8(text.trimmed()), record)) return;
        if (record.rating < minRating || (maxRating > 0 && record.rating > maxRating)) return;
        pending.push_back(std::move(record));
        if (pending.size() >= 2000) flush();
    };
    while (delivered + int(pending.size()) < max) {
        if (canceled && canceled->load()) throw std::runtime_error("Import de puzzles annulé.");
        const auto n = archive_read_data(reader.get(), chunk.data(), chunk.size());
        if (n < 0) throw std::runtime_error("Lecture interrompue : " + std::string(archive_error_string(reader.get())));
        if (n == 0) break;
        buffer.append(chunk.constData(), n);
        qsizetype start = 0;
        for (qsizetype end; (end = buffer.indexOf('\n', start)) >= 0 && delivered + int(pending.size()) < max;
             start = end + 1)
            line(buffer.mid(start, end - start));
        buffer.remove(0, start);
        if (buffer.size() > 1 << 20) throw std::runtime_error("Ligne de puzzle trop longue : fichier invalide.");
    }
    if (!buffer.isEmpty() && delivered + int(pending.size()) < max) line(buffer);
    flush();
    return delivered;
}
} // namespace t64
