// SPDX-License-Identifier: AGPL-3.0-or-later
#include "game.h"
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool value, const char *message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
} // namespace

int main() {
    try {
        std::mt19937 random(0x64302026);
        std::vector<std::string> corpus;
        for (int sample = 0; sample < 80; ++sample) {
            const bool frc = sample % 2;
            t64::Game game(frc ? t64::chess960Fen(static_cast<int>(random() % 960))
                              : std::string(chess::constants::STARTPOS), frc);
            auto board = game.boardAt(0);
            int node = 0;
            for (int ply = 0; ply < 40; ++ply) {
                chess::Movelist moves;
                chess::movegen::legalmoves(moves, board);
                if (moves.empty())
                    break;
                const auto move = moves[random() % moves.size()];
                const int parent = node;
                node = game.play(parent, chess::uci::moveToUci(move, frc));
                if (ply % 7 == 0 && moves.size() > 1) {
                    const auto alternative = moves[(random() % (moves.size() - 1) + 1) % moves.size()];
                    if (alternative != move) {
                        const int branch = game.play(parent, chess::uci::moveToUci(alternative, frc));
                        game.nodes[branch].comment = "Branche de contrôle";
                        if (ply % 2 == 0)
                            game.removeVariation(branch);
                    }
                }
                board.makeMove(move);
                check(game.boardAt(node).getFen() == board.getFen(), "Generated study and legal board diverged");
                if (ply % 9 == 0) {
                    game.nodes[node].comment = "Analyse {test} avec accents : échec";
                    game.nodes[node].nags = {1, 14};
                }
            }
            const auto pgn = game.pgn();
            auto restored = t64::parsePgn(pgn);
            check(restored.size() == 1 && restored[0].pgn() == pgn, "Generated PGN round trip changed study");
            check(restored[0].boardAt(restored[0].mainline().back()).getFen() == board.getFen(),
                  "Round trip changed final board");
            auto merged = restored[0];
            merged.merge(restored[0]);
            check(merged.pgn() == pgn, "Self merge must be idempotent");
            corpus.push_back(pgn);
        }
        std::string joined;
        for (const auto &pgn : corpus)
            joined += pgn + '\n';
        for (int workers : {0, 1, 2, 8}) {
            std::istringstream input(joined);
            size_t count = 0;
            uint64_t previous = 0;
            auto consume = [&](t64::Game &&game, uint64_t offset) {
                check(count < corpus.size() && game.pgn() == corpus[count], "Streaming parser changed order or study");
                check(offset > previous && offset <= joined.size(), "Record byte offsets must increase within input");
                previous = offset;
                ++count;
            };
            if (workers)
                t64::readPgnParallel(input, workers, consume);
            else
                t64::readPgnRecords(input, consume);
            check(count == corpus.size(), "Streaming parser lost a record");
        }
        int accepted = 0, rejected = 0;
        for (int sample = 0; sample < 3000; ++sample) {
            const bool fen = sample % 2;
            auto input = fen ? std::string(chess::constants::STARTPOS) : corpus[random() % corpus.size()];
            const int edits = 1 + static_cast<int>(random() % 4);
            for (int i = 0; i < edits; ++i) {
                const auto pos = random() % (input.size() + 1);
                const auto byte = static_cast<char>(random() % 256);
                if (pos == input.size())
                    input += byte;
                else if (random() % 3 == 0)
                    input.erase(pos, 1);
                else
                    input[pos] = byte;
            }
            if (fen) {
                std::optional<chess::Board> board;
                try {
                    board = t64::checkedBoard(input);
                } catch (const std::exception &) {
                    ++rejected;
                }
                if (board) {
                    ++accepted;
                    check(t64::checkedBoard(board->getFen()).getFen() == board->getFen(),
                          "Accepted FEN must round trip");
                }
            } else {
                std::optional<std::vector<t64::Game>> games;
                try {
                    games = t64::parsePgn(input);
                } catch (const std::exception &) {
                    ++rejected;
                }
                if (games) {
                    ++accepted;
                    for (const auto &game : *games) {
                        const auto canonical = game.pgn();
                        const auto again = t64::parsePgn(canonical);
                        check(again.size() == 1 && again[0].pgn() == canonical,
                              "Accepted mutated PGN must round trip");
                    }
                }
            }
        }
        std::cout << checks << " robustness checks; 80 generated studies; 3000 mutations ("
                  << accepted << " accepted, " << rejected << " rejected)\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
