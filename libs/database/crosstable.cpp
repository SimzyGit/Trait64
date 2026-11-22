// SPDX-License-Identifier: AGPL-3.0-or-later
#include "crosstable.h"
#include <QHash>
#include <QSet>
#include <QVariantList>
#include <algorithm>
#include <cmath>

namespace t64 {
namespace {
// Points du joueur dans une partie, -1 si elle n'est pas terminée.
double points(const QString &result, bool white) {
    if (result == "1-0")
        return white ? 1 : 0;
    if (result == "0-1")
        return white ? 0 : 1;
    if (result == "1/2-1/2")
        return 0.5;
    return -1;
}

QString symbol(double value) {
    return value == 1 ? "1" : value == 0 ? "0" : "½";
}

struct Player {
    QString name;
    int elo = 0;
    double score = 0, sonneborn = 0, buchholz = 0, expected = 0;
    int games = 0, wins = 0, losses = 0;
    qint64 ratedSum = 0;
    int rated = 0;
};
} // namespace

QVariantMap crosstable(const std::vector<CrossGame> &games) {
    QHash<QString, int> index;
    std::vector<Player> players;
    const auto at = [&](const QString &name, int elo) {
        auto found = index.find(name);
        if (found == index.end()) {
            found = index.insert(name, int(players.size()));
            players.push_back({name});
        }
        auto &player = players[size_t(*found)];
        player.elo = std::max(player.elo, elo); // l'Elo le plus récent manque souvent : le plus haut
        return *found;
    };
    struct Played {
        int white, black;
        double whitePoints;
        const CrossGame *game;
    };
    std::vector<Played> played;
    for (const auto &game : games) {
        if (game.white.trimmed().isEmpty() || game.black.trimmed().isEmpty() || game.white == game.black)
            continue;
        const int white = at(game.white, game.whiteElo), black = at(game.black, game.blackElo);
        played.push_back({white, black, points(game.result, true), &game});
    }
    for (const auto &p : played) {
        if (p.whitePoints < 0)
            continue;
        for (const bool white : {true, false}) {
            auto &me = players[size_t(white ? p.white : p.black)];
            const auto &them = players[size_t(white ? p.black : p.white)];
            const double mine = white ? p.whitePoints : 1 - p.whitePoints;
            me.score += mine;
            ++me.games;
            me.wins += mine == 1;
            me.losses += mine == 0;
            if (them.elo > 0) {
                me.ratedSum += them.elo;
                ++me.rated;
                if (me.elo > 0) // score attendu selon la formule Elo
                    me.expected += 1 / (1 + std::pow(10.0, (them.elo - me.elo) / 400.0));
            }
        }
    }
    // Le Sonneborn-Berger a besoin des totaux de tous : second passage.
    for (const auto &p : played) {
        if (p.whitePoints < 0)
            continue;
        players[size_t(p.white)].sonneborn += p.whitePoints * players[size_t(p.black)].score;
        players[size_t(p.black)].sonneborn += (1 - p.whitePoints) * players[size_t(p.white)].score;
        players[size_t(p.white)].buchholz += players[size_t(p.black)].score;
        players[size_t(p.black)].buchholz += players[size_t(p.white)].score;
    }
    std::vector<int> order(players.size());
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = int(i);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const auto &x = players[size_t(a)], &y = players[size_t(b)];
        if (x.score != y.score)
            return x.score > y.score;
        if (x.sonneborn != y.sonneborn)
            return x.sonneborn > y.sonneborn;
        if (x.wins != y.wins)
            return x.wins > y.wins;
        return x.name < y.name;
    });
    std::vector<int> rank(players.size());
    for (size_t r = 0; r < order.size(); ++r)
        rank[size_t(order[r])] = int(r) + 1;

    const int n = int(players.size());
    int rounds = 0;
    QSet<qint64> pairs;
    for (const auto &p : played) {
        rounds = std::max(rounds, p.game->round);
        pairs.insert(qint64(std::min(p.white, p.black)) * n + std::max(p.white, p.black));
    }
    const bool matrix = n >= 2 && n <= 20 && pairs.size() * 2 >= n * (n - 1) / 2;

    // Cases du tableau carré, dans l'ordre du classement, et lignes par ronde.
    const auto side = size_t(n);
    std::vector<std::vector<QString>> cells(side, std::vector<QString>(side));
    std::vector<std::vector<QVariantList>> cellGames(side, std::vector<QVariantList>(side));
    std::vector<std::vector<QString>> byRound(side, std::vector<QString>(size_t(rounds)));
    for (const auto &p : played) {
        for (const bool white : {true, false}) {
            const int me = white ? p.white : p.black, them = white ? p.black : p.white;
            const auto mark =
                p.whitePoints < 0 ? QString("*") : symbol(white ? p.whitePoints : 1 - p.whitePoints);
            cells[size_t(me)][size_t(them)] += mark;
            cellGames[size_t(me)][size_t(them)] << p.game->id;
            if (p.game->round > 0) {
                auto &entry = byRound[size_t(me)][size_t(p.game->round - 1)];
                if (!entry.isEmpty())
                    entry += ' ';
                entry += QString::number(rank[size_t(them)]) + (white ? "○" : "●") + mark;
            }
        }
    }
    QVariantList rows;
    for (const int i : order) {
        const auto &player = players[size_t(i)];
        QVariantList line, lineGames, lineRounds;
        for (const int j : order) {
            line << (i == j ? QString("×") : cells[size_t(i)][size_t(j)]);
            lineGames << QVariant(cellGames[size_t(i)][size_t(j)]);
        }
        for (const auto &entry : byRound[size_t(i)])
            lineRounds << (entry.isEmpty() ? QString("–") : entry);
        int performance = 0;
        if (player.rated > 0 && player.rated == player.games)
            performance = int(std::lround(double(player.ratedSum) / player.rated +
                                          400.0 * (player.wins - player.losses) / player.games));
        // Le gain d'Elo à la manière de la FIDE : K = 10 à partir de 2400, 20 en dessous,
        // sur les seules parties contre des adversaires classés.
        QVariant change;
        if (player.elo > 0 && player.rated == player.games && player.games > 0)
            change = std::round((player.elo >= 2400 ? 10 : 20) * (player.score - player.expected) * 10) / 10;
        rows << QVariantMap{
            {"rank", rank[size_t(i)]}, {"name", player.name},        {"elo", player.elo},
            {"score", player.score},   {"games", player.games},      {"sonneborn", player.sonneborn},
            {"wins", player.wins},     {"performance", performance}, {"cells", line},
            {"cellGames", lineGames},  {"rounds", lineRounds},       {"buchholz", player.buchholz},
            {"change", change}};
    }
    // Le tournoi d'un coup d'œil : moyenne Elo et catégorie FIDE (une par tranche de
    // 25 points à partir de 2251), puis la répartition des résultats.
    int whiteWins = 0, draws = 0, blackWins = 0, ratedPlayers = 0;
    qint64 eloSum = 0;
    for (const auto &p : played)
        whiteWins += p.whitePoints == 1, draws += p.whitePoints == 0.5, blackWins += p.whitePoints == 0;
    for (const auto &player : players)
        if (player.elo > 0)
            eloSum += player.elo, ++ratedPlayers;
    const int average = ratedPlayers ? int(eloSum / ratedPlayers) : 0;
    const int finished = whiteWins + draws + blackWins;
    const QVariantMap summary{{"games", finished},
                              {"whiteWins", whiteWins},
                              {"draws", draws},
                              {"blackWins", blackWins},
                              {"decisive", finished ? 100 * (whiteWins + blackWins) / finished : 0},
                              {"averageElo", average},
                              {"category", average >= 2251 ? (average - 2251) / 25 + 1 : 0}};
    return {{"players", rows}, {"rounds", rounds}, {"matrix", matrix}, {"summary", summary}};
}
} // namespace t64
