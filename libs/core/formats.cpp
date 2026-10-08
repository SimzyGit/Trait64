// SPDX-License-Identifier: AGPL-3.0-or-later
#include "game.h"
#include <algorithm>
#include <array>
#include <sstream>
#include <stdexcept>

namespace t64 {
std::string chess960Fen(int number) {
    if (number < 0 || number > 959)
        throw std::runtime_error("Numéro Chess960 : 0 à 959.");
    std::string rank(8, ' ');
    rank[(number % 4) * 2 + 1] = 'B';
    number /= 4;
    rank[(number % 4) * 2] = 'B';
    number /= 4;
    auto empty = [&] {
        std::vector<int> v;
        for (int i = 0; i < 8; ++i)
            if (rank[i] == ' ')
                v.push_back(i);
        return v;
    };
    rank[empty()[number % 6]] = 'Q';
    number /= 6;
    const std::array<std::pair<int, int>, 10> knights{
        {{0, 1}, {0, 2}, {0, 3}, {0, 4}, {1, 2}, {1, 3}, {1, 4}, {2, 3}, {2, 4}, {3, 4}}};
    auto e = empty();
    auto [a, b] = knights[number];
    rank[e[a]] = 'N';
    rank[e[b]] = 'N';
    e = empty();
    rank[e[0]] = 'R';
    rank[e[1]] = 'K';
    rank[e[2]] = 'R';
    auto black = rank;
    std::transform(black.begin(), black.end(), black.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::string rights;
    rights += char('A' + e[2]);
    rights += char('A' + e[0]);
    rights += char('a' + e[2]);
    rights += char('a' + e[0]);
    return black + "/pppppppp/8/8/8/8/PPPPPPPP/" + rank + " w " + rights + " - 0 1";
}
void Game::merge(const Game &other) {
    if (chess960 != other.chess960 || nodes[0].fen != other.nodes[0].fen)
        throw std::runtime_error("Les études doivent avoir la même position initiale et la même variante.");
    Game candidate = *this; // Entire merge is atomic, including a node limit or illegal imported move.
    std::vector<std::pair<int, int>> pending{{0, 0}};
    while (!pending.empty()) {
        auto [source, target] = pending.back();
        pending.pop_back();
        const auto &from = other.nodes[source];
        auto &to = candidate.nodes[target];
        if (!from.comment.empty() && to.comment.find(from.comment) == std::string::npos)
            to.comment += (to.comment.empty() ? "" : "\n") + from.comment;
        for (int n : from.nags)
            if (std::find(to.nags.begin(), to.nags.end(), n) == to.nags.end())
                to.nags.push_back(n);
        for (int child : from.children)
            pending.emplace_back(child, candidate.play(target, other.nodes[child].uci));
    }
    *this = std::move(candidate);
}
std::vector<Game> parseEpd(const std::string &text) {
    if (text.size() > 64 * 1024 * 1024)
        throw std::runtime_error("EPD trop volumineux.");
    std::vector<Game> games;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.find_first_not_of(" \r\t") == std::string::npos || line[0] == '#')
            continue;
        std::istringstream in(line);
        std::string fen, field;
        for (int i = 0; i < 4; ++i) {
            if (!(in >> field))
                throw std::runtime_error("EPD : quatre champs de position requis.");
            if (i)
                fen += ' ';
            fen += field;
        }
        std::string rest;
        std::getline(in, rest);
        std::vector<std::string> ops;
        std::string op;
        bool quote = false, escape = false;
        for (char c : rest) {
            if (c == ';' && !quote) {
                ops.push_back(op);
                op.clear();
                continue;
            }
            op += c;
            if (escape)
                escape = false;
            else if (c == '\\' && quote)
                escape = true;
            else if (c == '"')
                quote = !quote;
        }
        if (quote)
            throw std::runtime_error("EPD : guillemet non fermé.");
        if (!op.empty())
            ops.push_back(op);
        std::map<std::string, std::string> operations;
        for (auto &o : ops) {
            std::istringstream stream(o);
            std::string key, val;
            if (!(stream >> key))
                continue;
            if (!std::isalpha(static_cast<unsigned char>(key.front())) ||
                !std::all_of(key.begin(), key.end(),
                             [](unsigned char c) { return std::isalnum(c) || c == '_'; }))
                throw std::runtime_error("EPD : code d’opération invalide.");
            std::getline(stream, val);
            auto b = val.find_first_not_of(" \t");
            operations[key] = b == std::string::npos ? "" : val.substr(b);
        }
        Game g(fen + " " + (operations.contains("hmvc") ? operations["hmvc"] : "0") + " " +
               (operations.contains("fmvn") ? operations["fmvn"] : "1"));
        for (const auto &[key, val] : operations) {
            if (key == "hmvc" || key == "fmvn")
                continue;
            g.tags["EPD_" + key] = val;
            if (key == "bm" || key == "am") {
                std::istringstream moves(val);
                std::string move;
                while (moves >> move) {
                    auto copy = g;
                    copy.playSan(0, move);
                }
            }
        }
        if (operations.contains("id"))
            g.nodes[0].comment = operations["id"];
        games.push_back(std::move(g));
        if (games.size() > 10000)
            throw std::runtime_error("EPD limité à 10 000 positions par import.");
    }
    return games;
}
std::string exportEpd(const Game &game, int node) {
    if (game.chess960)
        throw std::runtime_error("L’export EPD concerne les échecs classiques.");
    const auto board = game.boardAt(node);
    std::istringstream fen(board.getFen());
    std::string f, out;
    for (int i = 0; i < 4; ++i) {
        fen >> f;
        if (i)
            out += ' ';
        out += f;
    }
    fen >> f;
    out += " hmvc " + f + ";";
    fen >> f;
    out += " fmvn " + f + ";";
    if (node == 0)
        for (const auto &[key, value] : game.tags)
            if (key.starts_with("EPD_"))
                out += ' ' + key.substr(4) + ' ' + value + ';';
    return out + "\n";
}
} // namespace t64

namespace t64 {
void readPgn(std::istream &input, const std::function<void(Game &&)> &consume) {
    std::string buffer, token;
    bool header = false, quoted = false, escaped = false, brace = false, lineComment = false, moves = false,
         lineStart = true;
    int depth = 0;
    auto flush = [&] {
        if (buffer.find_first_not_of(" \t\r\n") == std::string::npos) {
            buffer.clear();
            return;
        }
        auto games = parsePgn(buffer, 1);
        if (!games.empty())
            consume(std::move(games[0]));
        buffer.clear();
        token.clear();
        moves = false;
    };
    char c;
    while (input.get(c)) {
        if (!quoted && !brace && !lineComment && !header) {
            bool delim = std::isspace(static_cast<unsigned char>(c)) ||
                         std::string("{}();[]").find(c) != std::string::npos;
            if (delim && !token.empty()) {
                if (!depth && (token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*"))
                    flush();
                else
                    moves = true;
                token.clear();
            }
            if (c == '[' && moves && depth == 0)
                flush();
        }
        buffer += c;
        if (buffer.size() > 64 * 1024 * 1024)
            throw std::runtime_error("Une partie PGN dépasse 64 Mio.");
        if (lineComment) {
            if (c == '\n')
                lineComment = false;
        } else if (brace) {
            if (c == '}')
                brace = false;
        } else if (quoted) {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                quoted = false;
        } else if (header) {
            if (c == '"')
                quoted = true;
            else if (c == ']')
                header = false;
        } else if (c == '[')
            header = true;
        else if (c == '{')
            brace = true;
        else if (c == ';' || (c == '%' && lineStart))
            lineComment = true;
        else if (c == '(') {
            ++depth;
            moves = true;
        } else if (c == ')') {
            --depth;
            if (depth < 0)
                throw std::runtime_error("Parenthèse PGN inattendue.");
        } else if (!std::isspace(static_cast<unsigned char>(c)))
            token += c;
        lineStart = c == '\n';
    }
    if (input.bad())
        throw std::runtime_error("Erreur de lecture du PGN.");
    flush();
}
} // namespace t64
