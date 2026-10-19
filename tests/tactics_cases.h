// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <algorithm>
#include "explanation.h"
// Shares the core test runner's check() function.
void tacticalChecks() {
    const auto motif = [](const t64::MoveFact &move, t64::TacticalKind kind) {
        return std::find_if(move.tactics.begin(), move.tactics.end(),
                            [=](const auto &fact) { return fact.kind == kind; });
    };
    const auto has = [&](const t64::MoveFact &move, t64::TacticalKind kind) {
        return motif(move, kind) != move.tactics.end();
    };
    const auto one = [](const char *fen, const char *uci, bool variant960 = false) {
        return t64::explainLine(t64::checkedBoard(fen, variant960), {uci}).moves.front();
    };
    using Kind = t64::TacticalKind;
    const auto knight = one("4k3/5q2/8/8/2N5/8/8/4K3 w - - 0 1", "c4d6");
    check(has(knight, Kind::Fork), "Knight check fork detected");
    const auto fork = *motif(knight, Kind::Fork);
    check(fork.attackers[0].square == "d6" && fork.attackers[0].type == chess::PieceType::KNIGHT &&
              fork.targets.size() == 2 && fork.targets[0].square == "f7" && fork.targets[1].square == "e8",
          "Fork identifies actual attacker, queen and king squares");
    check(t64::tacticalAnnotations(knight.tactics) == "[%cal Gd6e8,Gd6f7]", "Check fork draws both actual attacks");
    const auto black = one("4k3/8/8/2n5/8/8/5Q2/4K3 b - - 0 1", "c5d3");
    check(has(black, Kind::Fork) && !motif(black, Kind::Fork)->attackers[0].white,
          "Black forks keep correct squares and color");
    check(has(one("7k/8/3n1r2/8/4P3/8/8/7K w - - 0 1", "e4e5"), Kind::Fork), "White pawn fork");
    check(has(one("7k/8/8/4p3/8/3N1R2/8/7K b - - 0 1", "e5e4"), Kind::Fork), "Black pawn fork");
    check(!has(one("4r2k/8/8/8/8/2n4q/4R3/4K3 w - - 0 1", "e2e3"), Kind::Fork),
          "Pinned rook cannot capture sideways");
    check(has(one("4r2k/8/8/8/8/2n1R2q/8/4K3 w - - 0 1", "e1f2"), Kind::Fork),
          "Unpinning an unmoved rook creates newly possible captures");
    check(!has(one("3r3k/8/8/8/8/8/4Pq2/K2R4 w - - 0 1", "d1d2"), Kind::Fork),
          "Friendly blocker prevents a slider attack");
    const auto pin = one("4k3/4n3/8/8/8/8/8/K1R5 w - - 0 1", "c1e1");
    check(has(pin, Kind::AbsolutePin) && motif(pin, Kind::AbsolutePin)->targets[0].square == "e7" &&
              motif(pin, Kind::AbsolutePin)->targets[1].square == "e8", "Pin keeps front piece and king in order");
    check(t64::tacticalAnnotations(pin.tactics) == "[%csl Ye1,Ye7,Ye8]", "Pin marks alignment without arrow through blocker");
    check(has(one("3k4/4n3/8/8/8/8/8/K1B5 w - - 0 1", "c1g5"), Kind::AbsolutePin), "Bishop diagonal pin");
    const auto mobile = t64::explainLine(t64::checkedBoard("4k3/4r3/8/8/8/8/8/K1R5 w - - 0 1"), {"c1e1", "e7e6"});
    check(has(mobile.moves[0], Kind::AbsolutePin) && mobile.moves.size() == 2, "Pinned rook can move on its pin line");
    check(!has(one("4k3/4n3/4P3/8/8/8/8/K1R5 w - - 0 1", "c1e1"), Kind::AbsolutePin), "Friendly interposer prevents pin");
    check(!has(one("4k3/4n3/4p3/8/8/8/8/K1R5 w - - 0 1", "c1e1"), Kind::AbsolutePin), "Two enemy blockers prevent pin");
    check(!has(one("4q2k/4n3/8/8/8/8/8/K1R5 w - - 0 1", "c1e1"), Kind::AbsolutePin), "Queen alignment is not a king pin");
    const auto skewer = one("4q3/8/8/4k3/8/8/8/K1R5 w - - 0 1", "c1e1");
    check(has(skewer, Kind::KingSkewer) && skewer.check && motif(skewer, Kind::KingSkewer)->targets[0].square == "e5",
          "Skewer puts checked king before rear piece");
    check(t64::tacticalAnnotations(skewer.tactics) == "[%csl Ye1,Ye5,Ye8]", "Skewer marks ordered alignment");
    const auto discovered = one("4k3/8/8/8/8/8/4B3/K3R3 w - - 0 1", "e2f3");
    check(has(discovered, Kind::DiscoveredCheck) && !has(discovered, Kind::DoubleCheck), "Single discovered check");
    const auto doubleCheck = one("4k3/8/8/8/8/8/4B3/K3R3 w - - 0 1", "e2b5");
    check(has(doubleCheck, Kind::DiscoveredCheck) && has(doubleCheck, Kind::DoubleCheck), "Double discovered check");
    check(t64::tacticalAnnotations(doubleCheck.tactics) == "[%cal Gb5e8,Ge1e8]", "Duplicate check arrows deduplicated");
    check(!has(one("4k3/8/8/8/8/8/8/K1R5 w - - 0 1", "c1e1"), Kind::DiscoveredCheck), "Direct rook check is not discovered");
    const auto ep = one("8/8/8/R2pP2k/8/8/8/K7 w - d6 0 1", "e5d6");
    check(ep.enPassant && has(ep, Kind::DiscoveredCheck), "En passant removes both rank blockers");
    const auto castle = one("5k2/8/8/8/8/8/8/6KR w H - 0 1", "g1h1", true);
    check(castle.castle && castle.check && !has(castle, Kind::DiscoveredCheck), "Chess960 castling rook direct check");
    check(!has(one("4k3/4n3/8/8/8/8/P7/K3R3 w - - 0 1", "a2a3"), Kind::AbsolutePin), "Unchanged pin is not repeated");
    const auto promotion = one("8/4P1r1/3k4/8/8/8/8/K7 w - - 0 1", "e7e8n");
    check(has(promotion, Kind::Fork) && motif(promotion, Kind::Fork)->attackers[0].type == chess::PieceType::KNIGHT,
          "Underpromotion attacks d6 and g7 from e8");
    const auto quiet = t64::explainLine(chess::Board(), {"e2e4"}).moves[0];
    check(quiet.promotion == chess::PieceType::NONE && quiet.tactics.empty() && t64::tacticalAnnotations(quiet.tactics).empty(),
          "Quiet opening move has no promotion or invented motif");
    t64::Game annotated("4k3/5q2/8/8/2N5/8/8/4K3 w - - 0 1");
    const int node = annotated.play(0, "c4d6");
    annotated.nodes[node].comment = "Fourchette. " + t64::tacticalAnnotations(knight.tactics);
    check(t64::parsePgn(annotated.pgn())[0].nodes[1].comment == annotated.nodes[node].comment,
          "Tactical PGN graphical annotations survive round trip");
    unsigned random = 20261017;
    int verified = 0;
    for (int game = 0; game < 12; ++game) {
        chess::Board position;
        for (int ply = 0; ply < 100; ++ply) {
            chess::Movelist legal;
            chess::movegen::legalmoves(legal, position);
            if (legal.empty()) break;
            random = random * 1664525u + 1013904223u;
            const auto move = legal[random % legal.size()];
            const auto before = position;
            position.makeMove(move);
            if (position.inCheck()) continue;
            auto hypothetical = position;
            hypothetical.makeNullMove();
            chess::movegen::legalmoves(legal, hypothetical);
            for (const auto &fact : t64::explainTactics(before, position)) {
                if (fact.kind != Kind::Fork) continue;
                for (const auto &target : fact.targets) {
                    const bool found = std::any_of(legal.begin(), legal.end(), [&](const auto &capture) {
                        return std::string(capture.from()) == fact.attackers[0].square &&
                               std::string(capture.to()) == target.square;
                    });
                    check(found, "Independent legal generator confirms hypothetical fork capture");
                    ++verified;
                }
            }
        }
    }
    check(verified > 100, "Random legality comparison exercises many capture targets");
}
