// SPDX-License-Identifier: AGPL-3.0-or-later
#include "polyglot.h"
#include <algorithm>

namespace t64 {
namespace {
std::uint64_t bigEndian(const unsigned char *bytes, int size) {
    std::uint64_t value = 0;
    for (int i = 0; i < size; ++i) value = value << 8 | bytes[i];
    return value;
}
} // namespace

bool PolyglotBook::open(const std::string &path) {
    file_ = std::ifstream(path, std::ios::binary);
    entries_ = 0;
    path_.clear();
    if (!file_) return false;
    file_.seekg(0, std::ios::end);
    const auto size = static_cast<std::uint64_t>(file_.tellg());
    if (size == 0 || size % 16 != 0) return false;
    entries_ = size / 16;
    path_ = path;
    return true;
}

PolyglotBook::Entry PolyglotBook::read(std::uint64_t index) const {
    unsigned char bytes[16]{};
    file_.clear();
    file_.seekg(static_cast<std::streamoff>(index * 16));
    file_.read(reinterpret_cast<char *>(bytes), 16);
    return {bigEndian(bytes, 8), static_cast<std::uint16_t>(bigEndian(bytes + 8, 2)),
            static_cast<std::uint16_t>(bigEndian(bytes + 10, 2))};
}

std::vector<BookMove> PolyglotBook::moves(const chess::Board &board) const {
    std::vector<BookMove> out;
    if (!isOpen()) return out;
    // chess-library hashes with the Polyglot Random64 table, so board.hash() is the book key.
    const std::uint64_t key = board.hash();
    std::uint64_t low = 0, high = entries_;
    while (low < high) {
        const auto middle = low + (high - low) / 2;
        if (read(middle).key < key) low = middle + 1;
        else high = middle;
    }
    chess::Movelist legal;
    chess::movegen::legalmoves(legal, board);
    for (auto index = low; index < entries_; ++index) {
        const auto entry = read(index);
        if (entry.key != key) break;
        // Bits: to file 0-2, to rank 3-5, from file 6-8, from rank 9-11, promotion 12-14.
        // Castling is stored as "king takes rook", which is also chess-library's encoding.
        const auto to = chess::Square(chess::File(entry.move & 7), chess::Rank(entry.move >> 3 & 7));
        const auto from = chess::Square(chess::File(entry.move >> 6 & 7), chess::Rank(entry.move >> 9 & 7));
        const int promotion = entry.move >> 12 & 7;
        for (const auto &move : legal) {
            if (move.from() != from || move.to() != to) continue;
            const bool promoting = move.typeOf() == chess::Move::PROMOTION;
            if (promoting != (promotion != 0)) continue;
            if (promoting && move.promotionType() != chess::PieceType(static_cast<chess::PieceType::underlying>(promotion)))
                continue;
            out.push_back({move, entry.weight});
            break;
        }
    }
    std::stable_sort(out.begin(), out.end(), [](const BookMove &a, const BookMove &b) { return a.weight > b.weight; });
    return out;
}
} // namespace t64
