// SPDX-License-Identifier: AGPL-3.0-or-later
// Mesures de lecture sur une bibliothèque déjà importée : ce que coûte la première
// exécution de chaque recherche, celle que l'utilisateur attend réellement. Les
// suivantes profitent des caches et ne disent rien de l'attente.
#include "database.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::cerr << "usage: search_probe <library.sqlite>\n";
        return 2;
    }
    try {
        t64::Database db(QString::fromLocal8Bit(argv[1]));
        QJsonObject report;
        const auto measure = [&](const QString &name, const auto &operation) {
            QElapsedTimer clock;
            clock.start();
            const qint64 rows = operation();
            report[name] = QJsonObject{{"ms", clock.nsecsElapsed() / 1e6}, {"rows", rows}};
        };
        const int games = db.stats()["games"].toInt();
        report["games"] = games;
        measure("recent_page_200",
                [&] { return db.search({}, {}, false, {}, false, {{"limit", 200}}).size(); });
        // Une partie du milieu de la base, assez longue pour passer l'horizon de l'index.
        t64::Game game;
        std::vector<int> line;
        for (qint64 id = games / 2; id < games / 2 + 5000 && line.size() < 90; ++id) {
            try {
                game = db.load(id);
                line = game.mainline();
            } catch (...) {
            }
        }
        if (line.size() < 90)
            throw std::runtime_error("No long game found in the middle of the library");
        const auto fen = [&](size_t ply) { return QString::fromStdString(game.nodes[line[ply]].fen); };
        const auto count = [&](const QString &position, const QVariantMap &filters = {}) {
            return db.search({}, {}, false, position, false, filters).size();
        };
        measure("position_ply_8", [&] { return count(fen(8)); });
        measure("position_ply_30", [&] { return count(fen(30)); });
        measure("position_ply_60_replayed", [&] { return count(fen(60)); });
        measure("position_last_replayed", [&] { return count(fen(line.size() - 1)); });
        measure("pawn_structure_ply_60", [&] { return count(fen(60), {{"pawnStructure", true}}); });
        measure("material_of_final_position",
                [&] { return count({}, {{"material", fen(line.size() - 1).section(' ', 0, 0)}}); });
        measure("material_rook_ending", [&] { return count({}, {{"material", "KRPPPkrppp"}}); });
        // Placement partiel : aucune aide d'index, toutes les parties sont rejouées.
        measure("placement_isolated_queen_pawn",
                [&] { return count({}, {{"placement", "Pd4 pd5 Nf3 nf6"}}); });
        measure("placement_again_other_connection", [&] {
            t64::Database again(db.path());
            return qsizetype(
                again.search({}, {}, false, {}, false, {{"placement", "Pd4 pd5 Nf3 nf6"}}).size());
        });
        const auto start = QString::fromStdString(chess::constants::STARTPOS);
        measure("opening_tree_start", [&] { return db.explorer(start).size(); });
        measure("opening_tree_ply_8", [&] { return db.explorer(fen(8)).size(); });
        measure("position_report_start", [&] { return db.openingReport(start)["total"].toLongLong(); });
        measure("position_report_ply_8", [&] { return db.openingReport(fen(8))["total"].toLongLong(); });
        measure("players_list", [&] { return db.players().size(); });
        measure("player_fragment", [&] { return db.search("arlse").size(); });
        measure("duplicates", [&] { return db.duplicates().size(); });
        std::cout << QJsonDocument(report).toJson().constData();
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}
