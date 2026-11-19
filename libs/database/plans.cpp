// SPDX-License-Identifier: AGPL-3.0-or-later
// L'explorateur de plans : dans les parties de la bibliothèque qui ont eu la structure de
// pions de l'échiquier, ce que chaque camp a fait de ses pièces tant qu'elle a tenu, et la
// poussée de pion qui l'a changée. Une manœuvre compte une fois par partie ; son score est
// celui du camp qui l'a jouée. Les positions semblables suivent : même matériel, et la
// ressemblance mesurée case par case.
#include "cancellation.h"
#include "database.h"
#include <QHash>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace t64 {
namespace {
// Une pièce et sa case d'arrivée (« Fd3 » s'écrit « Bd3 » ici, la notation choisie se
// fait à l'affichage) ; un roque tel quel. Vide pour un coup de pion.
QString pieceTarget(const std::string &san) {
    if (san.rfind("O-O", 0) == 0)
        return QString::fromStdString(san.substr(0, san.find_first_of("+#")));
    if (san.empty() || !std::strchr("NBRQK", san[0]))
        return {};
    std::string squares;
    for (char c : san)
        if ((c >= 'a' && c <= 'h') || (c >= '1' && c <= '8'))
            squares += c;
    return squares.size() >= 2
               ? QString(QChar(san[0])) + QString::fromStdString(squares.substr(squares.size() - 2))
               : QString();
}
} // namespace

QVariantMap Database::plans(const std::string &fen, int limit,
                            const std::shared_ptr<std::atomic_bool> &canceled) const {
    const qint64 structure = pawnsKey(fen);
    QSqlQuery q(db_);
    q.setForwardOnly(true);
    q.prepare("SELECT g.id,g.pgn,g.result,g.white_elo+g.black_elo FROM games g WHERE g.deleted=0 AND g.id IN("
              "SELECT game_id FROM game_signatures WHERE pawns=?) ORDER BY g.white_elo+g.black_elo DESC,g.id "
              "DESC LIMIT ?");
    q.addBindValue(structure);
    q.addBindValue(limit);
    if (!q.exec())
        throw std::runtime_error(q.lastError().text().toStdString());
    struct Tally {
        int games = 0;
        double points = 0;
        qint64 example = 0;
        int exampleElo = -1;
        bool rupture = false;
    };
    std::array<QHash<QString, Tally>, 2> sides;
    int games = 0;
    while (q.next()) {
        if (canceled && canceled->load())
            throw std::runtime_error("Recherche annulée.");
        std::vector<Game> parsed;
        try {
            parsed = parsePgn(q.value(1).toString().toStdString());
        } catch (const std::exception &) {
            continue;
        }
        if (parsed.size() != 1)
            continue;
        const auto &game = parsed.front();
        const auto line = game.mainline();
        size_t at = 0;
        while (at < line.size() && pawnsKey(game.nodes[line[at]].fen) != structure)
            ++at;
        if (at == line.size())
            continue;
        ++games;
        const auto result = q.value(2).toString();
        const qint64 id = q.value(0).toLongLong();
        const int elo = q.value(3).toInt();
        std::array<QSet<QString>, 2> seen;
        // Les coups qui suivent, tant que la structure tient ; le coup de pion qui la change
        // est la rupture, et l'exploration de cette partie s'arrête là.
        for (size_t i = at + 1; i < line.size() && i <= at + 40; ++i) {
            const auto &node = game.nodes[line[i]];
            const int side = game.boardOf(node.parent).sideToMove() == chess::Color::WHITE ? 0 : 1;
            const bool broken = pawnsKey(node.fen) != structure;
            QString key = pieceTarget(node.san);
            if (broken)
                key = QString::fromStdString(node.san.substr(0, node.san.find_first_of("+#")));
            if (!key.isEmpty() && !seen[side].contains(key)) {
                seen[side].insert(key);
                auto &tally = sides[side][key];
                ++tally.games;
                tally.points += result == "1/2-1/2" ? .5 : result == (side == 0 ? "1-0" : "0-1") ? 1 : 0;
                tally.rupture = tally.rupture || broken;
                if (elo > tally.exampleElo)
                    tally.example = id, tally.exampleElo = elo;
            }
            if (broken)
                break;
        }
    }
    QVariantMap out{{"games", games}};
    for (int side = 0; side < 2; ++side) {
        QVariantList rows;
        for (auto it = sides[side].cbegin(); it != sides[side].cend(); ++it)
            rows << QVariantMap{{"move", it.key()},
                                {"games", it->games},
                                {"share", games ? qRound(100. * it->games / games) : 0},
                                {"score", qRound(100. * it->points / it->games)},
                                {"rupture", it->rupture},
                                {"example", it->example}};
        std::sort(rows.begin(), rows.end(), [](const QVariant &a, const QVariant &b) {
            // Les plus jouées d'abord ; à égalité, les manœuvres avant les ruptures.
            const auto x = a.toMap(), y = b.toMap();
            if (x["games"].toInt() != y["games"].toInt())
                return x["games"].toInt() > y["games"].toInt();
            if (x["rupture"].toBool() != y["rupture"].toBool())
                return y["rupture"].toBool();
            return x["move"].toString() < y["move"].toString();
        });
        if (rows.size() > 12)
            rows.erase(rows.begin() + 12, rows.end());
        out[side == 0 ? "white" : "black"] = rows;
    }
    return out;
}
} // namespace t64

namespace t64 {
namespace {
// L'échiquier d'une FEN, case par case (a8 d'abord), pour comparer deux positions.
std::array<char, 64> squares(const std::string &fen) {
    std::array<char, 64> out;
    out.fill('.');
    int at = 0;
    for (const char c : fen) {
        if (c == ' ' || at >= 64)
            break;
        if (c == '/')
            continue;
        if (c >= '1' && c <= '8')
            at += c - '0';
        else
            out[size_t(at++)] = c;
    }
    return out;
}
} // namespace

QVariantList Database::similarPositions(const std::string &fen, int limit, int candidates,
                                        const std::shared_ptr<std::atomic_bool> &canceled) const {
    // Le même matériel, puis la ressemblance case par case : une pièce absente ou déplacée
    // compte un, un pion compte double (la structure décide du plan plus qu'une pièce).
    // L'index ne donne que les parties où ce matériel a été sur l'échiquier ; on les rejoue
    // et l'on garde, pour chacune, sa position la plus proche.
    const auto target = squares(fen);
    const auto code = materialCode(std::string_view(fen));
    const bool whiteToMove = fen.find(" b ") == std::string::npos;
    QSqlQuery q(db_);
    q.setForwardOnly(true);
    q.prepare("SELECT g.id,g.pgn,g.white,g.black,g.result,g.date,g.white_elo,g.black_elo FROM games g WHERE "
              "g.deleted=0 AND g.id IN(SELECT game_id FROM game_material WHERE material=?) ORDER BY "
              "g.white_elo+g.black_elo DESC,g.id DESC LIMIT ?");
    q.addBindValue(code);
    q.addBindValue(std::max(candidates, 1));
    if (!q.exec())
        throw std::runtime_error(q.lastError().text().toStdString());
    struct Hit {
        int distance;
        QVariantMap row;
    };
    std::vector<Hit> hits;
    while (q.next()) {
        if (canceled && canceled->load())
            throw std::runtime_error("Recherche annulée.");
        std::vector<Game> parsed;
        try {
            parsed = parsePgn(q.value(1).toString().toStdString());
        } catch (const std::exception &) {
            continue;
        }
        if (parsed.size() != 1)
            continue;
        const auto &game = parsed.front();
        int best = 13, bestNode = -1;
        for (const int n : game.mainline()) {
            const auto &position = game.nodes[n].fen;
            if ((position.find(" b ") == std::string::npos) != whiteToMove ||
                materialCode(std::string_view(position)) != code)
                continue;
            const auto here = squares(position);
            int distance = 0;
            for (size_t i = 0; i < 64 && distance < best; ++i)
                if (here[i] != target[i])
                    distance +=
                        (here[i] == 'P' || here[i] == 'p' || target[i] == 'P' || target[i] == 'p') ? 2 : 1;
            if (distance < best)
                best = distance, bestNode = n;
        }
        if (bestNode < 0)
            continue;
        hits.push_back({best,
                        {{"id", q.value(0)},
                         {"node", bestNode},
                         {"fen", QString::fromStdString(game.nodes[bestNode].fen)},
                         {"distance", best},
                         {"white", q.value(2)},
                         {"black", q.value(3)},
                         {"result", q.value(4)},
                         {"date", q.value(5)},
                         {"whiteElo", q.value(6)},
                         {"blackElo", q.value(7)}}});
    }
    // Les plus proches d'abord ; à égalité, l'ordre de la requête (les mieux classées).
    std::stable_sort(hits.begin(), hits.end(),
                     [](const Hit &a, const Hit &b) { return a.distance < b.distance; });
    QVariantList out;
    for (const auto &hit : hits) {
        if (out.size() >= limit)
            break;
        out << hit.row;
    }
    return out;
}
} // namespace t64
