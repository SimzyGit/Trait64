// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "chess.hpp"
#include <functional>
#include <istream>
#include <map>
#include <string>
#include <vector>

namespace t64 {
struct Node {
    int parent = -1;
    std::string uci, san, fen, comment;
    std::vector<int> children, nags;
};
struct Game {
    bool chess960 = false;
    std::map<std::string, std::string> tags;
    std::vector<Node> nodes;
    explicit Game(const std::string &fen = std::string(chess::constants::STARTPOS), bool variant960 = false);
    chess::Board boardAt(int node) const;
    int play(int parent, const std::string &uci);
    int playSan(int parent, const std::string &san);
    std::string pgn() const;
    std::vector<int> mainline() const;
    void promote(int node);
    void removeVariation(int node);
    void merge(const Game &other);
};
chess::Board checkedBoard(const std::string &fen, bool chess960 = false);
// Keep every generated position within the same serializable limits as imported FENs.
void checkBoardCounters(const chess::Board &board);
// Resolve forced endings before draws that a player may claim.
std::pair<chess::GameResultReason, chess::GameResult> gameResult(const chess::Board &board);
std::string chess960Fen(int number);
std::vector<Game> parseEpd(const std::string &text);
std::string exportEpd(const Game &game, int node = 0);
void readPgn(std::istream &input, const std::function<void(Game &&)> &consume);
// Offsets count bytes from the stream's initial position, before the next record.
// Callbacks may throw to stop; no subsequent record is consumed in that case.
void readPgnRecords(std::istream &input, const std::function<void(Game &&, uint64_t)> &consume,
                    const std::function<void(uint64_t)> &progress = {});
// Raw framing is sequential; parsed results are delivered in file order on the caller's thread.
void scanPgnRecords(std::istream &input, const std::function<void(std::string &&, uint64_t)> &consume,
                    const std::function<void(uint64_t)> &progress = {});
void readPgnParallel(std::istream &input, int workers, const std::function<void(Game &&, uint64_t)> &consume,
                     const std::function<void(uint64_t)> &progress = {});
std::vector<Game> parsePgn(const std::string &pgn, size_t maxGames = 10000,
                           const std::function<void()> &checkpoint = {});
std::string positionKey(const std::string &fen);
uint64_t perft(chess::Board &board, int depth);
} // namespace t64
