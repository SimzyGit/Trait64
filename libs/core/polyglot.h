// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "chess.hpp"
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace t64 {
struct BookMove {
    chess::Move move;
    std::uint16_t weight = 0;
};

// Read-only Polyglot opening book (.bin). Entries are 16 bytes, big-endian, sorted by key;
// lookups binary-search the file so large books are never loaded into memory.
class PolyglotBook {
  public:
    bool open(const std::string &path);
    bool isOpen() const { return entries_ > 0; }
    const std::string &path() const { return path_; }
    // Legal book moves for this position, highest weight first.
    std::vector<BookMove> moves(const chess::Board &board) const;

  private:
    struct Entry {
        std::uint64_t key = 0;
        std::uint16_t move = 0, weight = 0;
    };
    Entry read(std::uint64_t index) const;
    mutable std::ifstream file_;
    std::uint64_t entries_ = 0;
    std::string path_;
};
} // namespace t64
