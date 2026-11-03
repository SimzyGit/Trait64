// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "game.h"
#include <QVariant>
#include <functional>

namespace t64 {
// Presentation only: SAN stored in the game and PGN exports stay standard.
// notation: 0 = English letters, 1 = French letters (a bool converts), 2 = figurines.
enum Notation { EnglishLetters = 0, FrenchLetters = 1, Figurines = 2 };
inline QString displaySan(const std::string &san, int notation) {
    QString text = QString::fromStdString(san);
    if (notation == EnglishLetters) return text;
    static const QString pieces = "NBRQK", french = "CFTDR";
    static const QStringList figurines{"♘", "♗", "♖", "♕", "♔"};
    for (int i = int(text.size()) - 1; i >= 0; --i) {
        if (i != 0 && text[i - 1] != '=') continue;
        const auto piece = pieces.indexOf(text[i]);
        if (piece < 0) continue;
        if (notation == Figurines) text.replace(i, 1, figurines[piece]);
        else text[i] = french[piece];
    }
    return text;
}

inline QVariantList moveRows(const Game &game, int current, int french) {
    QVariantList rows;
    rows << QVariantMap{{"initial", true}, {"depth", 0}, {"active", current == 0},
                        {"comment", QString::fromStdString(game.nodes[0].comment)}};
    auto entry = [&](int id) {
        const auto &node = game.nodes[id];
        QString text = displaySan(node.san, french);
        static const QStringList symbols{"", "!", "?", "!!", "??", "!?", "?!"};
        for (int nag : node.nags)
            text += " " + (nag >= 0 && nag < symbols.size() ? symbols[nag] : "$" + QString::number(nag));
        return QVariantMap{{"id", id}, {"parent", node.parent}, {"text", text},
                           {"active", current == id}, {"comment", QString::fromStdString(node.comment)}};
    };
    int visited = 0;
    // Each recursive call owns one branch. Pair only its direct mainline child;
    // siblings are emitted as separate blocks after the complete parent row.
    std::function<void(int, int)> line = [&](int id, int depth) {
        if (depth > 128) return;
        while (id > 0 && visited < 10000) {
            const auto &node = game.nodes[id];
            const auto board = game.boardAt(node.parent);
            const int number = board.fullMoveNumber();
            const bool white = board.sideToMove() == chess::Color::WHITE;
            int last = id;
            QVariantMap row{{"initial", false}, {"depth", depth}, {"number", number},
                            {"white", QVariantMap()}, {"black", QVariantMap()}};
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
                    for (size_t k = 1; k < siblings.size(); ++k) line(siblings[k], depth + 1);
            };
            variations(id);
            if (last != id) variations(last);
            const auto &children = game.nodes[last].children;
            id = children.empty() ? -1 : children[0];
        }
    };
    if (!game.nodes[0].children.empty()) line(game.nodes[0].children[0], 0);
    return rows;
}

// Continuous notation in PGN order: "1. e4 e5 (1... c5 2. Nf3) 2. Nf3".
// Tokens: "move" (id, number, text), "open"/"close" around a variation, "comment".
inline QVariantList moveFlow(const Game &game, int current, int french) {
    QVariantList tokens;
    static const QStringList symbols{"", "!", "?", "!!", "??", "!?", "?!"};
    auto comment = [&](int id, int depth) {
        const auto &text = game.nodes[id].comment;
        if (text.empty()) return false;
        tokens << QVariantMap{{"kind", "comment"}, {"id", id}, {"depth", depth},
                              {"text", QString::fromStdString(text)}};
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
                text += nag >= 0 && nag < symbols.size() ? symbols[nag] : " $" + QString::number(nag);
            const QString number = white ? QString::number(board.fullMoveNumber()) + "."
                                   : numbered ? QString::number(board.fullMoveNumber()) + "…" : QString();
            tokens << QVariantMap{{"kind", "move"}, {"id", id}, {"depth", depth}, {"number", number},
                                  {"text", text}, {"active", current == id}};
            numbered = comment(id, depth);
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
    if (!game.nodes[0].children.empty()) line(game.nodes[0].children.front(), 0);
    return tokens;
}
} // namespace t64
