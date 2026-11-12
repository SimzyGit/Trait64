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
    bool chess960 = false;
    std::vector<MoveFact> moves;
};
// Pourquoi un coup est moins bon que celui du moteur, d'après les deux suites calculées.
// Rien n'est affirmé qui ne se lise sur l'échiquier en rejouant ces suites.
enum class MistakeReason {
    AllowsMate,     // la suite jouée finit par un mat contre celui qui a joué
    MissesMate,     // la suite du moteur matait
    LosesMaterial,  // la suite jouée coûte du matériel
    MissesMaterial, // la suite du moteur en gagnait
    Positional      // aucune différence de matériel dans les suites
};
struct Mistake {
    MistakeReason reason = MistakeReason::Positional;
    int mateIn = 0;   // coups du camp qui mate
    int material = 0; // points d'écart entre les deux suites, vus du joueur (toujours >= 0)
    // Le coup qui le montre : la prise adverse, ou le premier coup de la suite du moteur.
    std::string proof;
    // La pièce perdue ou gagnée, et sa case au moment de la prise.
    chess::PieceType piece = chess::PieceType::NONE;
    std::string square;
    // Le coup joué laissait lui-même cette pièce en prise.
    bool leftHanging = false;
    // Motifs relevés dans la suite qui fait la différence, du côté qui en profite.
    std::vector<TacticalKind> motifs;
};
// `played` commence par le coup joué, `best` par celui du moteur, depuis la même position.
Mistake explainMistake(const LineFacts &played, const LineFacts &best);
// Replay only legal moves, rejecting the entire line on error. Does not mutate the caller's board.
// Deliberately reports concrete board events, never the quality of a move or a player's rating.
LineFacts explainLine(chess::Board board, const std::vector<std::string> &moves);
} // namespace t64
