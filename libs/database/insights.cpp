// SPDX-License-Identifier: AGPL-3.0-or-later
#include "database.h"
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>
#include <stdexcept>
namespace t64 {
namespace {
void run(QSqlQuery &q) {
    if (!q.exec())
        throw std::runtime_error(q.lastError().text().toStdString());
}
} // namespace
QVariantList Database::players() const {
    QSqlQuery q(db_);
    q.prepare(
        "SELECT pc.name,pc.games,MAX(COALESCE((SELECT MAX(white_elo) FROM games WHERE deleted=0 AND "
        "white=pc.name),0),COALESCE((SELECT MAX(black_elo) FROM games WHERE deleted=0 AND black=pc.name),0)) "
        "FROM player_counts pc WHERE games>0 ORDER BY pc.games DESC,pc.name LIMIT 1000");
    run(q);
    QVariantList result;
    while (q.next())
        result << QVariantMap{{"name", q.value(0)}, {"games", q.value(1)}, {"elo", q.value(2)}};
    return result;
}
QVariantMap Database::player(const QString &name) const {
    QSqlQuery q(db_);
    q.prepare("SELECT white,black,result,eco,date,white_elo,black_elo,id,plies FROM games WHERE deleted=0 AND "
              "id IN(SELECT id FROM games WHERE deleted=0 AND white=? UNION SELECT id FROM games WHERE "
              "deleted=0 AND black=?) ORDER BY date DESC,id DESC");
    q.addBindValue(name);
    q.addBindValue(name);
    run(q);
    int games = 0, wins = 0, draws = 0, losses = 0, unfinished = 0, whiteGames = 0, blackGames = 0,
        rating = 0;
    QMap<QString, QVariantMap> openings;
    QVariantList recent, eloHistory;
    QHash<qint64, bool> colourOf;          // game id -> played white
    QHash<qint64, double> pointsOf;        // game id -> points scored, finished games only
    // Game length as a proxy for the phase where it was decided.
    struct Phase { int wins = 0, draws = 0, losses = 0; };
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
            weakest << QVariantMap{{"label", o["label"]}, {"count", o["finished"]},
                                   {"score", qRound(100 * o["points"].toDouble() / o["finished"].toInt())}};
    }
    std::sort(weakest.begin(), weakest.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()["score"].toInt() < b.toMap()["score"].toInt();
    });
    while (weakest.size() > 5) weakest.removeLast();
    // The first six plies of every game, grouped into the lines the player actually reaches.
    QHash<qint64, QStringList> starts;
    QSqlQuery moves(db_);
    moves.prepare("SELECT p.game_id,p.san FROM positions p JOIN games g ON g.id=p.game_id WHERE g.deleted=0 AND "
                  "(g.white=? OR g.black=?) AND p.ply<6 AND p.san<>'' ORDER BY p.game_id,p.ply");
    moves.addBindValue(name);
    moves.addBindValue(name);
    run(moves);
    while (moves.next()) starts[moves.value(0).toLongLong()] << moves.value(1).toString();
    QMap<QString, QVariantMap> lines[2];
    for (auto it = starts.cbegin(); it != starts.cend(); ++it) {
        if (!colourOf.contains(it.key())) continue;
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
        while (list.size() > 8) list.removeLast();
        return list;
    };
    QVariantList phaseList;
    const char *phaseNames[] = {"Courtes (≤ 25 coups)", "Moyennes (26–50 coups)", "Longues (> 50 coups)"};
    for (int i = 0; i < 3; ++i)
        phaseList << QVariantMap{{"label", phaseNames[i]}, {"wins", phases[i].wins}, {"draws", phases[i].draws},
                                 {"losses", phases[i].losses}};
    std::sort(openingList.begin(), openingList.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()["count"].toInt() > b.toMap()["count"].toInt();
    });
    return {{"name", name},
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
} // namespace

QVariantMap Database::reviewSummary(const QStringList &names) const {
    if (names.isEmpty())
        return {{"games", 0}, {"moves", 0}};
    const auto marks = placeholders(int(names.size()));
    // Les notes sont pondérées par le nombre de coups mesurés : une partie de soixante
    // coups pèse plus qu'une de dix, sinon une miniature fausserait la moyenne.
    const QString statement =
        "SELECT COUNT(*),"
        "SUM(CASE WHEN g.white COLLATE NOCASE IN (" + marks + ") THEN r.white_moves ELSE r.black_moves END),"
        "SUM(CASE WHEN g.white COLLATE NOCASE IN (" + marks + ") THEN r.white_all*r.white_moves ELSE r.black_all*r.black_moves END),"
        "SUM(CASE WHEN g.white COLLATE NOCASE IN (" + marks + ") THEN r.white_opening ELSE r.black_opening END),"
        "COUNT(CASE WHEN g.white COLLATE NOCASE IN (" + marks + ") THEN r.white_opening ELSE r.black_opening END),"
        "SUM(CASE WHEN g.white COLLATE NOCASE IN (" + marks + ") THEN r.white_middlegame ELSE r.black_middlegame END),"
        "COUNT(CASE WHEN g.white COLLATE NOCASE IN (" + marks + ") THEN r.white_middlegame ELSE r.black_middlegame END),"
        "SUM(CASE WHEN g.white COLLATE NOCASE IN (" + marks + ") THEN r.white_endgame ELSE r.black_endgame END),"
        "COUNT(CASE WHEN g.white COLLATE NOCASE IN (" + marks + ") THEN r.white_endgame ELSE r.black_endgame END),"
        "SUM(CASE WHEN g.white COLLATE NOCASE IN (" + marks + ") THEN r.white_blunders ELSE r.black_blunders END),"
        "SUM(CASE WHEN g.white COLLATE NOCASE IN (" + marks + ") THEN r.white_mistakes ELSE r.black_mistakes END),"
        "SUM(CASE WHEN g.white COLLATE NOCASE IN (" + marks + ") THEN r.white_inaccuracies ELSE r.black_inaccuracies END)"
        " FROM game_reviews r JOIN games g ON g.id=r.game"
        " WHERE g.deleted=0 AND (g.white COLLATE NOCASE IN (" + marks + ") OR g.black COLLATE NOCASE IN (" + marks + "))";
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
    const QString statement =
        "SELECT substr(g.date,1,7) AS period,COUNT(*),"
        "SUM(CASE WHEN g.white COLLATE NOCASE IN (" + marks + ") THEN r.white_moves ELSE r.black_moves END),"
        "SUM(CASE WHEN g.white COLLATE NOCASE IN (" + marks + ") THEN r.white_all*r.white_moves ELSE r.black_all*r.black_moves END),"
        "SUM(CASE WHEN g.white COLLATE NOCASE IN (" + marks + ") THEN r.white_blunders ELSE r.black_blunders END)"
        " FROM game_reviews r JOIN games g ON g.id=r.game"
        " WHERE g.deleted=0 AND (g.white COLLATE NOCASE IN (" + marks + ") OR g.black COLLATE NOCASE IN (" + marks + "))"
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
        rows << QVariantMap{{"period", q.value(0)},
                            {"games", q.value(1)},
                            {"moves", moves},
                            {"accuracy", moves > 0 && !q.value(3).isNull()
                                             ? QVariant(qRound(10 * q.value(3).toDouble() / double(moves)) / 10.)
                                             : QVariant()},
                            {"blunders", q.value(4).toLongLong()}};
    }
    return rows;
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
                            const QVariantMap &source) {
    std::ifstream input(path.toStdString(), std::ios::binary);
    if (!input)
        throw std::runtime_error("Impossible de lire le PGN.");
    const int before = stats()["games"].toInt();
    int processed = 0;
    sql("SAVEPOINT stream_import");
    try {
        const auto sourceId = registerSource(source);
        readPgn(input, [&](Game &&game) {
            if (progress && !progress(processed))
                throw std::runtime_error("Import interrompu : aucune partie ajoutée.");
            linkSource(sourceId, save(game));
            ++processed;
        });
        if (!processed)
            throw std::runtime_error("Aucune partie trouvée.");
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
} // namespace t64
