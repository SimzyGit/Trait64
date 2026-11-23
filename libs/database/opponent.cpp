// SPDX-License-Identifier: AGPL-3.0-or-later
// Préparer un adversaire : ses parties rassemblées sous une base à son nom, son arbre de
// coups par couleur, et les ouvertures où il marque peu.
#include "database.h"
#include "names.h"
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>
#include <stdexcept>

namespace t64 {
namespace {
void need(bool ok, const QString &message) {
    if (!ok)
        throw std::runtime_error(message.toStdString());
}
QString marks(qsizetype count) {
    QStringList list;
    for (qsizetype i = 0; i < count; ++i)
        list << "?";
    return list.join(',');
}
// Les points du joueur dans une partie, -1 si elle n'est pas finie.
double pointsFor(const QString &result, bool white) {
    if (result == "1-0")
        return white ? 1 : 0;
    if (result == "0-1")
        return white ? 0 : 1;
    return result == "1/2-1/2" ? .5 : -1;
}
} // namespace

QStringList Database::playerSpellings(const QString &name) const {
    // Les graphies d'un même joueur dans la bibliothèque : « Carlsen, Magnus », « Carlsen,M. »,
    // « Magnus Carlsen ». On part du nom de famille, puis la clé de nom tranche.
    const auto key = nameKey(name, true);
    if (key.isEmpty())
        return {};
    const auto surname = key.section('|', 0, 0);
    QSqlQuery q(db_);
    q.prepare(
        "SELECT DISTINCT white FROM games WHERE deleted=0 AND white LIKE ? UNION SELECT DISTINCT black FROM "
        "games WHERE deleted=0 AND black LIKE ?");
    q.addBindValue("%" + surname + "%");
    q.addBindValue("%" + surname + "%");
    need(q.exec(), q.lastError().text());
    QStringList out{name};
    while (q.next()) {
        const auto found = q.value(0).toString();
        if (!out.contains(found, Qt::CaseInsensitive) && nameKey(found, true) == key)
            out << found;
    }
    return out;
}

qint64 Database::collectPlayerGames(const QVariantMap &source, const QStringList &names) {
    const auto id = registerSource(source);
    need(id > 0, "Base de préparation impossible à créer.");
    if (names.isEmpty())
        return id;
    QSqlQuery q(db_);
    q.prepare("SELECT id FROM games WHERE deleted=0 AND (white COLLATE NOCASE IN (" + marks(names.size()) +
              ") OR black COLLATE NOCASE IN (" + marks(names.size()) + "))");
    for (int i = 0; i < 2; ++i)
        for (const auto &n : names)
            q.addBindValue(n);
    need(q.exec(), q.lastError().text());
    QList<qint64> games;
    while (q.next())
        games << q.value(0).toLongLong();
    q.finish();
    sql("SAVEPOINT collect_player");
    try {
        for (auto game : games)
            linkSource(id, game);
        sql("RELEASE collect_player");
    } catch (...) {
        sql("ROLLBACK TO collect_player");
        sql("RELEASE collect_player");
        throw;
    }
    return id;
}

QVariantMap Database::opponentTree(const QStringList &names, bool white, const QStringList &path) const {
    Game game;
    int node = 0;
    for (const auto &uci : path)
        node = game.play(node, uci.toStdString());
    const auto key = nodeKey(game.nodes[node]);
    QVariantMap out{
        {"fen", QString::fromStdString(game.nodes[node].fen)}, {"moves", QVariantList()}, {"games", 0}};
    if (names.isEmpty())
        return out;
    const QString column = white ? "g.white" : "g.black";
    QSqlQuery q(db_);
    q.setForwardOnly(true);
    q.prepare(
        "SELECT p.game_id,p.uci,p.san,g.result,g.date FROM positions p JOIN games g ON g.id=p.game_id WHERE "
        "p.key=? AND g.deleted=0 AND " +
        column + " COLLATE NOCASE IN (" + marks(names.size()) + ") ORDER BY p.game_id,p.ply");
    q.addBindValue(key);
    for (const auto &n : names)
        q.addBindValue(n);
    need(q.exec(), q.lastError().text());
    struct Move {
        QString san, last;
        int games = 0, finished = 0;
        double points = 0;
    };
    QHash<QString, Move> moves;
    qint64 previous = -1;
    int games = 0, finished = 0;
    double points = 0;
    while (q.next()) {
        const auto id = q.value(0).toLongLong();
        if (id == previous) // une partie qui repasse par la position ne compte qu'une fois
            continue;
        previous = id;
        const double p = pointsFor(q.value(3).toString(), white);
        const auto date = q.value(4).toString();
        ++games;
        if (p >= 0)
            ++finished, points += p;
        const auto uci = q.value(1).toString();
        if (uci.isEmpty())
            continue;
        auto &move = moves[uci];
        move.san = q.value(2).toString();
        ++move.games;
        if (p >= 0)
            ++move.finished, move.points += p;
        if (!date.contains('?') && date > move.last)
            move.last = date;
    }
    QVariantList list;
    for (auto it = moves.cbegin(); it != moves.cend(); ++it)
        list << QVariantMap{{"uci", it.key()},
                            {"san", it->san},
                            {"games", it->games},
                            {"score", it->finished ? qRound(100 * it->points / it->finished) : -1},
                            {"last", it->last}};
    std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()["games"].toInt() > b.toMap()["games"].toInt();
    });
    out["moves"] = list;
    out["games"] = games;
    out["score"] = finished ? qRound(100 * points / finished) : -1;
    // C'est à lui de jouer quand le camp au trait est sa couleur.
    out["hisTurn"] = (game.nodes[node].fen.find(" w ") != std::string::npos) == white;
    return out;
}

QVariantList Database::opponentWeaknesses(const QStringList &names, int minimum) const {
    if (names.isEmpty())
        return {};
    QSqlQuery q(db_);
    q.prepare(
        "SELECT white,result,eco FROM games WHERE deleted=0 AND eco<>'' AND (white COLLATE NOCASE IN (" +
        marks(names.size()) + ") OR black COLLATE NOCASE IN (" + marks(names.size()) + "))");
    for (int i = 0; i < 2; ++i)
        for (const auto &n : names)
            q.addBindValue(n);
    need(q.exec(), q.lastError().text());
    struct Tally {
        int games = 0;
        double points = 0;
    };
    QMap<QPair<bool, QString>, Tally> tallies;
    while (q.next()) {
        const bool white = names.contains(q.value(0).toString(), Qt::CaseInsensitive);
        const double p = pointsFor(q.value(1).toString(), white);
        if (p < 0)
            continue;
        auto &t = tallies[{white, q.value(2).toString().left(3)}];
        ++t.games;
        t.points += p;
    }
    // Ce qu'on observe, rien d'autre : une ouverture jouée au moins `minimum` fois où il
    // marque moins de 40 %, la plus faible d'abord.
    QVariantList out;
    for (auto it = tallies.cbegin(); it != tallies.cend(); ++it) {
        const int score = qRound(100 * it->points / it->games);
        if (it->games >= minimum && score < 40)
            out << QVariantMap{
                {"white", it.key().first}, {"eco", it.key().second}, {"games", it->games}, {"score", score}};
    }
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        const auto x = a.toMap(), y = b.toMap();
        return x["score"].toInt() != y["score"].toInt() ? x["score"].toInt() < y["score"].toInt()
                                                        : x["games"].toInt() > y["games"].toInt();
    });
    return out;
}
} // namespace t64
