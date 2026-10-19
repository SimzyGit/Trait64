// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "game.h"
#include "tactics.h"
namespace t64 {
struct MoveFact {
    std::string uci, san, fen, captureSquare;
    chess::PieceType captured = chess::PieceType::NONE, promotion = chess::PieceType::NONE;
    bool white = false, castle = false, enPassant = false, check = false, mate = false, stalemate = false;
    int material = 0; // White minus Black, conventional P=1, N/B=3, R=5, Q=9; kings excluded.
    std::vector<TacticalFact> tactics;
};
struct LineFacts {
    int initialMaterial = 0;
    std::vector<MoveFact> moves;
};
// Replay only legal moves, rejecting the entire line on error. Does not mutate the caller's board.
// Deliberately reports concrete board events, never the quality of a move or a player's rating.
LineFacts explainLine(chess::Board board, const std::vector<std::string> &moves);
} // namespace t64
