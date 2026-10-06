// SPDX-License-Identifier: AGPL-3.0-or-later
#include "game.h"
#include <iostream>
#include <stdexcept>
int checks = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void rejects(F f) {
    bool failed = false;
    try {
        f();
    } catch (const std::exception &) {
        failed = true;
    }
    check(failed, "Invalid input accepted");
}
int main() {
    try {
        chess::Board board;
        check(t64::perft(board, 4) == 197281, "Startpos perft 4");
        board = t64::checkedBoard("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
        check(t64::perft(board, 3) == 97862, "Kiwipete perft 3");
        board = t64::checkedBoard("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
        check(t64::perft(board, 4) == 43238, "Endgame perft 4");
        t64::Game g;
        int a = g.play(0, "e2e4");
        int b = g.play(a, "e7e5");
        int c = g.play(a, "c7c5");
        check(g.nodes[a].children.size() == 2, "Variation created");
        check(g.play(a, "c7c5") == c, "Variation deduplicated");
        g.promote(c);
        check(g.mainline().back() == c, "Promote variation");
        g.removeVariation(b);
        check(g.nodes[a].children.size() == 1, "Delete branch");
        rejects([&] { g.play(0, "e2e5"); });
        const std::string text = R"([Event "Test \"quoted\""]
[Result "*"]
{Root comment} 1.e4! {centre} (1.d4 d5 (1...Nf6) 2.c4) e5 2.Nf3 (2.Bc4 $5) Nc6 *

[Event "Second"]
1. d4 d5 1/2-1/2)";
        auto games = t64::parsePgn(text);
        check(games.size() == 2, "Multiple PGN games");
        auto again = t64::parsePgn(games[0].pgn());
        check(again[0].pgn() == games[0].pgn(),
              "PGN round trip including nested branches, comments, NAG, tags");
        check(again[0].nodes[0].comment == "Root comment", "Root comment");
        check(again[0].nodes[1].nags[0] == 1, "Symbol annotation");
        rejects([] { t64::parsePgn("1. e4 (1. d4"); });
        rejects([] { t64::parsePgn("1. e4 {unclosed"); });
        rejects([] { t64::parsePgn("1. e5 *"); });
        rejects([] { t64::parsePgn("[Variant \"Atomic\"] 1.e4 *"); });
        rejects([] { t64::checkedBoard("8/8/8/8/8/8/8/8 w - - 0 1"); });
        rejects([] { t64::checkedBoard("8/8/8/8/8/8/4k3/4K3 w - - 0 1"); });
        rejects([] { t64::checkedBoard("4k3/8/8/8/8/8/8/4K3 w K - 0 1"); });
        rejects([] { t64::checkedBoard("4k3/8/8/8/8/8/8/4K3 w - e3 0 1"); });
        t64::Game promo("7k/P7/8/8/8/8/8/7K w - - 0 1");
        auto n = promo.play(0, "a7a8n");
        check(promo.nodes[n].san == "a8=N", "Underpromotion");
        t64::Game castle("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
        n = castle.play(0, "e1g1");
        check(castle.nodes[n].san == "O-O", "Kingside castling UCI");
        n = castle.play(0, "e1c1");
        check(castle.nodes[n].san == "O-O-O", "Queenside castling UCI");
        auto ep = t64::parsePgn("1.e4 a6 2.e5 d5 3.exd6 *");
        auto epBoard = ep[0].boardAt(5);
        check(epBoard.at(chess::Square("d6")) == chess::Piece::WHITEPAWN &&
                  epBoard.at(chess::Square("d5")) == chess::Piece::NONE,
              "En passant");
        t64::Game repeat;
        n = 0;
        for (int i = 0; i < 2; ++i)
            for (const auto &move : {"g1f3", "g8f6", "f3g1", "f6g8"})
                n = repeat.play(n, move);
        check(repeat.boardAt(n).isGameOver().first == chess::GameResultReason::THREEFOLD_REPETITION,
              "Repetition history retained");
        auto mate = t64::parsePgn("1.f3 e5 2.g4 Qh4# 0-1");
        check(mate[0].boardAt(4).isGameOver().first == chess::GameResultReason::CHECKMATE,
              "Checkmate detected");
        std::cout << checks << " core checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
