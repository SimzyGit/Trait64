// SPDX-License-Identifier: AGPL-3.0-or-later
#include "explanation.h"
#include <algorithm>
#include <stdexcept>
namespace t64 {
namespace {
int material(const chess::Board &board) {
    const int values[]{1, 3, 3, 5, 9, 0, 0};
    int balance = 0;
    for (int i = 0; i < 64; ++i) {
        const auto piece = board.at(chess::Square(i));
        if (piece != chess::Piece::NONE)
            balance += values[int(piece.type())] * (piece.color() == chess::Color::WHITE ? 1 : -1);
    }
    return balance;
}
// Le matériel une fois la dernière prise rendue : une suite de moteur peut s'arrêter au
// milieu d'un échange, et compter là donnerait une pièce d'avance à qui vient de prendre.
int settled(const LineFacts &line, size_t plies) {
    if (plies == 0)
        return line.initialMaterial;
    const auto &last = line.moves[plies - 1];
    if (last.mate || last.stalemate)
        return last.material;
    const auto board = checkedBoard(last.fen, line.chess960);
    const int gain = bestCaptureGain(board);
    return last.material + (board.sideToMove() == chess::Color::WHITE ? gain : -gain);
}
int pieceValue(chess::PieceType type) {
    const int values[]{1, 3, 3, 5, 9, 0, 0};
    return values[int(type)];
}
} // namespace
Mistake explainMistake(const LineFacts &played, const LineFacts &best) {
    Mistake result;
    if (played.moves.empty() || best.moves.empty())
        return result;
    const bool white = played.moves.front().white;
    const auto collect = [&](const LineFacts &line, bool side, size_t plies) {
        for (size_t i = 0; i < plies; ++i)
            if (line.moves[i].white == side)
                for (const auto &fact : line.moves[i].tactics)
                    if (fact.kind != TacticalKind::Hanging &&
                        std::find(result.motifs.begin(), result.motifs.end(), fact.kind) ==
                            result.motifs.end())
                        result.motifs.push_back(fact.kind);
    };
    if (played.moves.back().mate && played.moves.back().white != white) {
        result.reason = MistakeReason::AllowsMate;
        result.mateIn = int(played.moves.size() / 2);
        result.proof = played.moves.size() > 1 ? played.moves[1].san : std::string();
        collect(played, !white, played.moves.size());
        return result;
    }
    if (best.moves.back().mate && best.moves.back().white == white) {
        result.reason = MistakeReason::MissesMate;
        result.mateIn = int((best.moves.size() + 1) / 2);
        result.proof = best.moves.front().san;
        collect(best, white, best.moves.size());
        return result;
    }
    // Même profondeur des deux côtés : un gain au vingtième coup d'une suite ne se compare
    // pas à une autre qui s'arrête au quatrième.
    const size_t plies = std::min(played.moves.size(), best.moves.size());
    const int sign = white ? 1 : -1;
    const int after = sign * settled(played, plies), wanted = sign * settled(best, plies);
    const int start = sign * played.initialMaterial;
    if (wanted - after < 1)
        return result;
    result.material = wanted - after;
    // La prise la plus lourde de la suite, du côté qui en sort gagnant.
    const auto heaviest = [&](const LineFacts &line, bool side) {
        const MoveFact *found = nullptr;
        for (size_t i = 0; i < plies; ++i) {
            const auto &move = line.moves[i];
            if (move.white == side && move.captured != chess::PieceType::NONE &&
                (!found || pieceValue(move.captured) > pieceValue(found->captured)))
                found = &move;
        }
        return found;
    };
    if (after < start) {
        result.reason = MistakeReason::LosesMaterial;
        if (const auto *capture = heaviest(played, !white)) {
            result.proof = capture->san;
            result.piece = capture->captured;
            result.square = capture->captureSquare;
            for (const auto &fact : played.moves.front().tactics)
                if (fact.kind == TacticalKind::Hanging && fact.targets.front().square == result.square)
                    result.leftHanging = true;
        }
        collect(played, !white, plies);
    } else {
        result.reason = MistakeReason::MissesMaterial;
        result.proof = best.moves.front().san;
        if (const auto *capture = heaviest(best, white)) {
            result.piece = capture->captured;
            result.square = capture->captureSquare;
        }
        collect(best, white, plies);
    }
    return result;
}
LineFacts explainLine(chess::Board board, const std::vector<std::string> &moves) {
    if (moves.size() > 128)
        throw std::runtime_error("Une explication est limitée à 128 demi-coups.");
    LineFacts result;
    result.initialMaterial = material(board);
    result.chess960 = board.chess960();
    for (const auto &uci : moves) {
        chess::Movelist legal;
        chess::movegen::legalmoves(legal, board);
        auto found = std::find_if(legal.begin(), legal.end(), [&](const auto &move) {
            return chess::uci::moveToUci(move, board.chess960()) == uci;
        });
        if (found == legal.end())
            throw std::runtime_error("Suite moteur illégale : " + uci);
        const auto move = *found;
        MoveFact fact;
        fact.uci = uci;
        fact.san = chess::uci::moveToSan(board, move);
        fact.white = board.sideToMove() == chess::Color::WHITE;
        fact.castle = move.typeOf() == chess::Move::CASTLING;
        fact.enPassant = move.typeOf() == chess::Move::ENPASSANT;
        fact.captured = board.getCapturing<chess::PieceType>(move);
        if (fact.captured != chess::PieceType::NONE)
            fact.captureSquare = std::string(fact.enPassant ? move.to().ep_square() : move.to());
        if (move.typeOf() == chess::Move::PROMOTION)
            fact.promotion = move.promotionType();
        const auto before = board;
        board.makeMove(move);
        checkBoardCounters(board);
        fact.tactics = explainTactics(before, board);
        fact.fen = board.getFen();
        fact.material = material(board);
        fact.check = board.inCheck();
        chess::movegen::legalmoves(legal, board);
        fact.mate = fact.check && legal.empty();
        fact.stalemate = !fact.check && legal.empty();
        result.moves.push_back(std::move(fact));
    }
    return result;
}
} // namespace t64
