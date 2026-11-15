// SPDX-License-Identifier: AGPL-3.0-or-later
#include "syzygy.h"
#include "tbprobe.h"
#include <QDir>
#include <QStringList>
#include <algorithm>
#include <chess.hpp>
#include <mutex>

namespace t64::syzygy {
namespace {
// Fathom tient ses tables dans des variables globales et sa sonde de racine n'est pas
// réentrante : tout passe par ce verrou.
std::mutex lock;
QString opened;
Tables current;

// La sonde de racine : verdict et distance au prochain coup de pion ou à la prochaine
// prise, du camp au trait. Les tables ne connaissent pas le roque.
bool root(const chess::Board &board, unsigned &result) {
    if (!board.castlingRights().isEmpty())
        return false;
    using P = chess::PieceType;
    const auto bits = [&board](P type) { return board.pieces(type).getBits(); };
    const auto ep = board.enpassantSq();
    result = tb_probe_root(board.us(chess::Color::WHITE).getBits(), board.us(chess::Color::BLACK).getBits(),
                           bits(P::KING), bits(P::QUEEN), bits(P::ROOK), bits(P::BISHOP), bits(P::KNIGHT),
                           bits(P::PAWN), board.halfMoveClock(), 0,
                           ep == chess::Square::NO_SQ ? 0 : unsigned(ep.index()),
                           board.sideToMove() == chess::Color::WHITE, nullptr);
    return result != TB_RESULT_FAILED;
}

QString category(unsigned wdl) {
    switch (wdl) {
    case TB_WIN:
        return "win";
    case TB_CURSED_WIN:
        return "cursed-win";
    case TB_DRAW:
        return "draw";
    case TB_BLESSED_LOSS:
        return "blessed-loss";
    default:
        return "loss";
    }
}
} // namespace

Tables open(const QString &folder) {
    const std::lock_guard guard(lock);
    if (folder == opened)
        return current;
    // Les tables de Lichess se téléchargent par paquets, chacun dans son dossier
    // (« 3-4-5 », « 6-wdl »…) : on prend le dossier et ses sous-dossiers directs.
    QStringList paths;
    current = {};
    if (!folder.isEmpty())
        for (const auto &dir : QStringList{folder} + [&folder] {
                 QStringList inner;
                 for (const auto &name : QDir(folder).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
                     inner << QDir(folder).filePath(name);
                 return inner;
             }()) {
            const auto found = QDir(dir).entryList({"*.rtbw"}, QDir::Files).size();
            if (found > 0) {
                paths << QDir::toNativeSeparators(dir);
                current.files += int(found);
            }
        }
#ifdef _WIN32
    const auto joined = paths.join(';');
#else
    const auto joined = paths.join(':');
#endif
    opened = folder;
    if (!tb_init(joined.toLocal8Bit().constData()))
        current = {};
    else
        current.largest = current.files > 0 ? int(TB_LARGEST) : 0;
    return current;
}

QVariantMap probe(const QString &fen) {
    const std::lock_guard guard(lock);
    if (current.largest == 0)
        return {};
    chess::Board board;
    if (!board.setFen(fen.toStdString()) || board.occ().count() > current.largest)
        return {};
    unsigned top = 0;
    if (!root(board, top))
        return {};
    QVariantMap report{{"tablebase", true}, {"local", true}};
    if (top == TB_RESULT_CHECKMATE || top == TB_RESULT_STALEMATE) {
        report["verdict"] = top == TB_RESULT_CHECKMATE ? "loss" : "draw";
        report["dtz"] = 0;
        report["moves"] = QVariantList{};
        report["total"] = 0;
        return report;
    }
    report["verdict"] = category(TB_GET_WDL(top));
    report["dtz"] = int(TB_GET_DTZ(top));
    struct Row {
        QString san, uci;
        unsigned wdl; // du camp qui joue le coup
        int dtz;
        bool mate;
    };
    std::vector<Row> rows;
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    for (const auto &move : moves) {
        auto after = board;
        after.makeMove(move);
        unsigned child = 0;
        if (!root(after, child))
            return {}; // une table manque pour l'une des suites : Lichess répondra
        Row row{QString::fromStdString(chess::uci::moveToSan(board, move)),
                QString::fromStdString(chess::uci::moveToUci(move)), 0, 0, child == TB_RESULT_CHECKMATE};
        // Le verdict de la suite est dit pour l'adversaire : on le retourne.
        row.wdl = row.mate ? TB_WIN : child == TB_RESULT_STALEMATE ? TB_DRAW : TB_WIN - TB_GET_WDL(child);
        row.dtz = row.mate || child == TB_RESULT_STALEMATE ? 0 : int(TB_GET_DTZ(child));
        rows.push_back(row);
    }
    // Du meilleur au pire, comme Lichess : le mat d'abord, puis le gain le plus court, et
    // dans une position perdue la résistance la plus longue.
    std::stable_sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
        if (a.wdl != b.wdl)
            return a.wdl > b.wdl;
        if (a.mate != b.mate)
            return a.mate;
        return a.wdl > TB_DRAW ? a.dtz < b.dtz : a.wdl < TB_DRAW && a.dtz > b.dtz;
    });
    QVariantList list;
    for (const auto &row : rows)
        list << QVariantMap{
            {"san", row.san}, {"uci", row.uci}, {"verdict", category(row.wdl)}, {"dtz", row.dtz}};
    report["moves"] = list;
    report["total"] = list.size();
    return report;
}
} // namespace t64::syzygy
