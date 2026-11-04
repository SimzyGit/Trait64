// SPDX-License-Identifier: AGPL-3.0-or-later
#include "game.h"
#include "accuracy.h"
#include "explanation.h"
#include "polyglot.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
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
#include "tactics_cases.h"
int main() {
    try {
        check(t64::gameResult(t64::checkedBoard("7k/5K2/6Q1/8/8/8/8/8 b - - 100 1")).first ==
                  chess::GameResultReason::STALEMATE,
              "Stalemate takes precedence over a claimable fifty-move draw");
        check(t64::gameResult(t64::checkedBoard("7k/8/8/8/8/8/8/K7 w - - 100 1")).first ==
                  chess::GameResultReason::INSUFFICIENT_MATERIAL,
              "Insufficient material remains a forced ending after fifty moves");
        check(t64::gameResult(t64::checkedBoard("4k3/8/8/8/8/8/8/R3K3 w - - 100 1")).first ==
                  chess::GameResultReason::FIFTY_MOVE_RULE,
              "A position with legal moves and material only offers a draw claim");
        auto extreme = t64::checkedBoard("4k3/8/8/8/8/8/8/R3K3 w - - 255 1");
        const auto quiet = chess::uci::uciToMove(extreme, "a1a2");
        extreme.makeMove(quiet);
        check(extreme.halfMoveClock() == 256 && extreme.isHalfMoveDraw(),
              "Halfmove counter cannot wrap to zero after a quiet move");
        extreme.unmakeMove(quiet);
        check(extreme.halfMoveClock() == 255, "Undo restores the complete halfmove counter");
        for (const auto &[fen, move] : std::vector<std::pair<std::string, std::string>>{
                 {"4k3/8/8/8/8/8/8/R3K3 w - - 255 1", "a1a2"},
                 {"4k3/8/8/8/8/8/8/R3K3 b - - 0 32000", "e8e7"}}) {
            t64::Game bounded(fen);
            const auto original = bounded.pgn();
            rejects([&] { bounded.play(0, move); });
            check(bounded.pgn() == original && bounded.nodes.size() == 1,
                  "Counter limit rejection cannot append an unreadable position");
            rejects([&] { t64::explainLine(bounded.boardAt(0), {move}); });
        }
        tacticalChecks();
        const chess::Board initial;
        const auto exchange = t64::explainLine(initial, {"e2e4", "d7d5", "e4d5", "d8d5"});
        check(exchange.initialMaterial == 0 && exchange.moves.back().material == 0,
              "Equal pawn exchange is not described as a material gain");
        check(exchange.moves[2].captured == chess::PieceType::PAWN &&
                  exchange.moves[2].captureSquare == "d5" && exchange.moves[2].material == 1 &&
                  exchange.moves[3].material == 0 && initial.getFen() == chess::constants::STARTPOS,
              "Capture facts, both colors and caller board preserved");
        check(exchange.moves[0].captured == chess::PieceType::NONE && !exchange.moves[0].check,
              "Quiet move has no invented capture or check");
        auto enPassant = t64::parsePgn("1.e4 a6 2.e5 d5 *").front();
        const auto epFacts = t64::explainLine(enPassant.boardAt(4), {"e5d6"}).moves.front();
        check(epFacts.enPassant && epFacts.captureSquare == "d5" && epFacts.material == 1,
              "En passant explains the removed pawn's actual square");
        const auto promotionFacts = t64::explainLine(
            t64::checkedBoard("1r5k/P7/8/8/8/8/8/7K w - - 0 1"), {"a7b8q"});
        check(promotionFacts.initialMaterial == -4 && promotionFacts.moves[0].material == 9 &&
                  promotionFacts.moves[0].captured == chess::PieceType::ROOK &&
                  promotionFacts.moves[0].promotion == chess::PieceType::QUEEN && promotionFacts.moves[0].check,
              "Capture promotion counts the captured rook and replacement of the pawn");
        const auto under = t64::explainLine(t64::checkedBoard("7k/P7/8/8/8/8/8/7K w - - 0 1"), {"a7a8n"});
        check(under.moves[0].promotion == chess::PieceType::KNIGHT && under.moves[0].material == 3,
              "Underpromotion uses the actual piece, not a queen");
        const auto blackPromotion = t64::explainLine(
            t64::checkedBoard("7K/8/8/8/8/8/p7/7k b - - 0 1"), {"a2a1q"});
        check(!blackPromotion.moves[0].white && blackPromotion.moves[0].material == -9,
              "Black promotion keeps White-minus-Black material orientation");
        const auto castleFacts = t64::explainLine(
            t64::checkedBoard("k7/8/8/8/8/8/8/6KR w H - 0 1", true), {"g1h1"});
        check(castleFacts.moves[0].castle && castleFacts.moves[0].captured == chess::PieceType::NONE &&
                  castleFacts.moves[0].material == 5 && castleFacts.moves[0].san == "O-O",
              "Stationary Chess960 king castling is not a rook capture");
        const auto mateFacts = t64::explainLine(initial, {"f2f3", "e7e5", "g2g4", "d8h4"});
        check(mateFacts.moves.back().mate && mateFacts.moves.back().check &&
                  !mateFacts.moves.back().stalemate && mateFacts.moves.back().material == 0,
              "Checkmate is a board fact, independent from material balance");
        const auto pat = t64::explainLine(t64::checkedBoard("7k/5K2/8/6Q1/8/8/8/8 w - - 0 1"), {"g5g6"});
        check(pat.moves.back().stalemate && !pat.moves.back().check && !pat.moves.back().mate,
              "Stalemate is distinguished from checkmate");
        rejects([&] { t64::explainLine(initial, {"e2e4", "e7e4"}); });
        rejects([&] { t64::explainLine(initial, {"0000"}); });
        rejects([&] { t64::explainLine(initial, {"f2f3", "e7e5", "g2g4", "d8h4", "a2a3"}); });
        rejects([&] { t64::explainLine(initial, std::vector<std::string>(129, "g1f3")); });
        check(t64::explainLine(initial, {}).moves.empty(), "Empty line contains no manufactured explanation");
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
        // Real CC0 Lichess January 2013 record, https://lichess.org/9opx3qh7.
        auto historical =
            t64::parsePgn("1. d4 e5 2. dxe5 d6 3. exd6 Bxd6 4. Nf3 Nf6 5. Nc3 O-O 6. a3 Nc6 7. e3 a6 "
                          "8. Be2 h6 9. O-O Ne5 10. Bd2 Nxf3+ 11. Bxf3 Be5 12. Rc1 c6 13. Qe2 Qd6 "
                          "14. Rfd1 Bxh2+ 15. Kh1 Be5 16. e4 Bxc3 17. Bxc3 Qe6 18. Rd3 Bd7 19. Rcd1 Rad8 "
                          "20. Bxf6 gxf6 21. Rd6 Qe7 22. Rd1d2 *")
                .at(0);
        check(historical.nodes.back().uci == "d1d2" && historical.nodes.back().san == "R1d2",
              "Historical full-origin SAN is normalized to canonical SAN");
        const auto parent = historical.nodes.back().parent;
        const auto unchanged = historical.pgn();
        for (const auto &bad : {"Rd2", "Rdd2", "Rd3d2", "Rd1xd2", "Rd1-d2", "Rdd1d2"})
            rejects([&] { historical.playSan(parent, bad); });
        check(historical.pgn() == unchanged, "Rejected ambiguous/invalid SAN never changes the study");
        check(t64::parsePgn("1.Ng1f3 d5 2.Nf3g5 *")[0].nodes.back().uci == "f3g5",
              "Redundant origin works for legal non-capturing piece moves");
        auto capture = t64::parsePgn("1.e4 d5 2.exd5 Qd8xd5 *").at(0);
        check(capture.nodes.back().san == "Qxd5", "Redundant capture origin is normalized");
        rejects([] { t64::parsePgn("1.e4 d5 2.exd5 Qd8d5 *"); });
        rejects([] { t64::parsePgn("1.Nb1f3 *"); });
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
        std::set<std::string> starts;
        for (int i = 0; i < 960; ++i) {
            const auto fen = t64::chess960Fen(i);
            starts.insert(fen);
            t64::Game frc(fen, true);
            auto b960 = frc.boardAt(0);
            check(t64::perft(b960, 1) >= 18, "Chess960 legal initial moves");
            check(t64::parsePgn(frc.pgn()).at(0).chess960, "Chess960 PGN variant preserved");
        }
        check(starts.size() == 960, "960 distinct starts");
        check(t64::chess960Fen(518).starts_with("rnbqkbnr/"), "Scharnagl 518 is classical");
        t64::Game stationary("k7/8/8/8/8/8/8/6KR w H - 0 1", true);
        int castled = stationary.play(0, "g1h1");
        auto cb = stationary.boardAt(castled);
        check(cb.at(chess::Square("g1")) == chess::Piece::WHITEKING &&
                  cb.at(chess::Square("f1")) == chess::Piece::WHITEROOK,
              "Chess960 stationary king castling");
        check(stationary.nodes[castled].san == "O-O", "960 castle SAN");
        check(t64::parsePgn(stationary.pgn()).at(0).pgn() == stationary.pgn(), "960 castling PGN round trip");
        rejects([] { t64::Game bad("k7/8/8/8/8/8/8/1K4RR w HG - 0 1", true); });
        auto epds = t64::parseEpd("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - id \"Start; "
                                  "position\"; bm e4 d4; hmvc 0; fmvn 1;\n");
        check(epds.size() == 1 && epds[0].tags.at("EPD_id") == "\"Start; position\"", "EPD quoted semicolon");
        check(t64::parseEpd(t64::exportEpd(epds[0]))[0].tags.at("EPD_bm") == "e4 d4",
              "EPD operation round trip");
        rejects([] { t64::parseEpd("8/8/8/8/8/8/8/K6k w - - bm e4;"); });
        rejects([] { t64::parseEpd("8/8/8/8/8/8/8/K6k w - - bad]key 1;"); });
        auto left = t64::parsePgn("1.e4 {center} e5 *").at(0),
             right = t64::parsePgn("1.e4 {idea} c5 (1...e5 2.Nf3) *").at(0);
        left.merge(right);
        auto merged = t64::parsePgn(left.pgn()).at(0);
        check(merged.nodes[1].children.size() == 2 && merged.nodes[1].comment == "center\nidea",
              "Merge combines branches and comments");
        auto once = left.pgn();
        left.merge(right);
        check(left.pgn() == once, "Merge is idempotent");
        rejects([&] { left.merge(stationary); });
        for (const auto &bad :
             std::vector<std::string>{"[", "{", "( 1. e4 )", "1. e4 $999 *", "1. e4 [Bad", "1. e4 )"})
            rejects([&] { t64::parsePgn(bad); });
        std::istringstream stream(text);
        std::vector<t64::Game> streamed;
        t64::readPgn(stream, [&](t64::Game &&game) { streamed.push_back(std::move(game)); });
        check(streamed.size() == 2 && streamed[0].pgn() == games[0].pgn(),
              "Streaming PGN preserves nested variations and headers");
        std::istringstream comments("[Event \"Quoted * name\"]\n{a * comment} 1.e4 (1.d4 {1-0} d5) e5 "
                                    "*\n[Event \"Next\"]\n1.d4 d5 *");
        int count = 0;
        t64::readPgn(comments, [&](t64::Game &&) { ++count; });
        check(count == 2, "PGN stream ignores result tokens inside comments");
        // Every reported byte boundary can be used to seek and parse the exact remainder.
        for (const auto &pgn : {text, std::string("1.e4 *\r\n\r\n[Event \"Été\"]\r\n1.d4 *"),
                                std::string("1.e4\n[Event \"No result\"]\n1.c4 e5 *\n\n")}) {
            std::istringstream input(pgn);
            std::vector<std::string> records;
            std::vector<uint64_t> offsets;
            t64::readPgnRecords(input, [&](t64::Game &&game, uint64_t offset) {
                records.push_back(game.pgn());
                offsets.push_back(offset);
            });
            for (size_t i = 0; i < offsets.size(); ++i) {
                std::istringstream resumed(pgn);
                resumed.seekg(offsets[i]);
                std::vector<std::string> remainder;
                t64::readPgn(resumed, [&](t64::Game &&game) { remainder.push_back(game.pgn()); });
                check(remainder == std::vector<std::string>(records.begin() + i + 1, records.end()),
                      "Record offsets preserve remaining games, UTF-8, CRLF and missing results");
            }
        }
        // Parallel parsing must keep order/offsets and execute consumers on the calling thread.
        const auto caller = std::this_thread::get_id();
        for (int workers : {1, 2, 4, 8}) {
            std::string corpus;
            for (int i = 0; i < 67; ++i)
                corpus += "[Event \"Partie " + std::to_string(i) + "\"]\r\n" + text;
            std::vector<std::pair<std::string, uint64_t>> expected, parallel;
            std::istringstream serial(corpus), input(corpus);
            t64::readPgnRecords(serial,
                                [&](t64::Game &&g, uint64_t n) { expected.emplace_back(g.pgn(), n); });
            t64::readPgnParallel(input, workers, [&](t64::Game &&g, uint64_t n) {
                check(std::this_thread::get_id() == caller, "Consumers run on caller thread");
                parallel.emplace_back(g.pgn(), n);
            });
            check(parallel == expected, "Parallel PGN is byte-boundary and order equivalent");
            int consumed = 0;
            std::istringstream canceled(corpus);
            rejects([&] {
                t64::readPgnParallel(canceled, workers, [&](t64::Game &&, uint64_t) {
                    if (++consumed == 3)
                        throw std::runtime_error("stop");
                });
            });
            check(consumed == 3, "Cancellation never consumes a later parsed record");
            std::istringstream invalid("1.e4 *\n1.e5 *\n1.d4 *");
            rejects([&] { t64::readPgnParallel(invalid, workers, [](t64::Game &&, uint64_t) {}); });
        }
        std::istringstream invalidWorkers("1.e4 *");
        rejects([&] { t64::readPgnParallel(invalidWorkers, 9, [](t64::Game &&, uint64_t) {}); });
        {
            // Reference keys from the Polyglot book format specification.
            const std::pair<const char *, std::uint64_t> keys[] = {
                {"", 0x463b96181691fc9cULL}, {"e2e4", 0x823c9b50fd114196ULL},
                {"e2e4 d7d5", 0x0756b94461c50fb0ULL}, {"e2e4 d7d5 e4e5", 0x662fafb965db29d4ULL},
                {"e2e4 d7d5 e4e5 f7f5", 0x22a48b5a8e47ff78ULL}, {"e2e4 d7d5 e4e5 f7f5 e1e2", 0x652a607ca3f242c1ULL},
                {"e2e4 d7d5 e4e5 f7f5 e1e2 e8f7", 0x00fdd303c946bdd9ULL},
                {"a2a4 b7b5 h2h4 b5b4 c2c4", 0x3c8123ea7b067637ULL},
                {"a2a4 b7b5 h2h4 b5b4 c2c4 b4c3 a1a3", 0x5c3f9b829b279560ULL}};
            for (const auto &[line, key] : keys) {
                chess::Board board;
                std::istringstream moves(line);
                for (std::string uci; moves >> uci;) board.makeMove(chess::uci::uciToMove(board, uci));
                check(board.hash() == key, "Board hash matches the Polyglot reference key");
            }
            auto entry = [](std::ofstream &out, std::uint64_t key, int from, int to, int promotion, int weight) {
                const std::uint64_t move = to | from << 6 | promotion << 12;
                const std::uint64_t parts[] = {key, move << 48 | std::uint64_t(weight) << 32};
                for (auto part : parts)
                    for (int shift = 56; shift >= 0; shift -= 8) out.put(char(part >> shift & 0xff));
            };
            const std::string path = "polyglot-test.bin";
            chess::Board castle("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"), promote("8/P6k/8/8/8/8/8/K7 w - - 0 1");
            const chess::Board start;
            std::vector<std::tuple<std::uint64_t, int, int, int, int>> rows = {
                {start.hash(), 12, 28, 0, 5}, {start.hash(), 11, 27, 0, 9}, {start.hash(), 12, 36, 0, 50},
                {castle.hash(), 4, 7, 0, 1}, {promote.hash(), 48, 56, 4, 3}, {promote.hash(), 48, 56, 1, 2}};
            std::sort(rows.begin(), rows.end(), [](auto &a, auto &b) { return std::get<0>(a) < std::get<0>(b); });
            {
                std::ofstream out(path, std::ios::binary);
                for (auto [key, from, to, promotion, weight] : rows) entry(out, key, from, to, promotion, weight);
            }
            t64::PolyglotBook book;
            check(book.open(path), "Polyglot book opens");
            const auto opening = book.moves(start);
            check(opening.size() == 2 && chess::uci::moveToUci(opening[0].move) == "d2d4" && opening[0].weight == 9 &&
                      chess::uci::moveToUci(opening[1].move) == "e2e4",
                  "Book moves are legal only and sorted by weight");
            const auto castling = book.moves(castle);
            check(castling.size() == 1 && castling[0].move.typeOf() == chess::Move::CASTLING &&
                      chess::uci::moveToUci(castling[0].move) == "e1g1",
                  "Polyglot king-takes-rook becomes castling");
            const auto promotions = book.moves(promote);
            check(promotions.size() == 2 && chess::uci::moveToUci(promotions[0].move) == "a7a8q" &&
                      chess::uci::moveToUci(promotions[1].move) == "a7a8n",
                  "Polyglot promotions keep their piece");
            check(book.moves(chess::Board("8/8/8/8/8/8/8/K6k w - - 0 1")).empty(), "Unknown position has no book move");
            { std::ofstream bad("polyglot-bad.bin", std::ios::binary); bad << "short"; }
            check(!book.open("polyglot-bad.bin") && !book.isOpen() && !book.open("missing.bin"), "Invalid book files are rejected");
            std::remove(path.c_str());
            std::remove("polyglot-bad.bin");
        }
        {
            const std::string a = "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3";
            const std::string sameP = "rnbqkb1r/pppp1ppp/5n2/4p3/4P3/2N5/PPPP1PPP/R1BQKBNR w KQkq - 2 3";
            const std::string otherP = "rnbqkbnr/pppp1ppp/8/4p3/3PP3/8/PPP2PPP/RNBQKBNR b KQkq d3 0 2";
            const std::string swapped = "rnbqkbnr/pppp1ppp/8/4P3/4p3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1";
            check(t64::pawnKey(a) == t64::pawnKey(sameP) && t64::pawnKey(a) != t64::pawnKey(otherP) &&
                      t64::pawnKey(a) != t64::pawnKey(swapped) && t64::positionHash(a) != t64::positionHash(sameP),
                  "Pawn key ignores pieces but not pawn squares or colours");
            check(t64::positionHash(a) == chess::Board(a).hash() &&
                      t64::positionHash("r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq -") ==
                          chess::Board(a).hash(),
                  "Position hash accepts four- or six-field FEN");
        }
        {
            // Précision : une position nulle vaut la moitié des chances de gain, et
            // l'avantage les fait croître sans jamais atteindre les bornes.
            check(std::abs(t64::winPercent(0) - 50) < 1e-9, "An equal position is half the winning chances");
            check(t64::winPercent(300) > 75 && t64::winPercent(300) < 100 &&
                      std::abs(t64::winPercent(-300) + t64::winPercent(300) - 100) < 1e-9,
                  "Winning chances grow with the advantage and stay symmetric");
            check(t64::moveAccuracy(50, 50) == 100 && t64::moveAccuracy(40, 60) == 100,
                  "Keeping or improving the position is not punished");
            check(t64::moveAccuracy(50, 20) < 40 && t64::moveAccuracy(50, 47) > 85 &&
                      t64::moveAccuracy(50, 20) < t64::moveAccuracy(50, 40),
                  "The mark falls as the lost chances grow");
            // Même perte en centipions, deux situations : elle compte dans une position
            // égale, presque pas quand la partie est déjà gagnée. C'est tout l'intérêt
            // de passer par les chances de gain plutôt que par les centipions.
            const double tight = t64::moveAccuracy(t64::winPercent(0), t64::winPercent(-100));
            const double won = t64::moveAccuracy(t64::winPercent(900), t64::winPercent(800));
            check(tight < won - 20, "The same centipawn loss costs more in a close position");
            check(t64::aggregateAccuracy({}) < 0, "An unmeasurable set is not a zero");
            check(std::abs(t64::aggregateAccuracy({100, 100, 100}) - 100) < 1e-9,
                  "A flawless set is a hundred");
            // La moyenne harmonique fait peser la gaffe : la note reste sous la moyenne
            // arithmétique, qui vaudrait 92,5 ici.
            const double blundered = t64::aggregateAccuracy({100, 100, 100, 70});
            check(blundered < 92 && blundered > 60, "One blunder weighs on an otherwise clean set");
            // Phases : position de départ, milieu de jeu après échanges, finale réduite.
            check(t64::phaseOf(chess::Board(std::string(chess::constants::STARTPOS)), 0) ==
                      t64::Phase::Opening,
                  "The initial position is the opening");
            check(t64::phaseOf(chess::Board(std::string(chess::constants::STARTPOS)), 20) ==
                      t64::Phase::Middlegame,
                  "A quiet opening still ends after ten moves each");
            check(t64::phaseOf(chess::Board("4k3/pp3ppp/8/8/8/8/PP3PPP/4K3 w - - 0 30"), 60) ==
                      t64::Phase::Endgame,
                  "Kings and pawns alone are an endgame");
            check(t64::phaseOf(chess::Board("r3k2r/pp3ppp/8/8/8/8/PP3PPP/R3K2R w - - 0 20"), 40) ==
                      t64::Phase::Endgame,
                  "Four rooks and pawns are an endgame");
            check(t64::phaseOf(chess::Board("r1bqkb1r/pppppppp/2n2n2/8/8/2N2N2/PPPPPPPP/R1BQKB1R w - - 0 5"),
                               8) == t64::Phase::Opening,
                  "A full board early on is still the opening");
        }
        std::cout << checks << " core checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
