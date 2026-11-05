// SPDX-License-Identifier: AGPL-3.0-or-later
#include "game.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <regex>
#include <sstream>
#include <string_view>
#include <stdexcept>

namespace t64 {
namespace {
// La vue évite de bâtir une std::string à chaque appel : le lecteur en fait un par
// balise et par coup, et le message ne sert que dans le cas qui échoue.
void require(bool ok, std::string_view message) {
    if (!ok)
        throw std::runtime_error(std::string(message));
}
bool result(const std::string &s) {
    return s == "1-0" || s == "0-1" || s == "1/2-1/2" || s == "*";
}
std::string quote(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (c == '\\' || c == '"')
            out += '\\';
        out += (c == '\n' || c == '\r') ? ' ' : c;
    }
    return out;
}
std::string annotation(std::string s) {
    std::replace(s.begin(), s.end(), '}', ']');
    std::replace(s.begin(), s.end(), '{', '[');
    return s;
}
std::string stripCheck(std::string s) {
    while (!s.empty() && (s.back() == '+' || s.back() == '#'))
        s.pop_back();
    return s;
}
} // namespace

void checkBoardCounters(const chess::Board &board) {
    require(board.halfMoveClock() <= 255 && board.fullMoveNumber() <= 32000,
            "Limite des compteurs FEN atteinte (255 demi-coups sans prise/pion, coup 32000).");
}

std::pair<chess::GameResultReason, chess::GameResult> gameResult(const chess::Board &board) {
    using Reason = chess::GameResultReason;
    using Result = chess::GameResult;
    if (!chess::movegen::anylegalmoves(board))
        return board.inCheck() ? std::pair{Reason::CHECKMATE, Result::LOSE}
                               : std::pair{Reason::STALEMATE, Result::DRAW};
    if (board.isInsufficientMaterial())
        return {Reason::INSUFFICIENT_MATERIAL, Result::DRAW};
    if (board.isHalfMoveDraw())
        return {Reason::FIFTY_MOVE_RULE, Result::DRAW};
    if (board.isRepetition())
        return {Reason::THREEFOLD_REPETITION, Result::DRAW};
    return {Reason::NONE, Result::NONE};
}

chess::Board checkedBoard(const std::string &fen, bool variant960) {
    require(fen.size() < 256, "FEN trop longue.");
    std::istringstream input(fen);
    std::array<std::string, 6> f;
    for (auto &x : f)
        require(bool(input >> x), "Une FEN doit contenir six champs.");
    std::string extra;
    require(!(input >> extra), "Champs FEN supplémentaires.");
    require(f[1] == "w" || f[1] == "b", "Trait invalide dans la FEN.");
    std::array<char, 64> squares{};
    int whitePieces = 0, blackPieces = 0, whitePawns = 0, blackPawns = 0;
    int rank = 7, file = 0, whiteKing = 0, blackKing = 0;
    for (char c : f[0]) {
        if (c == '/') {
            require(file == 8 && rank > 0, "Rangée FEN invalide.");
            --rank;
            file = 0;
        } else if (c >= '1' && c <= '8') {
            file += c - '0';
            require(file <= 8, "Rangée FEN trop longue.");
        } else {
            require(std::string("prnbqkPRNBQK").find(c) != std::string::npos && file < 8,
                    "Pièce FEN invalide.");
            require((c != 'p' && c != 'P') || (rank != 0 && rank != 7), "Pion sur une rangée de promotion.");
            squares[rank * 8 + file++] = c;
            whiteKing += c == 'K';
            blackKing += c == 'k';
            if (std::isupper(static_cast<unsigned char>(c)))
                ++whitePieces;
            else
                ++blackPieces;
            whitePawns += c == 'P';
            blackPawns += c == 'p';
        }
    }
    require(rank == 0 && file == 8 && whiteKing == 1 && blackKing == 1,
            "La position doit comporter huit rangées et un roi par couleur.");
    require(whitePieces <= 16 && blackPieces <= 16 && whitePawns <= 8 && blackPawns <= 8,
            "Trop de pièces ou de pions dans la position.");
    // std::regex bâtit son automate à la construction : la refaire à chaque position
    // validée revient à la rebâtir des millions de fois au cours d'un import. Statiques,
    // elles sont construites une fois ; regex_match ne les modifie pas, donc les fils
    // de lecture peuvent les partager.
    static const std::regex classicCastling("K?Q?k?q?"), chess960Castling("[A-HKQa-hkq]{1,4}");
    require(f[2] == "-" || std::regex_match(f[2], variant960 ? chess960Castling : classicCastling),
            "Droits de roque invalides.");
    if (variant960 && f[2] != "-") {
        std::string seen;
        int wings = 0;
        for (char c : f[2]) {
            const bool white = std::isupper(static_cast<unsigned char>(c));
            const int offset = white ? 0 : 56;
            const char king = white ? 'K' : 'k', rook = white ? 'R' : 'r';
            auto first = squares.begin() + offset;
            auto k = std::find(first, first + 8, king);
            require(k != first + 8, "Roi hors de sa rangée de roque.");
            int rookFile = std::toupper(static_cast<unsigned char>(c)) - 'A';
            if (c == 'K' || c == 'k') {
                rookFile = 7;
                while (rookFile > k - first && squares[offset + rookFile] != rook)
                    --rookFile;
                require(rookFile > k - first, "Tour de petit roque absente.");
            } else if (c == 'Q' || c == 'q') {
                rookFile = 0;
                while (rookFile < k - first && squares[offset + rookFile] != rook)
                    ++rookFile;
                require(rookFile < k - first, "Tour de grand roque absente.");
            }
            require(rookFile >= 0 && rookFile < 8 && squares[offset + rookFile] == rook,
                    "Tour de roque incohérente.");
            const int wing = (white ? 0 : 2) + (rookFile > k - first ? 1 : 0);
            require(!(wings & (1 << wing)), "Deux droits de roque du même côté du roi.");
            wings |= 1 << wing;
            char key = char(offset + rookFile);
            require(seen.find(key) == std::string::npos, "Droit de roque dupliqué.");
            seen += key;
        }
    }
    for (char c : variant960 ? std::string() : f[2]) {
        if (c == 'K')
            require(squares[4] == 'K' && squares[7] == 'R', "Roque blanc incohérent.");
        if (c == 'Q')
            require(squares[4] == 'K' && squares[0] == 'R', "Grand roque blanc incohérent.");
        if (c == 'k')
            require(squares[60] == 'k' && squares[63] == 'r', "Roque noir incohérent.");
        if (c == 'q')
            require(squares[60] == 'k' && squares[56] == 'r', "Grand roque noir incohérent.");
    }
    if (f[3] != "-") {
        require(f[3].size() == 2 && f[3][0] >= 'a' && f[3][0] <= 'h' && f[3][1] == (f[1] == "w" ? '6' : '3'),
                "Case en passant invalide.");
        const int ep = (f[3][1] - '1') * 8 + f[3][0] - 'a';
        require(!squares[ep] && squares[ep + (f[1] == "w" ? -8 : 8)] == (f[1] == "w" ? 'p' : 'P'),
                "Prise en passant incohérente.");
    }
    for (int i : {4, 5}) {
        require(!f[i].empty() && f[i].size() < 6 &&
                    std::all_of(f[i].begin(), f[i].end(), [](unsigned char c) { return std::isdigit(c); }),
                "Compteurs FEN invalides.");
        const auto value = std::stoi(f[i]);
        require(i == 4 ? value <= 255 : (value >= 1 && value <= 32000), "Compteur FEN hors limites.");
    }
    chess::Board board(chess::constants::STARTPOS, variant960);
    require(board.setFen(fen), "FEN invalide.");
    require(!board.isAttacked(board.kingSq(~board.sideToMove()), board.sideToMove()),
            "Le roi du camp qui vient de jouer est en échec.");
    return board;
}

Game::Game(const std::string &fen, bool variant960) : chess960(variant960) {
    const auto b = checkedBoard(fen, chess960);
    tags = {{"Event", "Étude Trait64"}, {"Site", "?"},      {"Date", "????.??.??"}, {"Round", "?"},
            {"White", "Blancs"},        {"Black", "Noirs"}, {"Result", "*"}};
    if (chess960)
        tags["Variant"] = "Chess960";
    Node root;
    root.fen = b.getFen();
    root.hash = b.hash();
    nodes.push_back(root);
    if (chess960 || root.fen != chess::constants::STARTPOS) {
        tags["SetUp"] = "1";
        tags["FEN"] = root.fen;
    }
}

chess::Board Game::boardAt(int node) const {
    require(node >= 0 && node < int(nodes.size()), "Position introuvable.");
    std::vector<int> path;
    for (int n = node; n > 0; n = nodes[n].parent)
        path.push_back(n);
    auto board = checkedBoard(nodes[0].fen, chess960);
    for (auto it = path.rbegin(); it != path.rend(); ++it)
        board.makeMove(chess::uci::uciToMove(board, nodes[*it].uci));
    return board;
}

int Game::play(int parent, const std::string &uci) {
    require(parent >= 0 && parent < int(nodes.size()), "Position parente introuvable.");
    for (int c : nodes[parent].children)
        if (nodes[c].uci == uci)
            return c;
    require(nodes.size() < 100000, "Limite de 100 000 positions par étude atteinte.");
    auto board = checkedBoard(nodes[parent].fen, chess960);
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    for (auto m : moves) {
        if (chess::uci::moveToUci(m, chess960) != uci)
            continue;
        Node n;
        n.parent = parent;
        n.uci = uci;
        n.san = chess::uci::moveToSan(board, m);
        board.makeMove(m);
        checkBoardCounters(board);
        n.fen = board.getFen();
        n.hash = board.hash();
        const int id = int(nodes.size());
        nodes[parent].children.push_back(id);
        nodes.push_back(std::move(n));
        return id;
    }
    throw std::runtime_error("Coup illégal : " + uci);
}

int Game::playSan(int parent, const std::string &san) {
    auto board = checkedBoard(nodes.at(parent).fen, chess960);
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    auto target = san;
    std::replace(target.begin(), target.end(), '0', 'O');
    target = stripCheck(target);
    for (auto m : moves)
        if (stripCheck(chess::uci::moveToSan(board, m)) == target)
            return play(parent, chess::uci::moveToUci(m, chess960));
    // Historical PGN exports sometimes include an unnecessary origin, e.g. Rd1d2.
    // Accept it only when it identifies exactly one legal move, with the correct
    // capture marker. Pawns, promotions and castling keep their canonical syntax.
    std::string candidate;
    if (!target.empty() && std::string("KQRBN").find(target.front()) != std::string::npos) {
        for (auto m : moves) {
            const auto canonical = stripCheck(chess::uci::moveToSan(board, m));
            if (canonical.front() != target.front() || m.typeOf() == chess::Move::CASTLING)
                continue;
            const auto origin = std::string(m.from());
            const auto suffix = (board.isCapture(m) ? "x" : "") + std::string(m.to());
            const auto piece = target.substr(0, 1);
            if (target != piece + origin + suffix && target != piece + origin.substr(0, 1) + suffix &&
                target != piece + origin.substr(1, 1) + suffix)
                continue;
            require(candidate.empty(), "Coup PGN ambigu : " + san);
            candidate = chess::uci::moveToUci(m, chess960);
        }
    }
    if (!candidate.empty())
        return play(parent, candidate);
    throw std::runtime_error("Coup PGN invalide : " + san + " (position " + std::to_string(parent) + ")");
}

std::vector<int> Game::mainline() const {
    std::vector<int> out{0};
    while (!nodes[out.back()].children.empty())
        out.push_back(nodes[out.back()].children[0]);
    return out;
}

void Game::promote(int node) {
    require(node > 0 && node < int(nodes.size()), "Sélectionnez un coup.");
    auto &children = nodes[nodes[node].parent].children;
    const auto it = std::find(children.begin(), children.end(), node);
    require(it != children.end(), "Variante supprimée.");
    std::rotate(children.begin(), it, it + 1);
}
void Game::removeVariation(int node) {
    require(node > 0 && node < int(nodes.size()), "La position initiale ne peut pas être supprimée.");
    auto &children = nodes[nodes[node].parent].children;
    children.erase(std::remove(children.begin(), children.end(), node), children.end());
}

std::string Game::pgn() const {
    std::ostringstream out;
    const std::vector<std::string> roster{"Event", "Site", "Date", "Round", "White", "Black", "Result"};
    for (const auto &key : roster)
        out << '[' << key << " \"" << quote(tags.contains(key) ? tags.at(key) : "?") << "\"]\n";
    for (const auto &[key, value] : tags)
        if (std::find(roster.begin(), roster.end(), key) == roster.end())
            out << '[' << key << " \"" << quote(value) << "\"]\n";
    out << '\n';
    if (!nodes[0].comment.empty())
        out << '{' << annotation(nodes[0].comment) << "} ";
    std::function<void(int, int)> line = [&](int node, int depth) {
        require(depth <= 128, "Trop de niveaux de variantes.");
        while (node > 0) {
            const auto &n = nodes[node];
            const auto b = checkedBoard(nodes[n.parent].fen, chess960);
            out << b.fullMoveNumber() << (b.sideToMove() == chess::Color::WHITE ? ". " : "... ") << n.san
                << ' ';
            for (int nag : n.nags)
                out << '$' << nag << ' ';
            if (!n.comment.empty())
                out << '{' << annotation(n.comment) << "} ";
            const auto &siblings = nodes[n.parent].children;
            if (!siblings.empty() && siblings[0] == node)
                for (size_t i = 1; i < siblings.size(); ++i) {
                    out << "( ";
                    line(siblings[i], depth + 1);
                    out << ") ";
                }
            node = n.children.empty() ? -1 : n.children[0];
        }
    };
    if (!nodes[0].children.empty())
        line(nodes[0].children[0], 0);
    out << (tags.contains("Result") && result(tags.at("Result")) ? tags.at("Result") : "*") << "\n\n";
    return out.str();
}

std::vector<Game> parsePgn(const std::string &text, size_t maxGames,
                           const std::function<void()> &checkpoint) {
    if (checkpoint)
        checkpoint();
    require(text.size() <= 64 * 1024 * 1024, "PGN limité à 64 Mio par import.");
    std::vector<Game> games;
    Game game;
    std::map<std::string, std::string> headers;
    bool started = false, touched = false;
    int node = 0;
    std::vector<int> stack;
    auto start = [&] {
        if (started)
            return;
        if (headers.contains("Variant"))
            require(headers["Variant"] == "Standard" || headers["Variant"] == "From Position" ||
                        headers["Variant"] == "Chess" || headers["Variant"] == "Chess960" ||
                        headers["Variant"] == "Fischerandom",
                    "Variante non prise en charge : " + headers["Variant"]);
        const bool frc = headers.contains("Variant") &&
                         (headers["Variant"] == "Chess960" || headers["Variant"] == "Fischerandom");
        require(!frc || headers.contains("FEN"), "Une partie Chess960 doit préciser sa FEN initiale.");
        game = Game(headers.contains("FEN") ? headers["FEN"] : std::string(chess::constants::STARTPOS), frc);
        for (const auto &[k, v] : headers)
            game.tags[k] = v;
        started = true;
    };
    auto finish = [&] {
        require(stack.empty(), "Variante PGN non fermée.");
        if (!touched)
            return;
        start();
        require(games.size() < maxGames, "Trop de parties dans cet import.");
        games.push_back(std::move(game));
        game = Game();
        headers.clear();
        started = touched = false;
        node = 0;
    };
    size_t i = text.starts_with("\xEF\xBB\xBF") ? 3 : 0;
    while (i < text.size()) {
        if (checkpoint)
            checkpoint();
        char c = text[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }
        if (c == '%' && (i == 0 || text[i - 1] == '\n')) {
            while (i < text.size() && text[i] != '\n')
                ++i;
            continue;
        }
        if (c == '[') {
            if (started)
                finish();
            ++i;
            while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])))
                ++i;
            std::string key, value;
            while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_'))
                key += text[i++];
            require(!key.empty(), "Nom de balise PGN absent.");
            while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])))
                ++i;
            require(i < text.size() && text[i++] == '"', "Valeur PGN sans guillemet.");
            bool closed = false;
            while (i < text.size()) {
                char v = text[i++];
                if (v == '"') {
                    closed = true;
                    break;
                }
                if (v == '\\' && i < text.size())
                    v = text[i++];
                value += v;
            }
            require(closed, "Balise PGN non fermée.");
            while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])))
                ++i;
            require(i < text.size() && text[i++] == ']', "Balise PGN sans crochet fermant.");
            headers[key] = value;
            touched = true;
            continue;
        }
        touched = true;
        start();
        if (c == '{' || c == ';') {
            ++i;
            std::string comment;
            while (i < text.size() && text[i] != (c == '{' ? '}' : '\n'))
                comment += text[i++];
            if (c == '{')
                require(i < text.size(), "Commentaire PGN non fermé.");
            if (i < text.size())
                ++i;
            if (!game.nodes[node].comment.empty())
                game.nodes[node].comment += '\n';
            game.nodes[node].comment += comment;
            continue;
        }
        if (c == '(') {
            require(node != 0 && stack.size() < 128, "Variante PGN mal placée ou trop profonde.");
            stack.push_back(node);
            node = game.nodes[node].parent;
            ++i;
            continue;
        }
        if (c == ')') {
            require(!stack.empty(), "Parenthèse PGN inattendue.");
            node = stack.back();
            stack.pop_back();
            ++i;
            continue;
        }
        require(c != '}' && c != ']', "Délimiteur PGN inattendu.");
        std::string token;
        while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i])) &&
               std::string("{}();[]").find(text[i]) == std::string::npos)
            token += text[i++];
        require(!token.empty(), "Texte PGN inattendu.");
        if (result(token)) {
            if (stack.empty()) {
                game.tags["Result"] = token;
                finish();
            }
            continue;
        }
        if (token[0] == '$') {
            static const std::regex nagPattern("\\$[0-9]{1,3}");
            require(std::regex_match(token, nagPattern), "NAG invalide.");
            int nag = std::stoi(token.substr(1));
            require(nag <= 255, "NAG hors limites.");
            game.nodes[node].nags.push_back(nag);
            continue;
        }
        size_t j = 0;
        while (j < token.size() && std::isdigit(static_cast<unsigned char>(token[j])))
            ++j;
        if (j < token.size() && token[j] == '.') {
            while (j < token.size() && token[j] == '.')
                ++j;
            token.erase(0, j);
        }
        if (token.empty() || token == "..." || token == "e.p.")
            continue;
        std::string suffix;
        while (!token.empty() && (token.back() == '!' || token.back() == '?')) {
            suffix = token.back() + suffix;
            token.pop_back();
        }
        static const std::map<std::string, int> nags{{"!", 1},  {"?", 2},  {"!!", 3},
                                                     {"??", 4}, {"!?", 5}, {"?!", 6}};
        if (!token.empty())
            node = game.playSan(node, token);
        if (nags.contains(suffix))
            game.nodes[node].nags.push_back(nags.at(suffix));
    }
    finish();
    return games;
}

std::string positionKey(const std::string &fen) {
    std::istringstream in(fen);
    std::string out, field;
    for (int i = 0; i < 4 && in >> field; ++i) {
        if (i)
            out += ' ';
        out += field;
    }
    return out;
}
std::uint64_t positionHash(const std::string &fen) {
    std::istringstream in(fen);
    std::array<std::string, 6> field{"", "w", "-", "-", "0", "1"};
    for (auto &value : field)
        if (!(in >> value)) break;
    const bool variant960 = field[2].find_first_not_of("KQkq-") != std::string::npos;
    const chess::Board board(field[0] + ' ' + field[1] + ' ' + field[2] + ' ' + field[3] + ' ' + field[4] + ' ' +
                                 field[5],
                             variant960);
    return board.hash();
}
std::uint64_t pawnKey(const std::string &fen) {
    // FNV-1a over the squares of white and black pawns, read straight from the FEN placement.
    std::uint64_t key = 1469598103934665603ULL;
    int square = 56; // a8, the first square of a FEN
    for (const char c : fen) {
        if (c == ' ') break;
        if (c == '/') square -= 16;
        else if (c >= '1' && c <= '8') square += c - '0';
        else {
            if (c == 'P' || c == 'p') {
                key ^= std::uint64_t(square << 1 | (c == 'p'));
                key *= 1099511628211ULL;
            }
            ++square;
        }
    }
    return key;
}
uint64_t perft(chess::Board &board, int depth) {
    if (depth <= 0)
        return 1;
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    if (depth == 1)
        return moves.size();
    uint64_t n = 0;
    for (const auto m : moves) {
        board.makeMove(m);
        n += perft(board, depth - 1);
        board.unmakeMove(m);
    }
    return n;
}
} // namespace t64
