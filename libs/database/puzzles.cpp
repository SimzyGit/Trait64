// SPDX-License-Identifier: AGPL-3.0-or-later
// Tactical puzzles: a local bank (Lichess CC0 export, own mistakes) and the attempt history.
#include "database.h"
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <stdexcept>

namespace t64 {
namespace {
void run(QSqlQuery &q) {
    if (!q.exec())
        throw std::runtime_error(q.lastError().text().toStdString());
}
} // namespace

void Database::createPuzzleTables() {
    // `fen` is the position the solver faces; `solution` starts with the solver's move.
    sql("CREATE TABLE IF NOT EXISTS puzzles(id INTEGER PRIMARY KEY,ref TEXT NOT NULL UNIQUE,fen TEXT NOT "
        "NULL,"
        "solution TEXT NOT NULL,rating INTEGER NOT NULL,themes TEXT NOT NULL DEFAULT '',popularity INTEGER "
        "NOT "
        "NULL DEFAULT 0,source TEXT NOT NULL,added TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)");
    sql("CREATE INDEX IF NOT EXISTS puzzles_rating ON puzzles(source,rating)");
    sql("CREATE INDEX IF NOT EXISTS puzzles_any_rating ON puzzles(rating)");
    sql("CREATE TABLE IF NOT EXISTS puzzle_attempts(id INTEGER PRIMARY KEY,puzzle INTEGER NOT NULL "
        "REFERENCES "
        "puzzles(id) ON DELETE CASCADE,at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,success INTEGER NOT NULL,"
        "seconds REAL NOT NULL,rating_after INTEGER NOT NULL)");
    sql("CREATE INDEX IF NOT EXISTS puzzle_attempts_puzzle ON puzzle_attempts(puzzle,success)");
}

int Database::addPuzzles(const std::vector<Puzzle> &puzzles) {
    sql("SAVEPOINT puzzles_add");
    try {
        QSqlQuery q(db_);
        q.prepare("INSERT OR IGNORE INTO puzzles(ref,fen,solution,rating,themes,popularity,source) "
                  "VALUES(?,?,?,?,?,?,?)");
        int added = 0;
        for (const auto &p : puzzles) {
            q.bindValue(0, p.ref);
            q.bindValue(1, p.fen);
            q.bindValue(2, p.solution);
            q.bindValue(3, std::clamp(p.rating, 100, 3500));
            q.bindValue(4, p.themes.left(500));
            q.bindValue(5, p.popularity);
            q.bindValue(6, p.source);
            run(q);
            added += q.numRowsAffected();
        }
        sql("RELEASE puzzles_add");
        return added;
    } catch (...) {
        sql("ROLLBACK TO puzzles_add");
        sql("RELEASE puzzles_add");
        throw;
    }
}

QVariantMap Database::nextPuzzle(int rating, int spread, const QString &theme, const QString &source) const {
    // A random target inside the window, then the first unsolved puzzles above it: the index
    // does the work, so the choice stays instant on a bank of millions.
    auto *random = QRandomGenerator::global();
    const int low = std::max(100, rating - spread), high = std::min(3500, rating + spread);
    for (int attempt = 0; attempt < 3; ++attempt) {
        const int target = attempt == 2 ? 100 : low + random->bounded(std::max(1, high - low + 1));
        QString statement = "SELECT id,fen,solution,rating,themes,source FROM puzzles WHERE rating>=?";
        QVariantList args{target};
        if (!source.isEmpty()) {
            statement += " AND source=?";
            args << source;
        }
        if (!theme.isEmpty()) {
            statement += " AND (' '||themes||' ') LIKE ?";
            args << "% " + theme + " %";
        }
        statement +=
            " AND NOT EXISTS(SELECT 1 FROM puzzle_attempts a WHERE a.puzzle=puzzles.id AND a.success=1) "
            "ORDER BY rating LIMIT 20";
        QSqlQuery q(db_);
        q.prepare(statement);
        for (const auto &arg : args)
            q.addBindValue(arg);
        run(q);
        QVariantList found;
        while (q.next())
            found << QVariantMap{{"id", q.value(0)},     {"fen", q.value(1)},    {"solution", q.value(2)},
                                 {"rating", q.value(3)}, {"themes", q.value(4)}, {"source", q.value(5)}};
        if (!found.isEmpty())
            return found[random->bounded(int(found.size()))].toMap();
    }
    return {};
}

void Database::recordPuzzle(qint64 id, bool success, double seconds, int ratingAfter) {
    QSqlQuery q(db_);
    q.prepare("INSERT INTO puzzle_attempts(puzzle,success,seconds,rating_after) VALUES(?,?,?,?)");
    q.addBindValue(id);
    q.addBindValue(success ? 1 : 0);
    q.addBindValue(std::max(0.0, seconds));
    q.addBindValue(ratingAfter);
    run(q);
}

QVariantMap Database::puzzleStats() const {
    QSqlQuery q(db_);
    q.prepare("SELECT (SELECT COUNT(*) FROM puzzles),(SELECT COUNT(*) FROM puzzles WHERE source='mine'),"
              "(SELECT COUNT(DISTINCT puzzle) FROM puzzle_attempts WHERE success=1),"
              "(SELECT COUNT(*) FROM puzzle_attempts WHERE date(at,'localtime')=date('now','localtime')),"
              "(SELECT COALESCE(AVG(success),0) FROM (SELECT success FROM puzzle_attempts ORDER BY id DESC "
              "LIMIT 50))");
    run(q);
    q.next();
    // Par thème : chaque tentative compte pour tous les thèmes du puzzle. Les tentatives
    // sont celles d'un joueur, quelques milliers au plus : on les lit toutes.
    QSqlQuery attempts(db_);
    attempts.prepare("SELECT p.themes,a.success FROM puzzle_attempts a JOIN puzzles p ON p.id=a.puzzle");
    run(attempts);
    QMap<QString, QPair<int, int>> tally; // thème → (tentatives, réussites)
    while (attempts.next())
        for (const auto &theme : attempts.value(0).toString().split(' ', Qt::SkipEmptyParts)) {
            auto &entry = tally[theme];
            ++entry.first;
            entry.second += attempts.value(1).toInt();
        }
    QVariantList themes;
    for (auto it = tally.cbegin(); it != tally.cend(); ++it)
        themes.append(QVariantMap{
            {"tag", it.key()}, {"attempts", it->first}, {"success", qRound(100.0 * it->second / it->first)}});
    return {{"total", q.value(0)},
            {"mine", q.value(1)},
            {"solved", q.value(2)},
            {"today", q.value(3)},
            {"recentSuccess", qRound(q.value(4).toDouble() * 100)},
            {"themes", themes}};
}
} // namespace t64
