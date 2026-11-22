// SPDX-License-Identifier: AGPL-3.0-or-later
// Réparation d'une bibliothèque abîmée : ce qui se lit encore est recopié, ligne par ligne
// là où une page est illisible, dans une base neuve dont les index sont ensuite recalculés.
#include "database.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <climits>
#include <stdexcept>
namespace t64 {
namespace {
// Les tables qui portent des données. Les autres (positions, statistiques, recherche
// plein texte, compteurs, cache) se déduisent des parties et sont recalculées.
const char *const kPrimary[] = {"games",
                                "cards",
                                "review_log",
                                "analysis_cache",
                                "analysis_jobs",
                                "analysis_results",
                                "game_reviews",
                                "import_history",
                                "import_jobs",
                                "puzzles",
                                "puzzle_attempts",
                                "repertoires",
                                "repertoire_positions",
                                "repertoire_moves",
                                "repertoire_reviews",
                                "sources",
                                "source_games"};
QStringList columns(const QSqlDatabase &db, const QString &schema, const QString &table) {
    QSqlQuery q(db);
    QStringList out;
    if (q.exec(QString("PRAGMA %1.table_info(%2)").arg(schema, table)))
        while (q.next())
            out << q.value(1).toString();
    return out;
}
} // namespace
QVariantMap Database::salvage(const QString &source, const QString &target,
                              const std::function<bool(int)> &progress) {
    { Database fresh(target); } // le schéma courant, vide
    const auto name = "repair-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    qint64 rows = 0, lost = 0;
    // La connexion de travail est rendue quoi qu'il arrive, échec compris.
    struct Release {
        QString name;
        ~Release() { QSqlDatabase::removeDatabase(name); }
    } release{name};
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(target);
        const auto fail = [&](const QString &message) { throw std::runtime_error(message.toStdString()); };
        if (!db.open())
            fail(db.lastError().text());
        QSqlQuery q(db);
        q.prepare("ATTACH DATABASE ? AS old");
        q.addBindValue(source);
        if (!q.exec() || !q.exec("SELECT count(*) FROM old.sqlite_master"))
            fail("Base illisible : même sa liste de tables est perdue. Restaurez une sauvegarde.");
        q.exec("BEGIN");
        for (const char *table : kPrimary) {
            const auto mine = columns(db, "main", table);
            QStringList shared;
            for (const auto &column : columns(db, "old", table))
                if (mine.contains(column))
                    shared << column;
            if (shared.isEmpty())
                continue;
            const auto list = shared.join(',');
            const auto copy =
                QString("INSERT OR IGNORE INTO main.%1(%2) SELECT %2 FROM old.%1").arg(table, list);
            QSqlQuery top(db);
            if (!top.exec(QString("SELECT min(rowid),max(rowid) FROM old.%1").arg(table)) || !top.next()) {
                // Sans bornes lisibles, la table passe d'un bloc ou pas du tout.
                QSqlQuery all(db);
                if (all.exec(copy))
                    rows += all.numRowsAffected();
                else
                    ++lost;
                continue;
            }
            const qint64 first = top.value(0).toLongLong(), last = top.value(1).toLongLong();
            top.finish();
            QSqlQuery chunk(db), one(db);
            chunk.prepare(copy + " WHERE rowid BETWEEN ? AND ?");
            one.prepare(copy + " WHERE rowid=?");
            constexpr qint64 step = 1000;
            for (qint64 from = first; from <= last && last > 0; from += step) {
                if (progress && !progress(int(std::min<qint64>(rows, INT_MAX))))
                    fail("Réparation interrompue : la bibliothèque d’origine n’a pas changé.");
                chunk.bindValue(0, from);
                chunk.bindValue(1, from + step - 1);
                if (chunk.exec()) {
                    rows += chunk.numRowsAffected();
                    continue;
                }
                // Une page illisible dans le bloc : on reprend ligne à ligne.
                for (qint64 id = from; id < from + step && id <= last; ++id) {
                    one.bindValue(0, id);
                    if (one.exec())
                        rows += one.numRowsAffected();
                    else
                        ++lost;
                }
            }
        }
        if (!q.exec("COMMIT") || !q.exec("DETACH DATABASE old"))
            fail(q.lastError().text());
    }
    Database fixed(target);
    const int games = fixed.rebuildIndexes(progress);
    return {{"games", games}, {"rows", rows}, {"lost", lost}, {"integrity", fixed.integrity()}};
}
} // namespace t64
