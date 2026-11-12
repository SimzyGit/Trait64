// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "explanation.h"
#include <algorithm>
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
    check(t64::tacticalAnnotations(knight.tactics) == "[%cal Gd6e8,Gd6f7]",
          "Check fork draws both actual attacks");
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
              motif(pin, Kind::AbsolutePin)->targets[1].square == "e8",
          "Pin keeps front piece and king in order");
    check(t64::tacticalAnnotations(pin.tactics) == "[%csl Ye1,Ye7,Ye8]",
          "Pin marks alignment without arrow through blocker");
    check(has(one("3k4/4n3/8/8/8/8/8/K1B5 w - - 0 1", "c1g5"), Kind::AbsolutePin), "Bishop diagonal pin");
    const auto mobile =
        t64::explainLine(t64::checkedBoard("4k3/4r3/8/8/8/8/8/K1R5 w - - 0 1"), {"c1e1", "e7e6"});
    check(has(mobile.moves[0], Kind::AbsolutePin) && mobile.moves.size() == 2,
          "Pinned rook can move on its pin line");
    check(!has(one("4k3/4n3/4P3/8/8/8/8/K1R5 w - - 0 1", "c1e1"), Kind::AbsolutePin),
          "Friendly interposer prevents pin");
    check(!has(one("4k3/4n3/4p3/8/8/8/8/K1R5 w - - 0 1", "c1e1"), Kind::AbsolutePin),
          "Two enemy blockers prevent pin");
    check(!has(one("4q2k/4n3/8/8/8/8/8/K1R5 w - - 0 1", "c1e1"), Kind::AbsolutePin),
          "Queen alignment is not a king pin");
    const auto skewer = one("4q3/8/8/4k3/8/8/8/K1R5 w - - 0 1", "c1e1");
    check(has(skewer, Kind::KingSkewer) && skewer.check &&
              motif(skewer, Kind::KingSkewer)->targets[0].square == "e5",
          "Skewer puts checked king before rear piece");
    check(t64::tacticalAnnotations(skewer.tactics) == "[%csl Ye1,Ye5,Ye8]", "Skewer marks ordered alignment");
    const auto discovered = one("4k3/8/8/8/8/8/4B3/K3R3 w - - 0 1", "e2f3");
    check(has(discovered, Kind::DiscoveredCheck) && !has(discovered, Kind::DoubleCheck),
          "Single discovered check");
    const auto doubleCheck = one("4k3/8/8/8/8/8/4B3/K3R3 w - - 0 1", "e2b5");
    check(has(doubleCheck, Kind::DiscoveredCheck) && has(doubleCheck, Kind::DoubleCheck),
          "Double discovered check");
    check(t64::tacticalAnnotations(doubleCheck.tactics) == "[%cal Gb5e8,Ge1e8]",
          "Duplicate check arrows deduplicated");
    check(!has(one("4k3/8/8/8/8/8/8/K1R5 w - - 0 1", "c1e1"), Kind::DiscoveredCheck),
          "Direct rook check is not discovered");
    const auto ep = one("8/8/8/R2pP2k/8/8/8/K7 w - d6 0 1", "e5d6");
    check(ep.enPassant && has(ep, Kind::DiscoveredCheck), "En passant removes both rank blockers");
    const auto castle = one("5k2/8/8/8/8/8/8/6KR w H - 0 1", "g1h1", true);
    check(castle.castle && castle.check && !has(castle, Kind::DiscoveredCheck),
          "Chess960 castling rook direct check");
    check(!has(one("4k3/4n3/8/8/8/8/P7/K3R3 w - - 0 1", "a2a3"), Kind::AbsolutePin),
          "Unchanged pin is not repeated");
    const auto promotion = one("8/4P1r1/3k4/8/8/8/8/K7 w - - 0 1", "e7e8n");
    check(has(promotion, Kind::Fork) &&
              motif(promotion, Kind::Fork)->attackers[0].type == chess::PieceType::KNIGHT,
          "Underpromotion attacks d6 and g7 from e8");
    const auto quiet = t64::explainLine(chess::Board(), {"e2e4"}).moves[0];
    check(quiet.promotion == chess::PieceType::NONE && quiet.tactics.empty() &&
              t64::tacticalAnnotations(quiet.tactics).empty(),
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
            if (legal.empty())
                break;
            random = random * 1664525u + 1013904223u;
            const auto move = legal[random % legal.size()];
            const auto before = position;
            position.makeMove(move);
            if (position.inCheck())
                continue;
            auto hypothetical = position;
            hypothetical.makeNullMove();
            chess::movegen::legalmoves(legal, hypothetical);
            for (const auto &fact : t64::explainTactics(before, position)) {
                if (fact.kind != Kind::Fork)
                    continue;
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
    // Échanges : la valeur d'une prise une fois toutes les reprises jouées.
    const auto exchange = [](const char *fen, const char *from, const char *to) {
        return t64::exchangeValue(t64::checkedBoard(fen), chess::Square(from), chess::Square(to));
    };
    check(exchange("4k3/8/8/3p4/8/8/3R4/4K3 w - - 0 1", "d2", "d5") == 1,
          "An undefended pawn is worth a pawn");
    check(exchange("4k3/8/4p3/3p4/8/8/3R4/4K3 w - - 0 1", "d2", "d5") == -4,
          "A rook for a defended pawn loses four");
    check(exchange("3rk3/8/8/3p4/8/8/3R4/3RK3 w - - 0 1", "d2", "d5") == 1,
          "The rook behind the rook counts: pawn won after both recaptures");
    check(exchange("4k3/8/2n5/4p3/8/5N2/8/4K3 w - - 0 1", "f3", "e5") == -2,
          "Knight takes a pawn defended by a knight: two points lost");
    check(t64::bestCaptureGain(t64::checkedBoard("4k3/8/8/3q4/4P3/8/8/4K3 w - - 0 1")) == 9,
          "The best capture of the side to move is found");
    check(t64::bestCaptureGain(chess::Board()) == 0, "No capture, no gain");

    // Pièce laissée en prise, attaque à la découverte, menace de mat.
    const auto hung = one("4k3/8/8/3p4/8/5N2/8/4K3 w - - 0 1", "f3e5");
    check(!has(hung, Kind::Hanging), "A piece nobody can take is not hanging");
    const auto dropped = one("4k3/8/3p4/8/8/5N2/8/4K3 w - - 0 1", "f3e5");
    check(has(dropped, Kind::Hanging) && motif(dropped, Kind::Hanging)->targets[0].square == "e5" &&
              motif(dropped, Kind::Hanging)->attackers[0].square == "d6",
          "A knight put where a pawn takes it is hanging, taker and victim named");
    check(t64::tacticalAnnotations(dropped.tactics) == "[%cal Rd6e5]", "The taker's capture is drawn in red");
    check(has(one("4k3/8/3p4/8/3P4/5N2/8/4K3 w - - 0 1", "f3e5"), Kind::Hanging),
          "A defended knight attacked by a pawn still loses two points: it hangs");
    check(!has(one("4k3/8/2n5/8/5P2/5N2/8/4K3 w - - 0 1", "f3e5"), Kind::Hanging),
          "A knight defended against a knight does not hang");
    check(!has(one("3rk3/8/8/8/8/3N4/8/3RK3 w - - 0 1", "d3f4"), Kind::DiscoveredAttack),
          "Unveiling an attack on a defended rook of equal value wins nothing: not reported");
    const auto opened = one("3qk3/8/8/8/8/3N4/8/3RK3 w - - 0 1", "d3f4");
    check(has(opened, Kind::DiscoveredAttack) &&
              motif(opened, Kind::DiscoveredAttack)->attackers[0].square == "d1" &&
              motif(opened, Kind::DiscoveredAttack)->targets[0].square == "d8",
          "Moving the knight unveils the rook's attack on the queen");
    const auto threat = one("6k1/5ppp/8/8/8/8/4K3/7R w - - 0 1", "h1a1");
    check(has(threat, Kind::MateThreat) && motif(threat, Kind::MateThreat)->attackers[0].square == "a1" &&
              motif(threat, Kind::MateThreat)->targets[0].square == "g8",
          "A rook swung to the open file threatens the back-rank mate");
    check(!has(one("6k1/5pp1/7p/8/8/8/4K3/7R w - - 0 1", "h1a1"), Kind::MateThreat),
          "With an escape square there is no mate threat");

    // Le verdict : pourquoi le coup joué vaut moins que celui du moteur.
    const auto line = [](const char *fen, std::vector<std::string> moves) {
        return t64::explainLine(t64::checkedBoard(fen), moves);
    };
    using Reason = t64::MistakeReason;
    const char *knightEnd = "4k3/8/3p4/8/8/5N2/8/4K3 w - - 0 1";
    const auto loses =
        t64::explainMistake(line(knightEnd, {"f3e5", "d6e5"}), line(knightEnd, {"f3d4", "e8d7"}));
    check(loses.reason == Reason::LosesMaterial && loses.material == 3 && loses.proof == "dxe5" &&
              loses.piece == chess::PieceType::KNIGHT && loses.square == "e5" && loses.leftHanging,
          "Putting the knight en prise: three points lost, the capture and the piece are named");
    const char *backRank = "6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1";
    const auto missedMate = t64::explainMistake(line(backRank, {"h2h3", "h7h6"}), line(backRank, {"a1a8"}));
    check(missedMate.reason == Reason::MissesMate && missedMate.mateIn == 1 && missedMate.proof == "Ra8#",
          "A missed mate in one is told with its move");
    const char *danger = "r5k1/5ppp/8/8/8/8/5PPP/6K1 w - - 0 1";
    const auto allowed =
        t64::explainMistake(line(danger, {"g1h1", "a8a1"}), line(danger, {"h2h3", "a8a1", "g1h2"}));
    check(allowed.reason == Reason::AllowsMate && allowed.mateIn == 1 && allowed.proof == "Ra1#",
          "Allowing a mate in one is told with the mating move");
    const char *chance = "4k3/8/8/3q4/8/2N5/8/4K3 w - - 0 1";
    const auto missed = t64::explainMistake(line(chance, {"e1e2", "d5d8"}), line(chance, {"c3d5", "e8d7"}));
    check(missed.reason == Reason::MissesMaterial && missed.material == 9 && missed.proof == "Nxd5" &&
              missed.piece == chess::PieceType::QUEEN && missed.square == "d5",
          "A queen left untaken is a missed gain, with the capture that won it");
    const auto even =
        t64::explainMistake(line(knightEnd, {"f3d2", "e8d7"}), line(knightEnd, {"f3d4", "e8d7"}));
    check(even.reason == Reason::Positional && even.material == 0,
          "No material difference: the gap is positional");
    // Une suite qui s'arrête au milieu d'un échange ne crédite pas la pièce qui vient de prendre.
    const char *trade = "4k3/8/2n5/4p3/8/5N2/8/4K3 w - - 0 1";
    const auto greedy = t64::explainMistake(line(trade, {"f3e5"}), line(trade, {"f3d2"}));
    check(greedy.reason == Reason::LosesMaterial && greedy.material == 2,
          "A line cut in mid-exchange is settled: knight for pawn costs two");
}
