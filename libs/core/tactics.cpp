// SPDX-License-Identifier: AGPL-3.0-or-later
#include "tactics.h"
#include <algorithm>
#include <array>
#include <set>
namespace t64 {
namespace {
using namespace chess;
TacticalPiece describe(const Board &board, Square square) {
    const auto piece = board.at(square);
    return {std::string(square), piece.type(), piece.color() == Color::WHITE};
}
Bitboard attacksFrom(const Board &board, Square square) {
    const auto piece = board.at(square);
    switch (int(piece.type())) {
    case int(PieceType::PAWN): return attacks::pawn(piece.color(), square);
    case int(PieceType::KNIGHT): return attacks::knight(square);
    case int(PieceType::BISHOP): return attacks::bishop(square, board.occ());
    case int(PieceType::ROOK): return attacks::rook(square, board.occ());
    case int(PieceType::QUEEN): return attacks::queen(square, board.occ());
    default: return {};
    }
}
bool slider(PieceType type) {
    return type == PieceType::BISHOP || type == PieceType::ROOK || type == PieceType::QUEEN;
}
// Hypothetical ordinary capture without an intervening reply. Recompute attacks
// on our unchanged king, removing the captured enemy from every attacker set.
bool captureKeepsKingSafe(const Board &board, Square from, Square target) {
    const auto us = board.at(from).color(), them = ~us;
    const auto king = board.kingSq(us);
    const auto remaining = ~Bitboard::fromSquare(target);
    const auto occupied = (board.occ() & ~Bitboard::fromSquare(from)) | Bitboard::fromSquare(target);
    const auto enemies = [&](PieceType type) { return board.pieces(type, them) & remaining; };
    return !(attacks::pawn(us, king) & enemies(PieceType::PAWN)) &&
           !(attacks::knight(king) & enemies(PieceType::KNIGHT)) &&
           !(attacks::king(king) & enemies(PieceType::KING)) &&
           !(attacks::bishop(king, occupied) & (enemies(PieceType::BISHOP) | enemies(PieceType::QUEEN))) &&
           !(attacks::rook(king, occupied) & (enemies(PieceType::ROOK) | enemies(PieceType::QUEEN)));
}
std::vector<TacticalFact> patterns(const Board &board, Color us) {
    std::vector<TacticalFact> result;
    auto pieces = board.us(us);
    constexpr std::array<std::array<int, 2>, 8> directions{{
        {{1, 0}}, {{-1, 0}}, {{0, 1}}, {{0, -1}}, {{1, 1}}, {{1, -1}}, {{-1, 1}}, {{-1, -1}}
    }};
    while (pieces) {
        const Square from(pieces.pop());
        const auto type = board.at(from).type();
        TacticalFact fork{TacticalKind::Fork, {describe(board, from)}, {}};
        auto targets = attacksFrom(board, from) & board.us(~us);
        while (targets) {
            const Square target(targets.pop());
            // King targets represent a check, never a possible king capture.
            if (board.at(target).type() == PieceType::KING || captureKeepsKingSafe(board, from, target))
                fork.targets.push_back(describe(board, target));
        }
        if (fork.targets.size() >= 2)
            result.push_back(std::move(fork));
        if (!slider(type))
            continue;
        for (const auto &direction : directions) {
            const auto dx = direction[0], dy = direction[1];
            if ((type == PieceType::BISHOP && (dx == 0 || dy == 0)) ||
                (type == PieceType::ROOK && dx != 0 && dy != 0))
                continue;
            std::vector<Square> hit;
            for (int x = from.index() % 8 + dx, y = from.index() / 8 + dy;
                 x >= 0 && x < 8 && y >= 0 && y < 8; x += dx, y += dy) {
                const Square at(y * 8 + x);
                if (board.at(at) == Piece::NONE) continue;
                if (board.at(at).color() == us) break;
                hit.push_back(at);
                if (hit.size() == 2) break;
            }
            if (hit.size() != 2) continue;
            const auto front = board.at(hit[0]).type(), rear = board.at(hit[1]).type();
            if (rear == PieceType::KING || front == PieceType::KING)
                result.push_back({rear == PieceType::KING ? TacticalKind::AbsolutePin : TacticalKind::KingSkewer,
                                  {describe(board, from)}, {describe(board, hit[0]), describe(board, hit[1])}});
        }
    }
    return result;
}
} // namespace
std::vector<TacticalFact> explainTactics(const chess::Board &before, const chess::Board &after) {
    const auto us = before.sideToMove();
    auto result = patterns(after, us);
    const auto previous = patterns(before, us);
    std::erase_if(result, [&](const auto &fact) {
        return std::find(previous.begin(), previous.end(), fact) != previous.end();
    });
    const auto king = after.kingSq(~us);
    auto checkers = chess::attacks::attackers(after, us, king);
    TacticalFact doubleCheck{TacticalKind::DoubleCheck, {}, {describe(after, king)}};
    while (checkers) {
        const chess::Square from(checkers.pop());
        doubleCheck.attackers.push_back(describe(after, from));
        if (slider(after.at(from).type()) && before.at(from) == after.at(from) &&
            !attacksFrom(before, from).check(king.index()))
            result.push_back({TacticalKind::DiscoveredCheck, {describe(after, from)}, {describe(after, king)}});
    }
    if (doubleCheck.attackers.size() >= 2) result.push_back(std::move(doubleCheck));
    return result;
}
std::string tacticalAnnotations(const std::vector<TacticalFact> &facts) {
    std::set<std::string> arrows, squares;
    for (const auto &fact : facts) {
        if (fact.kind == TacticalKind::AbsolutePin || fact.kind == TacticalKind::KingSkewer) {
            // Alignment marks do not imply an attack through the front piece.
            for (const auto &p : fact.attackers) squares.insert("Y" + p.square);
            for (const auto &p : fact.targets) squares.insert("Y" + p.square);
        } else {
            for (const auto &a : fact.attackers)
                for (const auto &t : fact.targets) arrows.insert("G" + a.square + t.square);
        }
    }
    const auto directive = [](const char *name, const std::set<std::string> &items) {
        if (items.empty()) return std::string();
        std::string result = std::string("[%") + name + " ";
        for (const auto &item : items) { if (result.back() != ' ') result += ','; result += item; }
        return result + "]";
    };
    const auto a = directive("cal", arrows), s = directive("csl", squares);
    return a + (!a.empty() && !s.empty() ? " " : "") + s;
}
} // namespace t64
