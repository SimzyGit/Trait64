// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "game.h"
#include <QRegularExpression>
#include <QVariant>
#include <algorithm>
#include <functional>

namespace t64 {
// Presentation only: SAN stored in the game and PGN exports stay standard.
// notation: 0 = English letters, 1 = French letters (a bool converts), 2 = figurines.
enum Notation { EnglishLetters = 0, FrenchLetters = 1, Figurines = 2 };
inline QString displaySan(const std::string &san, int notation) {
    QString text = QString::fromStdString(san);
    if (notation == EnglishLetters)
        return text;
    static const QString pieces = "NBRQK", french = "CFTDR";
    static const QStringList figurines{"♘", "♗", "♖", "♕", "♔"};
    for (int i = int(text.size()) - 1; i >= 0; --i) {
        if (i != 0 && text[i - 1] != '=')
            continue;
        const auto piece = pieces.indexOf(text[i]);
        if (piece < 0)
            continue;
        if (notation == Figurines)
            text.replace(i, 1, figurines[piece]);
        else
            text[i] = french[piece];
    }
    return text;
}

// Une manœuvre tapée dans la recherche : « Cf3-d2-f1 », « Nf3 d2 f1 », « f3-d2-f1 ». La
// pièce, en lettre anglaise (« P » pour un pion, vide pour n'importe laquelle), puis les
// cases traversées, deux au moins. Rien si le texte ne se lit pas ainsi.
inline QVariantMap parseManeuver(const QString &typed, int notation) {
    static const QRegularExpression shape(
        "^([KQRBNPCFTDkqrbnp])?\\s*([a-h][1-8](?:[\\s,\\-–]*[a-h][1-8])+)$");
    const auto found = shape.match(typed.trimmed());
    if (!found.hasMatch())
        return {};
    QString piece = found.captured(1).toUpper();
    static const QString pieces = "NBRQK", french = "CFTDR";
    if (notation != EnglishLetters && french.contains(piece))
        piece = pieces[french.indexOf(piece)];
    QStringList squares;
    static const QRegularExpression square("[a-h][1-8]");
    for (auto it = square.globalMatch(found.captured(2)); it.hasNext();)
        squares << it.next().captured();
    return {{"piece", piece}, {"squares", squares}};
}

// Les coups légaux qui répondent à ce que l'on tape au clavier : « Nf3 » ou « Cf3 » selon
// la notation, « nf3 », « exd5 » comme « ed5 », « O-O », « 0-0 » ou « oo », « e8=Q » comme
// « e8q », ou la forme UCI « e2e4 ». Les coups qui correspondent exactement passent avant
// ceux dont l'écriture commence par ce qui est tapé. Rendus en UCI.
inline QStringList typedMatches(const chess::Board &board, const QString &typed, int notation) {
    const auto plain = [](QString text) {
        text.remove(QRegularExpression("[x+#=!?:\\-\\s]"));
        text.replace('0', 'O');
        return text;
    };
    QString wanted = plain(typed.trimmed());
    if (wanted.isEmpty())
        return {};
    // « oo » et « ooo » : le roque, quelle que soit la casse.
    if (wanted.compare("OO", Qt::CaseInsensitive) == 0 || wanted.compare("OOO", Qt::CaseInsensitive) == 0)
        wanted = wanted.toUpper();
    // Lettres des pièces en français (C, F, T, D, R) ramenées à l'anglais. En tête du coup,
    // et en dernière place pour une promotion.
    static const QString pieces = "NBRQK", french = "CFTDR";
    // En tête, une minuscule qui est aussi une colonne (c, f, d) reste une colonne ; après
    // la rangée de promotion, elle ne peut être qu'une pièce.
    const auto translate = [&](int at, bool promotion) {
        if (notation != EnglishLetters && at >= 0 && at < wanted.size()) {
            const auto index = french.indexOf(wanted[at].toUpper());
            if (index >= 0 &&
                (promotion || wanted[at].isUpper() || !QStringLiteral("abcdefgh").contains(wanted[at])))
                wanted[at] = pieces[index];
        }
    };
    translate(0, false);
    if (wanted.size() > 2 && wanted[wanted.size() - 2].isDigit() &&
        !QRegularExpression("^[a-h][1-8][a-h][1-8]").match(wanted).hasMatch())
        translate(int(wanted.size()) - 1, true);
    // Une pièce tapée en minuscule (« nf3 ») : n, r, q, k ne sont pas des colonnes.
    if (QStringLiteral("nrqk").contains(wanted[0]))
        wanted[0] = wanted[0].toUpper();
    if (wanted.size() > 2 && wanted.back().isLower() && wanted[wanted.size() - 2].isDigit() &&
        QStringLiteral("nrqb").contains(wanted.back()) &&
        !QRegularExpression("^[a-h][1-8][a-h][1-8]").match(wanted).hasMatch())
        wanted.back() = wanted.back().toUpper();
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    QStringList exact, partial;
    for (const auto &move : moves) {
        const auto uci = QString::fromStdString(chess::uci::moveToUci(move));
        const auto san = plain(QString::fromStdString(chess::uci::moveToSan(board, move)));
        if (san == wanted || uci == typed.trimmed().toLower())
            exact << uci;
        else if (san.startsWith(wanted) || (wanted.size() >= 2 && uci.startsWith(typed.trimmed().toLower())))
            partial << uci;
    }
    return exact.isEmpty() ? partial : exact;
}

// Un diagramme après un coup : le NAG 220 de ChessBase, que d'autres logiciels lisent aussi.
constexpr int diagramNag = 220;
inline bool hasDiagram(const Node &node) {
    return std::find(node.nags.begin(), node.nags.end(), diagramNag) != node.nags.end();
}

// PGN command annotations ([%clk], [%eval], [%cal]…) are data, not prose: shown apart,
// never edited. The stored comment keeps them so PGN exports stay lossless.
struct CommentParts {
    QString text, clock, eval;
};
inline CommentParts splitComment(const std::string &raw) {
    static const QRegularExpression command("\\[%(\\w+)\\s+([^\\]]*)\\]");
    CommentParts parts;
    QString text = QString::fromStdString(raw);
    for (auto it = command.globalMatch(text); it.hasNext();) {
        const auto match = it.next();
        const auto name = match.captured(1), value = match.captured(2).trimmed();
        if (name == "clk") {
            // 0:02:59.4 -> 2:59 ; 1:05:00 stays 1:05:00.
            auto clock = value.section('.', 0, 0);
            while (clock.size() > 4 && (clock.startsWith('0') || clock.startsWith(':')))
                clock.remove(0, 1);
            parts.clock = clock;
        } else if (name == "eval") {
            bool ok = false;
            const double pawns = value.toDouble(&ok);
            parts.eval = value.startsWith('#') ? value
                         : ok                  ? (pawns > 0 ? "+" : "") + QString::number(pawns, 'f', 2)
                                               : value;
        }
    }
    text.remove(command);
    parts.text = text.simplified();
    return parts;
}

inline QVariantList moveRows(const Game &game, int current, int french) {
    QVariantList rows;
    rows << QVariantMap{{"initial", true},
                        {"depth", 0},
                        {"active", current == 0},
                        {"comment", splitComment(game.nodes[0].comment).text}};
    auto entry = [&](int id) {
        const auto &node = game.nodes[id];
        QString text = displaySan(node.san, french);
        static const QStringList symbols{"", "!", "?", "!!", "??", "!?", "?!"};
        for (int nag : node.nags)
            if (nag != diagramNag)
                text += " " + (nag >= 0 && nag < symbols.size() ? symbols[nag] : "$" + QString::number(nag));
        const auto parts = splitComment(node.comment);
        return QVariantMap{{"id", id},
                           {"parent", node.parent},
                           {"text", text},
                           {"active", current == id},
                           {"comment", parts.text},
                           {"clock", parts.clock},
                           {"eval", parts.eval},
                           {"diagram", hasDiagram(node) ? QString::fromStdString(node.fen) : QString()},
                           {"lastMove", QString::fromStdString(node.uci)}};
    };
    int visited = 0;
    // Each recursive call owns one branch. Pair only its direct mainline child;
    // siblings are emitted as separate blocks after the complete parent row.
    std::function<void(int, int)> line = [&](int id, int depth) {
        if (depth > 128)
            return;
        bool start = true;
        while (id > 0 && visited < 10000) {
            const auto &node = game.nodes[id];
            const auto board = game.boardAt(node.parent);
            const int number = board.fullMoveNumber();
            const bool white = board.sideToMove() == chess::Color::WHITE;
            int last = id;
            QVariantMap row{{"initial", false},       {"depth", depth},         {"number", number},
                            {"white", QVariantMap()}, {"black", QVariantMap()}, {"start", start}};
            start = false;
            row[white ? "white" : "black"] = entry(id);
            ++visited;
            if (white && !node.children.empty() && visited < 10000) {
                last = node.children[0];
                row["black"] = entry(last);
                ++visited;
            }
            rows << row;
            auto variations = [&](int move) {
                const auto &siblings = game.nodes[game.nodes[move].parent].children;
                if (!siblings.empty() && siblings[0] == move)
                    for (size_t k = 1; k < siblings.size(); ++k)
                        line(siblings[k], depth + 1);
            };
            variations(id);
            if (last != id)
                variations(last);
            const auto &children = game.nodes[last].children;
            id = children.empty() ? -1 : children[0];
        }
    };
    if (!game.nodes[0].children.empty())
        line(game.nodes[0].children[0], 0);
    return rows;
}

// Continuous notation in PGN order: "1. e4 e5 (1... c5 2. Nf3) 2. Nf3".
// Tokens: "move" (id, number, text), "open"/"close" around a variation, "comment".
inline QVariantList moveFlow(const Game &game, int current, int french) {
    QVariantList tokens;
    static const QStringList symbols{"", "!", "?", "!!", "??", "!?", "?!"};
    auto comment = [&](int id, int depth) {
        const auto text = splitComment(game.nodes[id].comment).text;
        if (text.isEmpty())
            return false;
        tokens << QVariantMap{{"kind", "comment"}, {"id", id}, {"depth", depth}, {"text", text}};
        return true;
    };
    comment(0, 0);
    int visited = 0;
    std::function<void(int, int)> line = [&](int id, int depth) {
        bool numbered = true; // a black move needs "N..." after a break in the flow
        while (id > 0 && visited++ < 10000) {
            const auto &node = game.nodes[id];
            const auto board = game.boardAt(node.parent);
            const bool white = board.sideToMove() == chess::Color::WHITE;
            QString text = displaySan(node.san, french);
            for (int nag : node.nags)
                if (nag != diagramNag)
                    text += nag >= 0 && nag < symbols.size() ? symbols[nag] : " $" + QString::number(nag);
            const QString number = white      ? QString::number(board.fullMoveNumber()) + "."
                                   : numbered ? QString::number(board.fullMoveNumber()) + "…"
                                              : QString();
            const auto parts = splitComment(node.comment);
            tokens << QVariantMap{{"kind", "move"},       {"id", id},          {"depth", depth},
                                  {"number", number},     {"text", text},      {"active", current == id},
                                  {"clock", parts.clock}, {"eval", parts.eval}};
            numbered = comment(id, depth);
            if (hasDiagram(node)) {
                tokens << QVariantMap{{"kind", "diagram"},
                                      {"id", id},
                                      {"depth", depth},
                                      {"fen", QString::fromStdString(node.fen)},
                                      {"lastMove", QString::fromStdString(node.uci)}};
                numbered = true;
            }
            const auto &siblings = game.nodes[node.parent].children;
            if (depth < 128 && siblings.front() == id)
                for (size_t k = 1; k < siblings.size(); ++k) {
                    tokens << QVariantMap{{"kind", "open"}, {"depth", depth + 1}};
                    line(siblings[k], depth + 1);
                    tokens << QVariantMap{{"kind", "close"}, {"depth", depth + 1}};
                    numbered = true;
                }
            id = node.children.empty() ? -1 : node.children.front();
        }
    };
    if (!game.nodes[0].children.empty())
        line(game.nodes[0].children.front(), 0);
    return tokens;
}
} // namespace t64
