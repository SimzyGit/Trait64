// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "chess.hpp"
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
    std::map<std::string, std::string> tags;
    std::vector<Node> nodes;
    explicit Game(const std::string &fen = std::string(chess::constants::STARTPOS));
    chess::Board boardAt(int node) const;
    int play(int parent, const std::string &uci);
    int playSan(int parent, const std::string &san);
    std::string pgn() const;
    std::vector<int> mainline() const;
    void promote(int node);
    void removeVariation(int node);
};
chess::Board checkedBoard(const std::string &fen);
std::vector<Game> parsePgn(const std::string &pgn, size_t maxGames = 10000);
std::string positionKey(const std::string &fen);
uint64_t perft(chess::Board &board, int depth);
} // namespace t64
