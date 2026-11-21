// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "game.h"
#include <optional>
namespace t64 {
enum class TacticalKind {
    Fork,
    AbsolutePin,
    KingSkewer,
    DiscoveredCheck,
    DoubleCheck,
    // Une pièce du camp qui vient de jouer peut être prise avec gain : `attackers` sont
    // les pièces adverses qui la prennent, `targets` la pièce elle-même.
    Hanging,
    // Une pièce à longue portée restée en place menace, par la ligne que le coup a
    // ouverte, une pièce qu'elle gagnerait.
    DiscoveredAttack,
    // Si l'adversaire passait son tour, `attackers` donnerait mat en un coup.
    MateThreat,
    // Surcharge : `attackers` est la seule pièce à défendre plusieurs pièces attaquées de son
    // camp (`targets`) ; elle ne peut pas toutes les reprendre, et la dévier en gagne une.
    Overloaded
};
struct TacticalPiece {
    std::string square;
    chess::PieceType type;
    bool white = false;
    bool operator==(const TacticalPiece &) const = default;
};
struct TacticalFact {
    TacticalKind kind;
    std::vector<TacticalPiece> attackers, targets;
    bool operator==(const TacticalFact &) const = default;
};
// Immediately before/after one legal move. New/changed patterns, not forced wins.
// Non-king fork targets exclude captures that would expose the attacker's king.
std::vector<TacticalFact> explainTactics(const chess::Board &before, const chess::Board &after);
// Ce que rapporte, en points (pion 1, cavalier et fou 3, tour 5, dame 9), la prise de
// `target` par la pièce en `from` une fois tous les échanges joués sur cette case, chaque
// camp reprenant par sa pièce la moins chère et s'arrêtant quand cela lui coûte. Les
// pièces alignées derrière comptent ; les clouages sont ignorés.
int exchangeValue(const chess::Board &board, chess::Square from, chess::Square target);
// Les pièces de `side` (roi exclu) qu'une prise adverse gagnerait, échanges joués : ce
// que ChessBase appelle les relations d'attaque, réduites à ce qui compte. Le camp au trait
// n'importe pas : une pièce en prise l'est aussi pour celui qui doit jouer.
std::vector<chess::Square> hangingPieces(const chess::Board &board, chess::Color side);
// Ce que le coup offre, en points, échanges joués : la perte d'une prise qui coûte plus
// qu'elle ne rapporte, ou ce que l'adversaire gagnerait en prenant la pièce qui vient de
// se poser. Zéro si le coup n'offre rien. Avec une évaluation qui ne baisse pas, c'est un
// sacrifice.
int offeredMaterial(const chess::Board &before, const chess::Move &move);
// La meilleure prise du camp au trait, zéro s'il n'en a aucune qui rapporte.
int bestCaptureGain(const chess::Board &board);
// PGN graphical annotations on the resulting position. No arrows through blockers.
std::string tacticalAnnotations(const std::vector<TacticalFact> &facts);
// Les coups du camp au trait qui forcent le mat en `moves` coups au plus (de 1 à 3), quelle
// que soit la défense. `budget` borne le nombre de positions examinées : au-delà, rien
// n'est sûr et la réponse est `std::nullopt`.
std::optional<std::vector<chess::Move>> forcedMates(const chess::Board &board, int moves,
                                                    long budget = 400000);
} // namespace t64
