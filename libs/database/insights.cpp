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
    q.prepare("SELECT name,COUNT(*),MAX(elo) FROM (SELECT white name,white_elo elo FROM games WHERE "
              "deleted=0 UNION ALL SELECT black name,black_elo elo FROM games WHERE deleted=0) GROUP BY name "
              "ORDER BY COUNT(*) DESC,name LIMIT 1000");
    run(q);
    QVariantList result;
    while (q.next())
        result << QVariantMap{{"name", q.value(0)}, {"games", q.value(1)}, {"elo", q.value(2)}};
    return result;
}
QVariantMap Database::player(const QString &name) const {
    QSqlQuery q(db_);
    q.prepare("SELECT white,black,result,eco,date,white_elo,black_elo FROM games WHERE deleted=0 AND "
              "(white=? OR black=?) ORDER BY date DESC,id DESC");
    q.addBindValue(name);
    q.addBindValue(name);
    run(q);
    int games = 0, wins = 0, draws = 0, losses = 0, unfinished = 0, whiteGames = 0, blackGames = 0,
        rating = 0;
    QMap<QString, QVariantMap> openings;
    QVariantList recent;
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
    QVariantList openingList;
    for (const auto &o : openings)
        openingList << o;
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
            {"score", games > unfinished ? 100. * (wins + .5 * draws) / (games - unfinished) : 0}};
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
        if (progress)
            progress(count);
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
int Database::importPgnFile(const QString &path, const std::function<bool(int)> &progress) {
    std::ifstream input(path.toStdString(), std::ios::binary);
    if (!input)
        throw std::runtime_error("Impossible de lire le PGN.");
    const int before = stats()["games"].toInt();
    int processed = 0;
    sql("SAVEPOINT stream_import");
    try {
        readPgn(input, [&](Game &&game) {
            if (!progress(processed))
                throw std::runtime_error("Import interrompu : aucune partie ajoutée.");
            save(game);
            ++processed;
        });
        if (!processed)
            throw std::runtime_error("Aucune partie trouvée.");
        if (!progress(processed))
            throw std::runtime_error("Import interrompu : aucune partie ajoutée.");
        sql("RELEASE stream_import");
    } catch (...) {
        sql("ROLLBACK TO stream_import");
        sql("RELEASE stream_import");
        throw;
    }
    return stats()["games"].toInt() - before;
}
} // namespace t64
