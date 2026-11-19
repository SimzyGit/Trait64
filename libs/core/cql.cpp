// SPDX-License-Identifier: AGPL-3.0-or-later
#include "cql.h"
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace t64::cql {
namespace {
using Squares = std::uint64_t; // bit = index chess-library (a1 = 0, h8 = 63)
constexpr Squares everywhere = ~Squares(0);

// Les pièces voulues : un bit par symbole de « KQRBNPkqrbnp_ ».
constexpr std::string_view symbols = "KQRBNPkqrbnp_";
using Pieces = unsigned;

[[noreturn]] void fail(const std::string &message) {
    throw std::invalid_argument("Requête CQL : " + message);
}

// Ce que contient une case, en symbole (« _ » si elle est vide).
char at(const chess::Board &board, int square) {
    const auto piece = board.at(chess::Square(square));
    if (piece == chess::Piece::NONE)
        return '_';
    return static_cast<std::string>(piece)[0];
}

// L'ensemble des cases où se trouve l'une des pièces voulues, parmi `within`.
Squares occupied(const chess::Board &board, Pieces pieces, Squares within) {
    Squares out = 0;
    for (int square = 0; square < 64; ++square)
        if ((within >> square & 1) && (pieces >> symbols.find(at(board, square)) & 1))
            out |= Squares(1) << square;
    return out;
}
} // namespace

struct Filter {
    virtual ~Filter() = default;
    virtual bool test(const chess::Board &board, std::string_view move) const = 0;
};

namespace {
struct Designator final : Filter {
    Pieces pieces;
    Squares squares;
    Designator(Pieces p, Squares s) : pieces(p), squares(s) {}
    Squares where(const chess::Board &board) const { return occupied(board, pieces, squares); }
    bool test(const chess::Board &board, std::string_view) const override { return where(board) != 0; }
};
struct Fact final : Filter {
    enum Kind { Check, Mate, Stalemate, WhiteToMove, BlackToMove } kind;
    explicit Fact(Kind k) : kind(k) {}
    bool test(const chess::Board &board, std::string_view) const override {
        switch (kind) {
        case WhiteToMove:
            return board.sideToMove() == chess::Color::WHITE;
        case BlackToMove:
            return board.sideToMove() == chess::Color::BLACK;
        case Check:
            return board.inCheck();
        default: {
            chess::Movelist moves;
            chess::movegen::legalmoves(moves, board);
            return moves.empty() && board.inCheck() == (kind == Mate);
        }
        }
    }
};
struct Attacks final : Filter {
    std::unique_ptr<Designator> from, to;
    Attacks(std::unique_ptr<Designator> a, std::unique_ptr<Designator> b)
        : from(std::move(a)), to(std::move(b)) {}
    bool test(const chess::Board &board, std::string_view) const override {
        const auto attackers = from->where(board);
        if (!attackers)
            return false;
        auto targets = to->where(board);
        for (int square = 0; square < 64; ++square) {
            if (!(targets >> square & 1))
                continue;
            for (const auto color : {chess::Color::WHITE, chess::Color::BLACK})
                if (chess::attacks::attackers(board, color, chess::Square(square)).getBits() & attackers)
                    return true;
        }
        return false;
    }
};
struct Move final : Filter {
    std::unique_ptr<Designator> from, to; // vides : n'importe où
    bool test(const chess::Board &board, std::string_view move) const override {
        if (move.size() < 4)
            return false;
        const auto square = [](std::string_view name) { return (name[1] - '1') * 8 + (name[0] - 'a'); };
        const int a = square(move.substr(0, 2)), b = square(move.substr(2, 2));
        // La pièce qui a bougé est maintenant sur sa case d'arrivée (le roi pour un roque).
        if (from && !(from->squares >> a & 1))
            return false;
        if (from && !(from->pieces >> symbols.find(at(board, b)) & 1))
            return false;
        return !to || (to->squares >> b & 1);
    }
};
struct Not final : Filter {
    std::unique_ptr<Filter> inner;
    explicit Not(std::unique_ptr<Filter> f) : inner(std::move(f)) {}
    bool test(const chess::Board &board, std::string_view move) const override {
        return !inner->test(board, move);
    }
};
struct All final : Filter {
    std::vector<std::unique_ptr<Filter>> parts;
    bool test(const chess::Board &board, std::string_view move) const override {
        for (const auto &part : parts)
            if (!part->test(board, move))
                return false;
        return true;
    }
};
struct Any final : Filter {
    std::unique_ptr<Filter> left, right;
    bool test(const chess::Board &board, std::string_view move) const override {
        return left->test(board, move) || right->test(board, move);
    }
};

class Parser {
  public:
    explicit Parser(std::string_view text) : text_(text) {}

    std::unique_ptr<Filter> query() {
        // Une requête tient en quelques lignes ; la borne évite qu'un texte collé par erreur
        // (ou mille « not » imbriqués) ne fasse déborder la pile.
        if (text_.size() > 4096)
            fail("trop longue (4 096 caractères au plus).");
        skip();
        // L'en-tête « cql(...) » des fichiers CQL est accepté et ignoré.
        if (text_.substr(at_, 3) == "cql" && next(3) == '(') {
            const auto close = text_.find(')', at_);
            if (close == std::string_view::npos)
                fail("parenthèse de « cql( » non fermée.");
            at_ = close + 1;
        }
        auto all = sequence('\0');
        skip();
        if (at_ < text_.size())
            fail("« " + std::string(1, text_[at_]) + " » inattendu.");
        if (all->parts.empty())
            fail("rien à chercher.");
        return all;
    }

  private:
    std::string_view text_;
    size_t at_ = 0;
    int depth_ = 0;

    char next(size_t offset = 0) const { return at_ + offset < text_.size() ? text_[at_ + offset] : '\0'; }
    void skip() {
        while (at_ < text_.size()) {
            if (std::isspace(static_cast<unsigned char>(text_[at_])))
                ++at_;
            else if (text_.substr(at_, 2) == "//") // commentaire jusqu'à la fin de la ligne
                while (at_ < text_.size() && text_[at_] != '\n')
                    ++at_;
            else
                break;
        }
    }
    std::string word() {
        skip();
        size_t end = at_;
        while (end < text_.size() && std::isalpha(static_cast<unsigned char>(text_[end])))
            ++end;
        return std::string(text_.substr(at_, end - at_));
    }
    bool take(std::string_view keyword) {
        if (word() != keyword)
            return false;
        at_ += keyword.size();
        return true;
    }

    std::unique_ptr<All> sequence(char closing) {
        auto all = std::make_unique<All>();
        for (;;) {
            skip();
            if (at_ >= text_.size() || next() == closing)
                break;
            auto filter = unit();
            while (take("or")) {
                auto either = std::make_unique<Any>();
                either->left = std::move(filter);
                either->right = unit();
                filter = std::move(either);
            }
            all->parts.push_back(std::move(filter));
        }
        return all;
    }

    std::unique_ptr<Filter> unit() {
        struct Depth {
            int &depth;
            explicit Depth(int &d) : depth(d) {
                if (++depth > 64)
                    fail("trop d'imbrications.");
            }
            ~Depth() { --depth; }
        } guard(depth_);
        skip();
        if (next() == '{') {
            ++at_;
            auto group = sequence('}');
            if (next() != '}')
                fail("accolade non fermée.");
            ++at_;
            return group;
        }
        const auto name = word();
        static const std::pair<std::string_view, Fact::Kind> facts[] = {{"check", Fact::Check},
                                                                        {"mate", Fact::Mate},
                                                                        {"stalemate", Fact::Stalemate},
                                                                        {"wtm", Fact::WhiteToMove},
                                                                        {"btm", Fact::BlackToMove}};
        for (const auto &[keyword, kind] : facts)
            if (name == keyword) {
                at_ += keyword.size();
                return std::make_unique<Fact>(kind);
            }
        if (name == "not") {
            at_ += 3;
            return std::make_unique<Not>(unit());
        }
        if (name == "move") {
            at_ += 4;
            auto move = std::make_unique<Move>();
            for (;;) {
                if (take("from"))
                    move->from = designator();
                else if (take("to"))
                    move->to = designator();
                else
                    break;
            }
            if (!move->from && !move->to)
                fail("« move » attend « from » ou « to ».");
            return move;
        }
        // Les mots de plus d'une lettre qui ne sont pas des désignateurs (« Ra1 » en est un).
        if (name.size() > 1 && !std::strchr("KQRBNPkqrbnpAa", name[0]))
            fail("« " + name + " » n'est pas compris par Trait64 (seul un sous-ensemble de CQL l'est).");
        auto piece = designator();
        if (take("attacks"))
            return std::make_unique<Attacks>(std::move(piece), designator());
        return piece;
    }

    std::unique_ptr<Designator> designator() {
        skip();
        Pieces pieces = 0;
        const auto symbol = [&](char c) {
            if (c == 'A')
                pieces |= 0b111111;
            else if (c == 'a')
                pieces |= 0b111111 << 6;
            else if (const auto i = symbols.find(c); i != std::string_view::npos)
                pieces |= 1u << i;
            else
                fail("pièce « " + std::string(1, c) + " » inconnue.");
        };
        // « a » et « b » sont aussi des colonnes : suivis d'un chiffre ou d'un tiret, ils
        // commencent une case (« b5 »), sinon ils désignent une pièce (« bb5 », « [Aa] »).
        const auto square = [&](size_t offset) {
            const char c = next(offset), after = next(offset + 1);
            return c && std::strchr("abcdefgh", c) &&
                   (std::isdigit(static_cast<unsigned char>(after)) || after == '-');
        };
        const auto pieceSymbol = [&](size_t offset) {
            const char c = next(offset);
            return c && std::strchr("KQRBNPkqrbnpAa_", c) && !square(offset);
        };
        bool hasPieces = false;
        if (next() == '[' && pieceSymbol(1)) {
            ++at_;
            while (next() && next() != ']')
                symbol(text_[at_++]);
            if (next() != ']')
                fail("crochet non fermé.");
            ++at_;
            hasPieces = true;
        } else if (pieceSymbol(0)) {
            symbol(text_[at_++]);
            hasPieces = true;
        }
        Squares squares = everywhere;
        if (std::strchr("abcdefgh[", next()) && next())
            squares = squareSet();
        else if (!hasPieces)
            fail("une pièce ou une case était attendue.");
        if (!hasPieces) // une case seule : n'importe quelle pièce (« _ » pour le vide)
            pieces = 0b111111111111;
        return std::make_unique<Designator>(pieces, squares);
    }

    // e4, a-h7, [a-d1-4], [e4,d5,a-h1]
    Squares squareSet() {
        if (next() != '[')
            return squareRange();
        ++at_;
        Squares squares = 0;
        for (;;) {
            squares |= squareRange();
            if (next() == ',') {
                ++at_;
                continue;
            }
            if (next() != ']')
                fail("crochet de cases non fermé.");
            ++at_;
            return squares;
        }
    }
    Squares squareRange() {
        const auto range = [&](char low, char high) {
            const char first = next();
            if (first < low || first > high)
                fail("case attendue (de a1 à h8).");
            ++at_;
            char last = first;
            if (next() == '-') {
                ++at_;
                last = next();
                if (last < first || last > high)
                    fail("intervalle de cases invalide.");
                ++at_;
            }
            return std::pair{first - low, last - low};
        };
        const auto [fileFrom, fileTo] = range('a', 'h');
        const auto [rankFrom, rankTo] = range('1', '8');
        Squares squares = 0;
        for (int file = fileFrom; file <= fileTo; ++file)
            for (int rank = rankFrom; rank <= rankTo; ++rank)
                squares |= Squares(1) << (rank * 8 + file);
        return squares;
    }
};
} // namespace

Query::Query(std::string_view text) : root_(Parser(text).query()) {}
Query::~Query() = default;
Query::Query(Query &&) noexcept = default;

bool Query::matches(const chess::Board &board, std::string_view lastMove) const {
    return root_->test(board, lastMove);
}
} // namespace t64::cql
