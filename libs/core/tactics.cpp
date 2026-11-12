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
    case int(PieceType::PAWN):
        return attacks::pawn(piece.color(), square);
    case int(PieceType::KNIGHT):
        return attacks::knight(square);
    case int(PieceType::BISHOP):
        return attacks::bishop(square, board.occ());
    case int(PieceType::ROOK):
        return attacks::rook(square, board.occ());
    case int(PieceType::QUEEN):
        return attacks::queen(square, board.occ());
    default:
        return {};
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
int worth(PieceType type) {
    constexpr int values[]{1, 3, 3, 5, 9, 100, 0};
    return values[int(type)];
}
Bitboard attackersTo(const Board &board, Square square, Bitboard occupied) {
    const auto diagonal = board.pieces(PieceType::BISHOP) | board.pieces(PieceType::QUEEN);
    const auto straight = board.pieces(PieceType::ROOK) | board.pieces(PieceType::QUEEN);
    return ((attacks::pawn(Color::BLACK, square) & board.pieces(PieceType::PAWN, Color::WHITE)) |
            (attacks::pawn(Color::WHITE, square) & board.pieces(PieceType::PAWN, Color::BLACK)) |
            (attacks::knight(square) & board.pieces(PieceType::KNIGHT)) |
            (attacks::king(square) & board.pieces(PieceType::KING)) |
            (attacks::bishop(square, occupied) & diagonal) | (attacks::rook(square, occupied) & straight)) &
           occupied;
}
// Les pièces du camp `us` qu'une prise adverse gagnerait, avec celles qui les prennent.
std::vector<TacticalFact> liabilities(const Board &board, Color us) {
    std::vector<TacticalFact> result;
    auto pieces = board.us(us) & ~board.pieces(PieceType::KING, us);
    while (pieces) {
        const Square target(pieces.pop());
        TacticalFact fact{TacticalKind::Hanging, {}, {describe(board, target)}};
        auto takers = attackersTo(board, target, board.occ()) & board.us(~us);
        while (takers) {
            const Square from(takers.pop());
            const bool king = board.at(from).type() == PieceType::KING;
            if ((king || captureKeepsKingSafe(board, from, target)) && exchangeValue(board, from, target) > 0)
                fact.attackers.push_back(describe(board, from));
        }
        if (!fact.attackers.empty())
            result.push_back(std::move(fact));
    }
    return result;
}
// Les coups du camp `us`, supposé au trait, qui matent tout de suite.
std::vector<TacticalFact> matingMoves(Board board, Color us) {
    std::vector<TacticalFact> result;
    if (board.sideToMove() != us) {
        if (board.inCheck())
            return result;
        board.makeNullMove();
    }
    Movelist moves, replies;
    movegen::legalmoves(moves, board);
    for (const auto &move : moves) {
        const auto piece = describe(board, move.from());
        board.makeMove(move);
        movegen::legalmoves(replies, board);
        if (board.inCheck() && replies.empty()) {
            TacticalFact fact{TacticalKind::MateThreat, {piece}, {describe(board, board.kingSq(~us))}};
            // Une dame qui mate de deux façons ne fait qu'une menace.
            if (std::find(result.begin(), result.end(), fact) == result.end())
                result.push_back(std::move(fact));
        }
        board.unmakeMove(move);
    }
    return result;
}
std::vector<TacticalFact> patterns(const Board &board, Color us) {
    std::vector<TacticalFact> result;
    auto pieces = board.us(us);
    constexpr std::array<std::array<int, 2>, 8> directions{
        {{{1, 0}}, {{-1, 0}}, {{0, 1}}, {{0, -1}}, {{1, 1}}, {{1, -1}}, {{-1, 1}}, {{-1, -1}}}};
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
            for (int x = from.index() % 8 + dx, y = from.index() / 8 + dy; x >= 0 && x < 8 && y >= 0 && y < 8;
                 x += dx, y += dy) {
                const Square at(y * 8 + x);
                if (board.at(at) == Piece::NONE)
                    continue;
                if (board.at(at).color() == us)
                    break;
                hit.push_back(at);
                if (hit.size() == 2)
                    break;
            }
            if (hit.size() != 2)
                continue;
            const auto front = board.at(hit[0]).type(), rear = board.at(hit[1]).type();
            if (rear == PieceType::KING || front == PieceType::KING)
                result.push_back(
                    {rear == PieceType::KING ? TacticalKind::AbsolutePin : TacticalKind::KingSkewer,
                     {describe(board, from)},
                     {describe(board, hit[0]), describe(board, hit[1])}});
        }
    }
    return result;
}
} // namespace
int exchangeValue(const chess::Board &board, chess::Square from, chess::Square target) {
    int gain[32];
    int depth = 0;
    gain[0] = board.at(target) == Piece::NONE ? 0 : worth(board.at(target).type());
    auto occupied = board.occ() & ~Bitboard::fromSquare(from);
    int standing = worth(board.at(from).type()); // ce que vaut la pièce posée sur la case
    auto side = ~board.at(from).color();
    while (depth < 31) {
        auto takers = attackersTo(board, target, occupied) & board.us(side);
        if (!takers)
            break;
        Square cheapest(takers.lsb());
        while (takers) {
            const Square candidate(takers.pop());
            if (worth(board.at(candidate).type()) < worth(board.at(cheapest).type()))
                cheapest = candidate;
        }
        ++depth;
        gain[depth] = standing - gain[depth - 1];
        standing = worth(board.at(cheapest).type());
        occupied &= ~Bitboard::fromSquare(cheapest);
        side = ~side;
    }
    while (depth > 0) {
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
        --depth;
    }
    return gain[0];
}
int bestCaptureGain(const chess::Board &board) {
    int best = 0;
    Movelist moves;
    movegen::legalmoves(moves, board);
    for (const auto &move : moves)
        if (move.typeOf() != Move::CASTLING && board.at(move.to()) != Piece::NONE)
            best = std::max(best, exchangeValue(board, move.from(), move.to()));
    return best;
}
std::vector<TacticalFact> explainTactics(const chess::Board &before, const chess::Board &after) {
    const auto us = before.sideToMove();
    auto result = patterns(after, us);
    for (auto &&fact : liabilities(after, us))
        result.push_back(std::move(fact));
    for (auto &&fact : matingMoves(after, us))
        result.push_back(std::move(fact));
    auto previous = patterns(before, us);
    for (auto &&fact : liabilities(before, us))
        previous.push_back(std::move(fact));
    for (auto &&fact : matingMoves(before, us))
        previous.push_back(std::move(fact));
    std::erase_if(result, [&](const auto &fact) {
        return std::find(previous.begin(), previous.end(), fact) != previous.end();
    });
    // Une pièce à longue portée qui n'a pas bougé et atteint, par la ligne ouverte, une
    // pièce qu'elle gagnerait : attaque à la découverte. L'échec, lui, est traité plus bas.
    auto sliders = after.us(us);
    while (sliders) {
        const chess::Square from(sliders.pop());
        if (!slider(after.at(from).type()) || before.at(from) != after.at(from))
            continue;
        auto fresh = attacksFrom(after, from) & after.us(~us) & ~attacksFrom(before, from) &
                     ~after.pieces(chess::PieceType::KING);
        while (fresh) {
            const chess::Square target(fresh.pop());
            if (before.at(target) == after.at(target) && exchangeValue(after, from, target) > 0)
                result.push_back(
                    {TacticalKind::DiscoveredAttack, {describe(after, from)}, {describe(after, target)}});
        }
    }
    const auto king = after.kingSq(~us);
    auto checkers = chess::attacks::attackers(after, us, king);
    TacticalFact doubleCheck{TacticalKind::DoubleCheck, {}, {describe(after, king)}};
    while (checkers) {
        const chess::Square from(checkers.pop());
        doubleCheck.attackers.push_back(describe(after, from));
        if (slider(after.at(from).type()) && before.at(from) == after.at(from) &&
            !attacksFrom(before, from).check(king.index()))
            result.push_back(
                {TacticalKind::DiscoveredCheck, {describe(after, from)}, {describe(after, king)}});
    }
    if (doubleCheck.attackers.size() >= 2)
        result.push_back(std::move(doubleCheck));
    return result;
}
std::string tacticalAnnotations(const std::vector<TacticalFact> &facts) {
    std::set<std::string> arrows, squares;
    for (const auto &fact : facts) {
        if (fact.kind == TacticalKind::Hanging) {
            // En rouge : ce sont les prises de l'adversaire, pas les nôtres.
            for (const auto &a : fact.attackers)
                for (const auto &t : fact.targets)
                    arrows.insert("R" + a.square + t.square);
        } else if (fact.kind == TacticalKind::MateThreat) {
            for (const auto &p : fact.targets)
                squares.insert("R" + p.square);
        } else if (fact.kind == TacticalKind::AbsolutePin || fact.kind == TacticalKind::KingSkewer) {
            // Alignment marks do not imply an attack through the front piece.
            for (const auto &p : fact.attackers)
                squares.insert("Y" + p.square);
            for (const auto &p : fact.targets)
                squares.insert("Y" + p.square);
        } else {
            for (const auto &a : fact.attackers)
                for (const auto &t : fact.targets)
                    arrows.insert("G" + a.square + t.square);
        }
    }
    const auto directive = [](const char *name, const std::set<std::string> &items) {
        if (items.empty())
            return std::string();
        std::string result = std::string("[%") + name + " ";
        for (const auto &item : items) {
            if (result.back() != ' ')
                result += ',';
            result += item;
        }
        return result + "]";
    };
    const auto a = directive("cal", arrows), s = directive("csl", squares);
    return a + (!a.empty() && !s.empty() ? " " : "") + s;
}
} // namespace t64
