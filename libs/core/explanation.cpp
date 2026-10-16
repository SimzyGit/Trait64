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
}
LineFacts explainLine(chess::Board board, const std::vector<std::string> &moves) {
    if (moves.size() > 128)
        throw std::runtime_error("Une explication est limitée à 128 demi-coups.");
    LineFacts result;
    result.initialMaterial = material(board);
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
        board.makeMove(move);
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
