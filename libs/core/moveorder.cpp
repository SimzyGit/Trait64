// SPDX-License-Identifier: AGPL-3.0-or-later
#include "moveorder.h"
#include <algorithm>
#include <array>

namespace t64 {
namespace {
// Des groupes espacés : le roi change de place selon l'échec, et les intervalles vides
// évitent d'avoir à renuméroter tout le reste pour autant.
enum Group {
    EnPassant = 0,
    KingInCheck = 1,
    PawnCaptureWest = 10,
    PawnCaptureWestPromotion = 11,
    PawnCaptureEast = 12,
    PawnCaptureEastPromotion = 13,
    PawnPush = 14,
    PawnPushPromotion = 15,
    PawnDouble = 16,
    Knight = 20,
    Bishop = 21,
    Rook = 22,
    Queen = 23,
    King = 30,
    CastleKingSide = 40,
    CastleQueenSide = 41,
};
// Dame, tour, fou, cavalier : l'ordre dans lequel Shakmaty empile les promotions.
int promotionRank(chess::PieceType piece) {
    switch (piece.internal()) {
    case chess::PieceType::underlying::QUEEN:
        return 0;
    case chess::PieceType::underlying::ROOK:
        return 1;
    case chess::PieceType::underlying::BISHOP:
        return 2;
    default:
        return 3;
    }
}
struct Key {
    int group = 0, from = 0, to = 0, promotion = 0;
    bool operator<(const Key &other) const {
        return std::tie(group, from, to, promotion) <
               std::tie(other.group, other.from, other.to, other.promotion);
    }
};
Key keyOf(const chess::Board &board, chess::Move move, bool check) {
    const int from = move.from().index(), to = move.to().index();
    if (move.typeOf() == chess::Move::ENPASSANT)
        // Shakmaty parcourt les pions qui peuvent prendre, pas leur case d'arrivée :
        // elle est la même pour les deux.
        return {EnPassant, from, to, 0};
    if (move.typeOf() == chess::Move::CASTLING)
        // La bibliothèque note le roque « roi prend tour » : c'est la tour qui dit le côté.
        return {to > board.kingSq(board.sideToMove()).index() ? CastleKingSide : CastleQueenSide, 0, 0, 0};
    const auto piece = board.at<chess::PieceType>(move.from());
    if (piece == chess::PieceType::PAWN) {
        const bool promotion = move.typeOf() == chess::Move::PROMOTION;
        const int rank = promotion ? promotionRank(move.promotionType()) : 0;
        // Shakmaty prend d'abord vers l'ouest, puis vers l'est. Pour les Blancs, l'ouest
        // monte d'une case vers la gauche (+7) ; pour les Noirs il descend (−9), et non
        // l'inverse : le miroir porte sur la direction, pas sur l'écart.
        const int delta = to - from, white = board.sideToMove() == chess::Color::WHITE ? 1 : 0;
        if (delta == (white ? 7 : -9))
            return {promotion ? PawnCaptureWestPromotion : PawnCaptureWest, 0, to, rank};
        if (delta == (white ? 9 : -7))
            return {promotion ? PawnCaptureEastPromotion : PawnCaptureEast, 0, to, rank};
        if (delta == 16 || delta == -16)
            return {PawnDouble, 0, to, 0};
        return {promotion ? PawnPushPromotion : PawnPush, 0, to, rank};
    }
    if (piece == chess::PieceType::KING)
        return {check ? KingInCheck : King, 0, to, 0};
    const int group = piece == chess::PieceType::KNIGHT   ? Knight
                      : piece == chess::PieceType::BISHOP ? Bishop
                      : piece == chess::PieceType::ROOK   ? Rook
                                                          : Queen;
    return {group, from, to, 0};
}
} // namespace

std::vector<chess::Move> movesInShakmatyOrder(const chess::Board &board) {
    chess::Movelist legal;
    chess::movegen::legalmoves(legal, board);
    const bool check = board.inCheck();
    std::vector<std::pair<Key, chess::Move>> ordered;
    ordered.reserve(legal.size());
    for (const auto &move : legal)
        ordered.emplace_back(keyOf(board, move, check), move);
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const auto &a, const auto &b) { return a.first < b.first; });
    std::vector<chess::Move> moves;
    moves.reserve(ordered.size());
    for (const auto &[key, move] : ordered)
        moves.push_back(move);
    return moves;
}
} // namespace t64
