// SPDX-License-Identifier: AGPL-3.0-or-later
#include "cancellation.h"
#include "database.h"
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>
#include <array>
#include <stdexcept>
namespace t64 {
namespace {
// Un joueur peut avoir les Blancs ou les Noirs : chaque note est prise du côté où il se
// trouvait. Les pseudos sont comparés sans égard à la casse, les sites n'en tenant pas
// compte non plus.
QString placeholders(int count) {
    QStringList marks;
    for (int i = 0; i < count; ++i)
        marks << "?";
    return marks.join(',');
}
void run(QSqlQuery &q) {
    if (!q.exec())
        throw std::runtime_error(q.lastError().text().toStdString());
}
void check(bool ok, const QString &message) {
    if (!ok)
        throw std::runtime_error(message.toStdString());
}
} // namespace
QVariantList Database::players() const {
    QSqlQuery q(db_);
    // Les noms des parties d'études (chapitres, titres de cours) ne sont pas des joueurs,
    // ni les noms de remplissage d'une étude neuve ou d'un PGN incomplet.
    q.prepare(
        "WITH study AS (SELECT name,COUNT(*) n FROM (SELECT g.white name FROM games g WHERE g.deleted=0 AND "
        "g.id "
        "IN(SELECT sg.game_id FROM source_games sg JOIN sources s ON s.id=sg.source_id WHERE "
        "s.purpose='study') UNION ALL SELECT g.black FROM games g WHERE g.deleted=0 AND g.id IN(SELECT "
        "sg.game_id FROM source_games sg JOIN sources s ON s.id=sg.source_id WHERE s.purpose='study')) GROUP "
        "BY "
        "name) "
        "SELECT pc.name,pc.games-COALESCE(st.n,0) real,MAX(COALESCE((SELECT MAX(white_elo) FROM games WHERE "
        "deleted=0 AND white=pc.name),0),COALESCE((SELECT MAX(black_elo) FROM games WHERE deleted=0 AND "
        "black=pc.name),0)) FROM player_counts pc LEFT JOIN study st ON st.name=pc.name WHERE "
        "pc.games-COALESCE(st.n,0)>0 AND trim(pc.name) NOT IN('','?','-','NN','N.N.','N.N','Blancs','Noirs',"
        "'White','Black') ORDER BY real DESC,pc.name LIMIT 1000");
    run(q);
    QVariantList result;
    while (q.next())
        result << QVariantMap{{"name", q.value(0)}, {"games", q.value(1)}, {"elo", q.value(2)}};
    return result;
}
QStringList Database::openingMoves(const QString &pgn, int plies) {
    QStringList moves;
    int depth = 0;
    bool brace = false, tag = false;
    QString token;
    const auto flush = [&] {
        if (token.isEmpty())
            return;
        // Numéros (« 12. », « 12... »), résultats et NAG ne sont pas des coups.
        static const QRegularExpression skip("^(\\d+\\.+|\\$\\d+|1-0|0-1|1/2-1/2|\\*)$");
        if (!skip.match(token).hasMatch() && moves.size() < plies) {
            auto move = token;
            move.remove(QRegularExpression("^\\d+\\.+"));
            move.remove(QRegularExpression("[!?]+$"));
            if (!move.isEmpty())
                moves << move;
        }
        token.clear();
    };
    for (const QChar c : pgn) {
        if (moves.size() >= plies)
            break;
        if (brace) {
            brace = c != '}';
            continue;
        }
        if (tag) {
            tag = c != ']';
            continue;
        }
        if (c == '{') {
            flush();
            brace = true;
        } else if (c == '[' && depth == 0 && token.isEmpty()) {
            tag = true;
        } else if (c == '(') {
            flush();
            ++depth;
        } else if (c == ')') {
            flush();
            depth = std::max(0, depth - 1);
        } else if (c.isSpace()) {
            flush();
        } else if (depth == 0)
            token += c;
    }
    flush();
    return moves;
}

QVariantMap Database::playerRepertoire(const QString &name, int plies, int lines) const {
    return playerRepertoire(QStringList{name}, plies, lines);
}
QVariantMap Database::playerRepertoire(const QStringList &names, int plies, int lines) const {
    if (names.isEmpty())
        return {{"white", QVariantList()}, {"black", QVariantList()}, {"years", QVariantList()}};
    const auto marks = placeholders(int(names.size()));
    QSqlQuery q(db_);
    q.prepare("SELECT white,result,date,pgn FROM games WHERE deleted=0 AND (white COLLATE NOCASE IN (" +
              marks + ") OR black COLLATE NOCASE IN (" + marks + "))");
    for (int i = 0; i < 2; ++i)
        for (const auto &n : names)
            q.addBindValue(n);
    run(q);
    struct Line {
        int games = 0;
        double points = 0;
    };
    QMap<QString, Line> asWhite, asBlack;
    // Par année : le premier coup avec les Blancs, la réponse au premier coup adverse avec
    // les Noirs (« 1.e4 c5 »).
    QMap<QString, QMap<QString, int>> whiteYears, blackYears;
    while (q.next()) {
        const bool white = names.contains(q.value(0).toString(), Qt::CaseInsensitive);
        const auto result = q.value(1).toString();
        const double points = result == "1-0"       ? (white ? 1 : 0)
                              : result == "0-1"     ? (white ? 0 : 1)
                              : result == "1/2-1/2" ? .5
                                                    : -1;
        const auto moves = openingMoves(q.value(3).toString(), plies);
        if (moves.isEmpty())
            continue;
        QStringList numbered;
        for (int i = 0; i < moves.size(); ++i)
            numbered << (i % 2 == 0 ? QString::number(i / 2 + 1) + "." + moves[i] : moves[i]);
        auto &line = (white ? asWhite : asBlack)[numbered.join(' ')];
        ++line.games;
        if (points >= 0)
            line.points += points;
        const auto year = q.value(2).toString().left(4);
        if (year.size() == 4 && !year.contains('?')) {
            if (white)
                ++whiteYears[year]["1." + moves[0]];
            else if (moves.size() >= 2)
                ++blackYears[year]["1." + moves[0] + " " + moves[1]];
        }
    }
    const auto top = [&](const QMap<QString, Line> &all) {
        QList<QPair<QString, Line>> sorted;
        for (auto it = all.cbegin(); it != all.cend(); ++it)
            sorted << qMakePair(it.key(), it.value());
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto &a, const auto &b) { return a.second.games > b.second.games; });
        QVariantList out;
        for (int i = 0; i < std::min(lines, int(sorted.size())); ++i)
            out << QVariantMap{{"line", sorted[i].first},
                               {"games", sorted[i].second.games},
                               {"score", qRound(100.0 * sorted[i].second.points / sorted[i].second.games)}};
        return out;
    };
    const auto favourite = [](const QMap<QString, int> &counts) {
        QString best;
        int total = 0;
        for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
            total += it.value();
            if (best.isEmpty() || it.value() > counts[best])
                best = it.key();
        }
        return best.isEmpty() ? QVariantMap()
                              : QVariantMap{{"move", best}, {"games", counts[best]}, {"total", total}};
    };
    QStringList years = whiteYears.keys() + blackYears.keys();
    years.removeDuplicates();
    std::sort(years.begin(), years.end());
    QVariantList evolution;
    for (const auto &year : years)
        evolution << QVariantMap{{"year", year},
                                 {"white", favourite(whiteYears.value(year))},
                                 {"black", favourite(blackYears.value(year))}};
    return {{"white", top(asWhite)}, {"black", top(asBlack)}, {"years", evolution}};
}

QVariantMap Database::player(const QString &name) const {
    QSqlQuery q(db_);
    q.prepare(
        "SELECT white,black,result,eco,date,white_elo,black_elo,id,plies FROM games WHERE deleted=0 AND "
        "id IN(SELECT id FROM games WHERE deleted=0 AND white=? UNION SELECT id FROM games WHERE "
        "deleted=0 AND black=?) ORDER BY date DESC,id DESC");
    q.addBindValue(name);
    q.addBindValue(name);
    run(q);
    int games = 0, wins = 0, draws = 0, losses = 0, unfinished = 0, whiteGames = 0, blackGames = 0,
        rating = 0;
    qint64 totalPlies = 0;
    QMap<QString, QVariantMap> openings;
    QVariantList recent, eloHistory;
    QHash<qint64, bool> colourOf;   // game id -> played white
    QHash<qint64, double> pointsOf; // game id -> points scored, finished games only
    // Game length as a proxy for the phase where it was decided.
    struct Phase {
        int wins = 0, draws = 0, losses = 0;
    };
    Phase phases[3];
    while (q.next()) {
        bool white = q.value(0).toString() == name;
        QString result = q.value(2).toString();
        ++games;
        white ? ++whiteGames : ++blackGames;
        bool won = (white && result == "1-0") || (!white && result == "0-1"),
             lost = (white && result == "0-1") || (!white && result == "1-0");
        if (won)
            ++wins;
        else if (lost)
            ++losses;
        else if (result == "1/2-1/2")
            ++draws;
        else
            ++unfinished;
        if (!rating)
            rating = q.value(white ? 5 : 6).toInt();
        totalPlies += q.value(8).toInt();
        const qint64 id = q.value(7).toLongLong();
        colourOf.insert(id, white);
        const bool finished = won || lost || result == "1/2-1/2";
        if (finished) {
            pointsOf.insert(id, won ? 1 : lost ? 0 : .5);
            const int plies = q.value(8).toInt();
            auto &phase = phases[plies <= 50 ? 0 : plies <= 100 ? 1 : 2];
            won ? ++phase.wins : lost ? ++phase.losses : ++phase.draws;
        }
        if (q.value(white ? 5 : 6).toInt() > 0 && eloHistory.size() < 200)
            eloHistory.prepend(QVariantMap{{"date", q.value(4)}, {"elo", q.value(white ? 5 : 6)}});
        QString key = (white ? "Blancs · " : "Noirs · ") + q.value(3).toString();
        auto &o = openings[key];
        o["label"] = key;
        o["count"] = o["count"].toInt() + 1;
        o["points"] = o["points"].toDouble() + (won ? 1 : result == "1/2-1/2" ? .5 : 0);
        o["finished"] = o["finished"].toInt() + (won || lost || result == "1/2-1/2");
        if (recent.size() < 20)
            recent << QVariantMap{{"date", q.value(4)},
                                  {"opponent", q.value(white ? 1 : 0)},
                                  {"result", result},
                                  {"elo", q.value(white ? 5 : 6)}};
    }
    q.finish();
    QVariantList openingList, weakest;
    for (const auto &o : openings) {
        openingList << o;
        if (o["finished"].toInt() >= 3)
            weakest << QVariantMap{{"label", o["label"]},
                                   {"count", o["finished"]},
                                   {"score", qRound(100 * o["points"].toDouble() / o["finished"].toInt())}};
    }
    std::sort(weakest.begin(), weakest.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()["score"].toInt() < b.toMap()["score"].toInt();
    });
    while (weakest.size() > 5)
        weakest.removeLast();
    // Le style, d'après la ligne principale : parties décisives, longueur, échecs donnés et
    // prises par partie, roques opposés. Des tendances à lire ensemble, pas un verdict.
    QVariantMap style;
    if (games) {
        QSqlQuery traits(db_);
        traits.prepare(
            "SELECT p.game_id,p.ply%2,SUM(instr(p.san,'+')>0 OR instr(p.san,'#')>0),SUM(instr(p.san,"
            "'x')>0),MAX(p.san GLOB 'O-O' OR p.san GLOB 'O-O[+#]'),MAX(p.san GLOB 'O-O-O*') FROM "
            "positions p JOIN games g ON g.id=p.game_id WHERE g.deleted=0 AND (g.white=? OR g.black=?) "
            "AND p.san<>'' GROUP BY p.game_id,p.ply%2");
        traits.addBindValue(name);
        traits.addBindValue(name);
        run(traits);
        struct Side {
            int checks = 0, captures = 0;
            bool shortCastle = false, longCastle = false;
        };
        QHash<qint64, std::array<Side, 2>> sides;
        while (traits.next())
            sides[traits.value(0).toLongLong()][traits.value(1).toInt()] = {
                traits.value(2).toInt(), traits.value(3).toInt(), traits.value(4).toBool(),
                traits.value(5).toBool()};
        double checks = 0, captures = 0;
        int opposite = 0, measured = 0;
        for (auto it = sides.cbegin(); it != sides.cend(); ++it) {
            if (!colourOf.contains(it.key()))
                continue;
            const auto &mine = it.value()[colourOf[it.key()] ? 0 : 1],
                       &theirs = it.value()[colourOf[it.key()] ? 1 : 0];
            ++measured;
            checks += mine.checks;
            captures += mine.captures;
            opposite += (mine.shortCastle && theirs.longCastle) || (mine.longCastle && theirs.shortCastle);
        }
        const int finished = games - unfinished;
        style = {{"decisive", finished ? qRound(100. * (wins + losses) / finished) : 0},
                 {"averageMoves", qRound(double(totalPlies) / games / 2)},
                 {"checks", measured ? qRound(10 * checks / measured) / 10. : 0},
                 {"captures", measured ? qRound(10 * captures / measured) / 10. : 0},
                 {"oppositeCastling", measured ? qRound(100. * opposite / measured) : 0}};
    }
    // The first six plies of every game, grouped into the lines the player actually reaches.
    QHash<qint64, QStringList> starts;
    QSqlQuery moves(db_);
    moves.prepare(
        "SELECT p.game_id,p.san FROM positions p JOIN games g ON g.id=p.game_id WHERE g.deleted=0 AND "
        "(g.white=? OR g.black=?) AND p.ply<6 AND p.san<>'' ORDER BY p.game_id,p.ply");
    moves.addBindValue(name);
    moves.addBindValue(name);
    run(moves);
    while (moves.next())
        starts[moves.value(0).toLongLong()] << moves.value(1).toString();
    QMap<QString, QVariantMap> lines[2];
    for (auto it = starts.cbegin(); it != starts.cend(); ++it) {
        if (!colourOf.contains(it.key()))
            continue;
        QStringList numbered;
        for (int i = 0; i < it.value().size(); ++i)
            numbered << (i % 2 == 0 ? QString::number(i / 2 + 1) + ". " : QString()) + it.value()[i];
        auto &line = lines[colourOf[it.key()] ? 0 : 1][numbered.join(' ')];
        line["line"] = numbered.join(' ');
        line["count"] = line["count"].toInt() + 1;
        if (pointsOf.contains(it.key())) {
            line["finished"] = line["finished"].toInt() + 1;
            line["points"] = line["points"].toDouble() + pointsOf[it.key()];
        }
    }
    auto top = [](const QMap<QString, QVariantMap> &all) {
        QVariantList list;
        for (auto line : all) {
            const int finished = line["finished"].toInt();
            line["score"] = finished ? qRound(100 * line["points"].toDouble() / finished) : QVariant();
            list << line;
        }
        std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
            return a.toMap()["count"].toInt() > b.toMap()["count"].toInt();
        });
        while (list.size() > 8)
            list.removeLast();
        return list;
    };
    QVariantList phaseList;
    const char *phaseNames[] = {"Courtes (≤ 25 coups)", "Moyennes (26–50 coups)", "Longues (> 50 coups)"};
    for (int i = 0; i < 3; ++i)
        phaseList << QVariantMap{{"label", phaseNames[i]},
                                 {"wins", phases[i].wins},
                                 {"draws", phases[i].draws},
                                 {"losses", phases[i].losses}};
    std::sort(openingList.begin(), openingList.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()["count"].toInt() > b.toMap()["count"].toInt();
    });
    // Où le retrouver en ligne : son pseudo, s'il a joué sous ce nom sur Lichess ou
    // Chess.com, et sa fiche FIDE si une partie en donne le numéro (les retransmissions
    // l'écrivent). Rien n'est demandé au réseau : ce sont des liens.
    QVariantMap links;
    if (games) {
        static const QRegularExpression handle("^[A-Za-z0-9_-]{2,30}$");
        QSqlQuery sites(db_);
        sites.prepare("SELECT SUM(pgn LIKE '%lichess.org/%'), SUM(pgn LIKE '%chess.com%') FROM games WHERE "
                      "deleted=0 AND (white=? OR black=?)");
        sites.addBindValue(name);
        sites.addBindValue(name);
        run(sites);
        if (sites.next() && handle.match(name).hasMatch()) {
            if (sites.value(0).toInt() > 0)
                links["lichess"] = "https://lichess.org/@/" + name;
            if (sites.value(1).toInt() > 0)
                links["chesscom"] = "https://www.chess.com/member/" + name;
        }
        QSqlQuery fide(db_);
        fide.prepare("SELECT white=?, pgn FROM games WHERE deleted=0 AND (white=? OR black=?) AND pgn LIKE "
                     "'%FideId \"%' ORDER BY date DESC LIMIT 20");
        fide.addBindValue(name);
        fide.addBindValue(name);
        fide.addBindValue(name);
        run(fide);
        while (fide.next() && !links.contains("fide")) {
            static const QRegularExpression white(R"re(\[WhiteFideId "(\d{3,12})"\])re"),
                black(R"re(\[BlackFideId "(\d{3,12})"\])re");
            const auto found = (fide.value(0).toBool() ? white : black).match(fide.value(1).toString());
            if (found.hasMatch())
                links["fide"] = "https://ratings.fide.com/profile/" + found.captured(1);
        }
    }
    return {{"name", name},
            {"links", links},
            {"style", style},
            {"games", games},
            {"wins", wins},
            {"draws", draws},
            {"losses", losses},
            {"unfinished", unfinished},
            {"whiteGames", whiteGames},
            {"blackGames", blackGames},
            {"elo", rating},
            {"openings", openingList},
            {"recent", recent},
            {"whiteLines", top(lines[0])},
            {"blackLines", top(lines[1])},
            {"phases", phaseList},
            {"weakest", weakest},
            {"eloHistory", eloHistory},
            {"score", games > unfinished ? 100. * (wins + .5 * draws) / (games - unfinished) : 0}};
}

QVariantList Database::gamesToReviewFor(const QStringList &names, int depth, int count) const {
    if (names.isEmpty())
        return {};
    const auto marks = placeholders(int(names.size()));
    QSqlQuery q(db_);
    // Les plus récentes d'abord pour choisir le lot, puis remises dans l'ordre : on passe
    // en revue de la plus ancienne à la plus récente, comme un import.
    q.prepare("SELECT id FROM games WHERE deleted=0 AND reviewed_depth<? AND (white COLLATE NOCASE IN (" +
              marks + ") OR black COLLATE NOCASE IN (" + marks + ")) ORDER BY id DESC LIMIT ?");
    q.addBindValue(std::clamp(depth, 1, 99));
    // La liste revient deux fois : une par côté de l'échiquier.
    for (int side = 0; side < 2; ++side)
        for (const auto &name : names)
            q.addBindValue(name);
    q.addBindValue(std::clamp(count, 1, 100000));
    run(q);
    QVariantList ids;
    while (q.next())
        ids << q.value(0).toLongLong();
    std::reverse(ids.begin(), ids.end());
    return ids;
}
int Database::gamesCountFor(const QStringList &names) const {
    if (names.isEmpty())
        return 0;
    const auto marks = placeholders(int(names.size()));
    QSqlQuery q(db_);
    q.prepare("SELECT COUNT(*) FROM games WHERE deleted=0 AND (white COLLATE NOCASE IN (" + marks +
              ") OR black COLLATE NOCASE IN (" + marks + "))");
    for (int side = 0; side < 2; ++side)
        for (const auto &name : names)
            q.addBindValue(name);
    run(q);
    return q.next() ? q.value(0).toInt() : 0;
}
QVariantMap Database::reviewSummary(const QStringList &names) const {
    if (names.isEmpty())
        return {{"games", 0}, {"moves", 0}};
    const auto marks = placeholders(int(names.size()));
    // Les notes sont pondérées par le nombre de coups mesurés : une partie de soixante
    // coups pèse plus qu'une de dix, sinon une miniature fausserait la moyenne.
    const QString statement = "SELECT COUNT(*),"
                              "SUM(CASE WHEN g.white COLLATE NOCASE IN (" +
                              marks +
                              ") THEN r.white_moves ELSE r.black_moves END),"
                              "SUM(CASE WHEN g.white COLLATE NOCASE IN (" +
                              marks +
                              ") THEN r.white_all*r.white_moves ELSE r.black_all*r.black_moves END),"
                              "SUM(CASE WHEN g.white COLLATE NOCASE IN (" +
                              marks +
                              ") THEN r.white_opening ELSE r.black_opening END),"
                              "COUNT(CASE WHEN g.white COLLATE NOCASE IN (" +
                              marks +
                              ") THEN r.white_opening ELSE r.black_opening END),"
                              "SUM(CASE WHEN g.white COLLATE NOCASE IN (" +
                              marks +
                              ") THEN r.white_middlegame ELSE r.black_middlegame END),"
                              "COUNT(CASE WHEN g.white COLLATE NOCASE IN (" +
                              marks +
                              ") THEN r.white_middlegame ELSE r.black_middlegame END),"
                              "SUM(CASE WHEN g.white COLLATE NOCASE IN (" +
                              marks +
                              ") THEN r.white_endgame ELSE r.black_endgame END),"
                              "COUNT(CASE WHEN g.white COLLATE NOCASE IN (" +
                              marks +
                              ") THEN r.white_endgame ELSE r.black_endgame END),"
                              "SUM(CASE WHEN g.white COLLATE NOCASE IN (" +
                              marks +
                              ") THEN r.white_blunders ELSE r.black_blunders END),"
                              "SUM(CASE WHEN g.white COLLATE NOCASE IN (" +
                              marks +
                              ") THEN r.white_mistakes ELSE r.black_mistakes END),"
                              "SUM(CASE WHEN g.white COLLATE NOCASE IN (" +
                              marks +
                              ") THEN r.white_inaccuracies ELSE r.black_inaccuracies END)"
                              " FROM game_reviews r JOIN games g ON g.id=r.game"
                              " WHERE g.deleted=0 AND (g.white COLLATE NOCASE IN (" +
                              marks + ") OR g.black COLLATE NOCASE IN (" + marks + "))";
    QSqlQuery q(db_);
    q.prepare(statement);
    // La même liste revient treize fois : onze fois dans les colonnes, deux dans le
    // filtre. Un décalage ici donnerait des chiffres faux sans rien signaler.
    for (int repeat = 0; repeat < 13; ++repeat)
        for (const auto &name : names)
            q.addBindValue(name);
    run(q);
    if (!q.next())
        throw std::runtime_error("Bilan indisponible.");
    const auto games = q.value(0).toInt();
    const auto moves = q.value(1).toLongLong();
    auto weighted = [&](int sumColumn, qint64 weight) {
        return weight > 0 && !q.value(sumColumn).isNull()
                   ? QVariant(qRound(10 * q.value(sumColumn).toDouble() / double(weight)) / 10.)
                   : QVariant();
    };
    auto mean = [&](int sumColumn, int countColumn) {
        const auto count = q.value(countColumn).toLongLong();
        return count > 0 && !q.value(sumColumn).isNull()
                   ? QVariant(qRound(10 * q.value(sumColumn).toDouble() / double(count)) / 10.)
                   : QVariant();
    };
    return {{"games", games},
            {"moves", moves},
            // La note d'ensemble est pondérée par les coups ; celles des phases sont une
            // moyenne par partie, le nombre de coups de chaque phase n'étant pas stocké.
            {"all", weighted(2, moves)},
            {"opening", mean(3, 4)},
            {"middlegame", mean(5, 6)},
            {"endgame", mean(7, 8)},
            {"blunders", q.value(9).toLongLong()},
            {"mistakes", q.value(10).toLongLong()},
            {"inaccuracies", q.value(11).toLongLong()}};
}

QVariantList Database::reviewHistory(const QStringList &names, int months) const {
    if (names.isEmpty())
        return {};
    const auto marks = placeholders(int(names.size()));
    // La date PGN s'écrit « AAAA.MM.JJ » : ses sept premiers caractères donnent le mois.
    // Une date inconnue s'écrit « ???? » et se retrouve donc groupée à part, ce qui vaut
    // mieux que de la rattacher arbitrairement à un mois.
    const QString statement = "SELECT substr(g.date,1,7) AS period,COUNT(*),"
                              "SUM(CASE WHEN g.white COLLATE NOCASE IN (" +
                              marks +
                              ") THEN r.white_moves ELSE r.black_moves END),"
                              "SUM(CASE WHEN g.white COLLATE NOCASE IN (" +
                              marks +
                              ") THEN r.white_all*r.white_moves ELSE r.black_all*r.black_moves END),"
                              "SUM(CASE WHEN g.white COLLATE NOCASE IN (" +
                              marks +
                              ") THEN r.white_blunders ELSE r.black_blunders END)"
                              " FROM game_reviews r JOIN games g ON g.id=r.game"
                              " WHERE g.deleted=0 AND (g.white COLLATE NOCASE IN (" +
                              marks + ") OR g.black COLLATE NOCASE IN (" + marks +
                              "))"
                              " GROUP BY period ORDER BY period DESC LIMIT ?";
    QSqlQuery q(db_);
    q.prepare(statement);
    for (int repeat = 0; repeat < 5; ++repeat)
        for (const auto &name : names)
            q.addBindValue(name);
    q.addBindValue(std::clamp(months, 1, 120));
    run(q);
    QVariantList rows;
    while (q.next()) {
        const auto moves = q.value(2).toLongLong();
        rows << QVariantMap{
            {"period", q.value(0)},
            {"games", q.value(1)},
            {"moves", moves},
            {"accuracy", moves > 0 && !q.value(3).isNull()
                             ? QVariant(qRound(10 * q.value(3).toDouble() / double(moves)) / 10.)
                             : QVariant()},
            {"blunders", q.value(4).toLongLong()}};
    }
    return rows;
}

std::vector<std::pair<double, double>> Database::accuracyEloSamples(int limit) const {
    QSqlQuery q(db_);
    q.setForwardOnly(true);
    q.prepare(
        "SELECT r.white_all,g.white_elo FROM game_reviews r JOIN games g ON g.id=r.game WHERE g.deleted=0 "
        "AND g.white_elo>0 AND r.white_all IS NOT NULL AND r.white_moves>=15 UNION ALL "
        "SELECT r.black_all,g.black_elo FROM game_reviews r JOIN games g ON g.id=r.game WHERE g.deleted=0 "
        "AND g.black_elo>0 AND r.black_all IS NOT NULL AND r.black_moves>=15 LIMIT ?");
    q.addBindValue(std::max(limit, 1));
    run(q);
    std::vector<std::pair<double, double>> samples;
    while (q.next())
        samples.emplace_back(q.value(0).toDouble(), q.value(1).toDouble());
    return samples;
}

QVariantList Database::duplicates() const {
    QSqlQuery q(db_);
    q.prepare("SELECT GROUP_CONCAT(id),COUNT(*),MIN(white),MIN(black) FROM games WHERE deleted=0 AND "
              "fingerprint<>'' GROUP BY fingerprint HAVING COUNT(*)>1 ORDER BY COUNT(*) DESC LIMIT 200");
    run(q);
    QVariantList out;
    while (q.next())
        out << QVariantMap{
            {"ids", q.value(0)}, {"count", q.value(1)}, {"white", q.value(2)}, {"black", q.value(3)}};
    return out;
}
QVariantMap Database::compareWith(const QString &other) const {
    check(QFileInfo(other).isFile(), "Base introuvable.");
    check(QFileInfo(other).canonicalFilePath() != QFileInfo(path_).canonicalFilePath(),
          "Choisissez une autre base que la bibliothèque ouverte.");
    QSqlQuery q(db_);
    q.prepare("ATTACH DATABASE ? AS other");
    q.addBindValue(other);
    check(q.exec(), "Base illisible : " + q.lastError().text());
    QVariantMap out;
    try {
        const auto count = [&](const QString &query) {
            QSqlQuery c(db_);
            check(c.exec(query) && c.next(), "Ce fichier n’est pas une bibliothèque Trait64.");
            return c.value(0).toLongLong();
        };
        const QString live = "deleted=0 AND fingerprint<>''";
        const auto mine = count("SELECT count(*) FROM main.games WHERE " + live);
        const auto theirs = count("SELECT count(*) FROM other.games WHERE " + live);
        const auto onlyTheirs =
            count("SELECT count(*) FROM other.games o WHERE " + live +
                  " AND NOT EXISTS(SELECT 1 FROM main.games g WHERE g.fingerprint=o.fingerprint "
                  "AND g.deleted=0)");
        const auto onlyMine =
            count("SELECT count(*) FROM main.games g WHERE " + live +
                  " AND NOT EXISTS(SELECT 1 FROM other.games o WHERE o.fingerprint=g.fingerprint "
                  "AND o.deleted=0)");
        QVariantList players;
        QSqlQuery top(db_);
        check(top.exec(
                  "SELECT name,count(*) n FROM (SELECT white name FROM other.games o WHERE " + live +
                  " AND NOT EXISTS(SELECT 1 FROM main.games g WHERE g.fingerprint=o.fingerprint AND "
                  "g.deleted=0) UNION ALL SELECT black FROM other.games o WHERE " +
                  live +
                  " AND NOT EXISTS(SELECT 1 FROM main.games g WHERE g.fingerprint=o.fingerprint AND "
                  "g.deleted=0)) WHERE name<>'' AND name<>'?' GROUP BY name ORDER BY n DESC,name LIMIT 10"),
              top.lastError().text());
        while (top.next())
            players << QVariantMap{{"name", top.value(0)}, {"games", top.value(1)}};
        out = {{"mine", mine},         {"theirs", theirs},         {"common", theirs - onlyTheirs},
               {"onlyMine", onlyMine}, {"onlyTheirs", onlyTheirs}, {"players", players}};
    } catch (...) {
        QSqlQuery(db_).exec("DETACH DATABASE other");
        throw;
    }
    QSqlQuery(db_).exec("DETACH DATABASE other");
    return out;
}
QVariantMap Database::mergeDuplicates(const std::function<bool(int)> &progress) {
    // Une même suite de coups ne suffit pas : deux nulles rapides de joueurs différents
    // ont la même empreinte. Les noms et le résultat départagent, sans casse.
    QSqlQuery q(db_);
    q.setForwardOnly(true);
    check(q.exec("SELECT id FROM games g WHERE deleted=0 AND fingerprint<>'' AND fingerprint IN (SELECT "
                 "fingerprint FROM games WHERE deleted=0 AND fingerprint<>'' GROUP BY fingerprint HAVING "
                 "COUNT(*)>1) ORDER BY fingerprint,lower(white),lower(black),result,id"),
          q.lastError().text());
    QList<qint64> ids;
    while (q.next())
        ids << q.value(0).toLongLong();
    q.finish();
    // Un en-tête vide, inconnu (« ? ») ou laissé à sa valeur par défaut se complète.
    const Game blank;
    const auto known = [&](const std::string &name, const std::string &value) {
        const auto fallback = blank.tags.find(name);
        return !value.empty() && value.find('?') == std::string::npos &&
               (fallback == blank.tags.end() || fallback->second != value);
    };
    const auto identity = [](const Game &g) {
        const auto tag = [&](const char *name) {
            const auto it = g.tags.find(name);
            return it == g.tags.end() ? QString() : QString::fromStdString(it->second).toLower().simplified();
        };
        return tag("White") + '\n' + tag("Black") + '\n' + tag("Result");
    };
    qint64 groups = 0, removed = 0;
    sql("SAVEPOINT merge_duplicates");
    try {
        std::vector<std::pair<qint64, Game>> group;
        QString groupKey;
        const auto flush = [&] {
            if (group.size() > 1) {
                // La partie la plus fournie (nœuds, puis commentaires) sert de base.
                const auto weight = [](const Game &g) {
                    size_t text = 0;
                    for (const auto &n : g.nodes)
                        text += n.comment.size() + n.nags.size();
                    return std::pair(g.nodes.size(), text);
                };
                std::stable_sort(group.begin(), group.end(), [&](const auto &a, const auto &b) {
                    return weight(a.second) > weight(b.second);
                });
                auto &base = group.front().second;
                for (size_t i = 1; i < group.size(); ++i) {
                    const auto &other = group[i].second;
                    base.merge(other);
                    for (const auto &[name, value] : other.tags)
                        if (known(name, value) && !known(name, base.tags[name]))
                            base.tags[name] = value;
                    trash(group[i].first, true);
                    ++removed;
                }
                save(base, group.front().first);
                ++groups;
            }
            group.clear();
        };
        int seen = 0;
        QByteArray fingerprint;
        QSqlQuery print(db_);
        print.prepare("SELECT fingerprint FROM games WHERE id=?");
        for (auto id : ids) {
            check(!progress || progress(seen++), "Fusion interrompue : aucune partie modifiée.");
            print.bindValue(0, id);
            run(print);
            const auto current = print.next() ? print.value(0).toByteArray() : QByteArray();
            print.finish();
            Game game = load(id);
            const auto key = identity(game);
            if (current != fingerprint || key != groupKey)
                flush();
            fingerprint = current;
            groupKey = key;
            group.emplace_back(id, std::move(game));
        }
        flush();
        sql("RELEASE merge_duplicates");
    } catch (...) {
        sql("ROLLBACK TO merge_duplicates");
        sql("RELEASE merge_duplicates");
        throw;
    }
    return {{"groups", groups}, {"removed", removed}};
}
int Database::rebuildIndexes(const std::function<bool(int)> &progress) {
    QSqlQuery ids(db_);
    ids.prepare("SELECT id FROM games ORDER BY id");
    run(ids);
    QList<qint64> list;
    while (ids.next())
        list << ids.value(0).toLongLong();
    ids.finish();
    sql("SAVEPOINT rebuild");
    try {
        int count = 0;
        for (auto id : list) {
            if (progress && !progress(count))
                throw std::runtime_error("Réindexation interrompue ; anciens index conservés.");
            save(load(id), id);
            ++count;
        }
        if (progress && !progress(count))
            throw std::runtime_error("Réindexation interrompue ; anciens index conservés.");
        sql("RELEASE rebuild");
    } catch (...) {
        sql("ROLLBACK TO rebuild");
        sql("RELEASE rebuild");
        throw;
    }
    return list.size();
}
} // namespace t64

#include <fstream>
namespace t64 {
int Database::importPgnFile(const QString &path, const std::function<bool(int)> &progress,
                            const QVariantMap &source, int *skipped) {
    std::ifstream input(path.toStdString(), std::ios::binary);
    if (!input)
        throw std::runtime_error("Impossible de lire le PGN.");
    const int before = stats()["games"].toInt();
    int processed = 0, unreadable = 0;
    sql("SAVEPOINT stream_import");
    try {
        const auto sourceId = registerSource(source);
        // Une partie illisible ou d'une variante non prise en charge (Crazyhouse, Atomic…)
        // est mise de côté et comptée. Elle faisait échouer tout le fichier : un compte
        // Lichess avec une seule partie de Crazyhouse n'importait aucune de ses 670 parties.
        scanPgnRecords(input, [&](std::string &&record, uint64_t) {
            std::vector<Game> games;
            try {
                games = parsePgn(record, 1);
            } catch (const std::exception &) {
                ++unreadable;
                return;
            }
            if (games.empty())
                return;
            if (progress && !progress(processed))
                throw std::runtime_error("Import interrompu : aucune partie ajoutée.");
            linkSource(sourceId, save(games.front(), 0, source.value("collection").toString()));
            ++processed;
        });
        if (skipped)
            *skipped = unreadable;
        if (!processed)
            throw std::runtime_error(unreadable
                                         ? QString("Aucune partie lisible : %1 partie(s) de variantes non "
                                                   "prises en charge ou illisibles.")
                                               .arg(unreadable)
                                               .toStdString()
                                         : std::string("Aucune partie trouvée."));
        if (progress && !progress(processed))
            throw std::runtime_error("Import interrompu : aucune partie ajoutée.");
        recordImport(sourceId, path, source.value("sha256", QString("")).toString(), processed,
                     stats()["games"].toInt() - before);
        sql("RELEASE stream_import");
    } catch (...) {
        sql("ROLLBACK TO stream_import");
        sql("RELEASE stream_import");
        throw;
    }
    return stats()["games"].toInt() - before;
}
QVariantMap Database::repertoireGap(qint64 repertoire, const QStringList &names, int limit,
                                    const std::shared_ptr<std::atomic_bool> &canceled, bool opponent) const {
    QueryCancellation cancellation(db_, canceled);
    if (names.isEmpty())
        return {{"games", 0}, {"touched", 0}, {"left", 0}, {"rows", QVariantList()}};
    QSqlQuery meta(db_);
    meta.prepare("SELECT white FROM repertoires WHERE id=?");
    meta.addBindValue(repertoire);
    run(meta);
    check(meta.next(), "Répertoire introuvable.");
    const bool white = meta.value(0).toBool();
    // L'adversaire joue l'autre couleur : c'est à son trait qu'il sort de ce que le
    // répertoire attend de lui.
    const bool side = opponent ? !white : white;
    // Les positions d'un répertoire sont rangées sous une clé textuelle, les parties sous
    // l'empreinte Zobrist : on dresse la correspondance une fois, en mémoire, plutôt que
    // de rejouer les coups de chaque partie.
    sql("CREATE TEMP TABLE IF NOT EXISTS gap_positions(key INTEGER PRIMARY KEY, textkey TEXT NOT NULL)");
    sql("CREATE TEMP TABLE IF NOT EXISTS gap_moves(key INTEGER NOT NULL, uci TEXT NOT NULL, "
        "PRIMARY KEY(key,uci))");
    sql("DELETE FROM gap_positions");
    sql("DELETE FROM gap_moves");
    QSqlQuery read(db_), insert(db_);
    read.prepare("SELECT key,fen FROM repertoire_positions WHERE repertoire=?");
    read.addBindValue(repertoire);
    run(read);
    insert.prepare("INSERT OR IGNORE INTO gap_positions(key,textkey) VALUES(?,?)");
    QMap<QString, qint64> keys;
    while (read.next()) {
        const auto textkey = read.value(0).toString();
        const auto key = qint64(positionHash(read.value(1).toString().toStdString()));
        keys.insert(textkey, key);
        insert.addBindValue(key);
        insert.addBindValue(textkey);
        run(insert);
    }
    read.prepare("SELECT key,uci FROM repertoire_moves WHERE repertoire=?");
    read.addBindValue(repertoire);
    run(read);
    insert.prepare("INSERT OR IGNORE INTO gap_moves(key,uci) VALUES(?,?)");
    while (read.next()) {
        const auto it = keys.constFind(read.value(0).toString());
        if (it == keys.cend())
            continue;
        insert.addBindValue(*it);
        insert.addBindValue(read.value(1));
        run(insert);
    }
    const auto marks = placeholders(int(names.size()));
    auto bindNames = [&](QSqlQuery &q, int times) {
        for (int i = 0; i < times; ++i)
            for (const auto &name : names)
                q.addBindValue(name);
    };
    // Le joueur ne dévie que de son propre trait : les plis pairs sont aux Blancs.
    const QString ownSide = side ? "0" : "1";
    const QString mine =
        side ? "g.white COLLATE NOCASE IN (" + marks + ")" : "g.black COLLATE NOCASE IN (" + marks + ")";
    // « Entrée dans le répertoire » veut dire avoir joué au moins un coup préparé. La
    // seule présence d'une position connue ne dit rien : la position de départ l'est
    // toujours, et toute partie y passe.
    // Pour l'adversaire, il suffit d'être arrivé à son trait sur une position préparée :
    // c'est là qu'il peut surprendre, dès le premier coup.
    const QString entered =
        opponent
            ? "EXISTS(SELECT 1 FROM positions p JOIN gap_positions r ON r.key=p.key WHERE p.game_id=g.id AND "
              "p.ply%2=" +
                  ownSide + ")"
            : "EXISTS(SELECT 1 FROM positions p JOIN gap_moves m ON m.key=p.key AND"
              " m.uci=p.uci WHERE p.game_id=g.id AND p.ply%" +
                  QString("2=") + ownSide + ")";
    QSqlQuery counts(db_);
    counts.prepare("SELECT COUNT(*),SUM(" + entered + ") FROM games g WHERE g.deleted=0 AND " + mine);
    bindNames(counts, 1);
    run(counts);
    qint64 played = 0, touched = 0;
    if (counts.next()) {
        played = counts.value(0).toLongLong();
        touched = counts.value(1).toLongLong();
    }
    // Premier pli où le joueur quitte sa propre préparation : la position est connue du
    // répertoire, le coup joué n'y figure pas.
    QSqlQuery leave(db_);
    leave.prepare("SELECT g.id,g.white,g.black,g.date,g.result,g.eco,d.ply,d.san,d.key FROM games g JOIN"
                  " (SELECT p.game_id,MIN(p.ply) AS ply,p.key,p.san FROM positions p"
                  "  JOIN gap_positions r ON r.key=p.key"
                  "  LEFT JOIN gap_moves m ON m.key=p.key AND m.uci=p.uci"
                  "  WHERE p.uci<>'' AND m.uci IS NULL AND p.ply%2=" +
                  ownSide +
                  "  GROUP BY p.game_id) d ON d.game_id=g.id"
                  " WHERE g.deleted=0 AND " +
                  entered + " AND " + mine + " ORDER BY d.ply,g.date DESC LIMIT ?");
    bindNames(leave, 1);
    leave.addBindValue(std::clamp(limit, 1, 500));
    run(leave);
    // L'empreinte revient vers la clé textuelle du répertoire : parcourir toutes les
    // positions pour chaque ligne rendait la lecture quadratique sans raison.
    QHash<qint64, QString> textOf;
    textOf.reserve(keys.size());
    for (auto it = keys.cbegin(); it != keys.cend(); ++it)
        textOf.insert(*it, it.key());
    QVariantList rows;
    QSqlQuery expected(db_);
    expected.prepare(
        "SELECT san FROM repertoire_moves WHERE repertoire=? AND key=? ORDER BY priority DESC,san");
    while (leave.next()) {
        QStringList prepared;
        const auto text = textOf.value(leave.value(8).toLongLong());
        if (!text.isEmpty()) {
            expected.addBindValue(repertoire);
            expected.addBindValue(text);
            run(expected);
            while (expected.next())
                prepared << expected.value(0).toString();
        }
        rows.append(QVariantMap{{"id", leave.value(0)},
                                {"white", leave.value(1)},
                                {"black", leave.value(2)},
                                {"date", leave.value(3)},
                                {"result", leave.value(4)},
                                {"eco", leave.value(5)},
                                // Le numéro du coup, pas le pli : c'est ce qu'on lit sur une feuille.
                                {"move", leave.value(6).toInt() / 2 + 1},
                                {"played", leave.value(7)},
                                {"ply", leave.value(6)},
                                {"expected", prepared}});
    }
    QSqlQuery total(db_);
    total.prepare("SELECT COUNT(*) FROM games g JOIN"
                  " (SELECT p.game_id FROM positions p JOIN gap_positions r ON r.key=p.key"
                  "  LEFT JOIN gap_moves m ON m.key=p.key AND m.uci=p.uci"
                  "  WHERE p.uci<>'' AND m.uci IS NULL AND p.ply%2=" +
                  ownSide + "  GROUP BY p.game_id) d ON d.game_id=g.id WHERE g.deleted=0 AND " + entered +
                  " AND " + mine);
    bindNames(total, 1);
    run(total);
    const qint64 left = total.next() ? total.value(0).toLongLong() : 0;
    return {{"games", played}, {"touched", touched}, {"left", left}, {"rows", rows}, {"white", white}};
}
} // namespace t64
