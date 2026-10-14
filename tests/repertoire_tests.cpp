// SPDX-License-Identifier: AGPL-3.0-or-later
#include "database.h"
#include <QCoreApplication>
#include <QDate>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void rejects(F action) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception &) {
        failed = true;
    }
    check(failed, "Invalid repertoire operation accepted");
}
QString fen(const t64::Game &game, int node) {
    return QString::fromStdString(game.nodes.at(node).fen);
}
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir dir;
        t64::Database db(dir.filePath("repertoires.sqlite"));
        const auto a = t64::parsePgn("{Plan de départ} 1.Nf3! Nf6 2.g3 g6 3.Bg2 {Fianchetto} *").at(0);
        const auto b = t64::parsePgn("1.g3 g6 2.Nf3 Nf6 3.Bg2 *").at(0);
        auto id = db.createRepertoire("Reti", true, a);
        const auto model = db.save(a);
        db.addToRepertoire(id, a, model);
        db.addToRepertoire(id, b);
        check(db.repertoireMoves(id, fen(a, 0)).size() == 2, "Several planned answers from one position");
        check(db.repertoireMoves(id, fen(a, 4)).size() == 1 &&
                  db.repertoireMoves(id, fen(b, 4)).first().toMap()["note"] == "Fianchetto",
              "Transpositions share continuation and annotations");
        check(db.repertoires().first().toMap()["positions"].toInt() == 4,
              "White training positions are deduplicated across move orders");
        db.addToRepertoire(id, a);
        check(db.repertoireMoves(id, fen(a, 4)).first().toMap()["note"] == "Fianchetto",
              "Repeated merges preserve notes without duplication");
        db.editRepertoireMove(id, fen(a, 0), "g2g3", "Choix principal", true);
        check(db.repertoireMoves(id, fen(a, 0)).first().toMap()["uci"] == "g2g3",
              "Principal continuation can be changed independently of alternatives");
        auto exported = db.exportRepertoire(id);
        auto roundTrip = t64::parsePgn(exported.pgn()).at(0);
        check(roundTrip.nodes[0].comment == "Plan de départ" &&
                  roundTrip.tags.at("Trait64RepertoireSide") == "White",
              "PGN export preserves root note and repertoire side");
        check(roundTrip.nodes[roundTrip.mainline()[1]].uci == "g2g3", "PGN principal move follows selection");
        bool nag = false;
        for (const auto &node : roundTrip.nodes)
            nag |= node.uci == "g1f3" && node.nags == std::vector<int>{1};
        check(nag, "Move annotations survive repertoire export");
        auto copy = db.createRepertoire("Copie", true, roundTrip);
        check(db.repertoireMoves(copy, fen(a, 0)).size() == 2 &&
                  db.repertoireMoves(copy, fen(a, 4)).size() == 1,
              "Exported transposition graph can be imported without losing edges");
        auto card = db.nextRepertoirePosition(id);
        check(!card.empty() && card["answers"].toList().size() > 0, "Due position has actual accepted moves");
        db.reviewRepertoire(id, fen(a, 4), false);
        check(db.repertoireWeaknesses(id).size() == 1 &&
                  db.repertoireWeaknesses(id).first().toMap()["successes"].toInt() == 0,
              "Failed recall is recorded as an observed weakness");
        db.reviewRepertoire(id, fen(b, 4), true);
        const auto weak = db.repertoireWeaknesses(id).first().toMap();
        check(weak["reviews"].toInt() == 2 && weak["successes"].toInt() == 1 &&
                  weak["interval"].toInt() == 3 &&
                  weak["due"].toString() == QDate::currentDate().addDays(3).toString(Qt::ISODate),
              "Review schedule belongs to the position across transpositions");
        db.removeRepertoireMove(id, fen(a, 0), "g1f3");
        check(db.repertoireMoves(id, fen(a, 4)).size() == 1 &&
                  db.repertoireWeaknesses(id).first().toMap()["reviews"].toInt() == 2,
              "Deleting one route retains shared positions and their training history");
        rejects([&] { db.editRepertoireMove(id, fen(a, 0), "a2a3", "", true); });
        db.renameRepertoire(id, "Mon répertoire");
        check(db.repertoire(id)["name"] == "Mon répertoire", "Repertoire rename persists");
        rejects([&] { db.renameRepertoire(id, "   "); });
        t64::Game disconnected("8/8/8/8/8/8/4k3/K7 w - - 0 1");
        rejects([&] { db.addToRepertoire(id, disconnected); });
        t64::Game frc(t64::chess960Fen(518), true);
        rejects([&] { db.addToRepertoire(id, frc); });
        auto extension = t64::parsePgn("1.d4 d5 *").at(0);
        rejects([&] { db.importRepertoire(id, {extension, disconnected}); });
        check(db.repertoireMoves(id, fen(a, 0)).size() == 1, "Multi-game import rollback is atomic");
        auto deleted = t64::parsePgn("1.e4 (1.d4 d5) e5 *").at(0);
        deleted.removeVariation(deleted.nodes[0].children[1]);
        auto noOrphans = db.createRepertoire("Sans branche supprimée", true, deleted);
        check(db.repertoireMoves(noOrphans, fen(deleted, 0)).size() == 1,
              "Removed study branches are not added from backing nodes");
        auto black = db.createRepertoire("Noirs", false, a);
        const auto blackCard = db.nextRepertoirePosition(black);
        check(t64::checkedBoard(blackCard["fen"].toString().toStdString()).sideToMove() ==
                  chess::Color::BLACK,
              "Black repertoire trains only Black turns");
        auto cycle = t64::parsePgn("1.Nf3 Nf6 2.Ng1 Ng8 3.Nf3 *").at(0);
        auto circular = db.createRepertoire("Cycle", true, cycle);
        check(db.exportRepertoire(circular).nodes.size() == 5, "Cycle export terminates and retains graph");
        frc.play(0, "e2e4");
        auto variant = db.createRepertoire("960", true, frc);
        check(t64::parsePgn(db.exportRepertoire(variant).pgn()).at(0).chess960,
              "Chess960 repertoire export keeps variant");
        auto sicilian = t64::parsePgn("[Event \"Reference\"]\n1.e4 c5 *").at(0);
        db.save(sicilian);
        auto gaps = db.repertoireGaps(noOrphans);
        check(gaps.size() == 1 && gaps.first().toMap()["uci"] == "c7c5" &&
                  gaps.first().toMap()["count"].toInt() == 1,
              "Coverage identifies an actual missing opponent reply in local reference games");
        db.deleteRepertoire(copy);
        db.deleteRepertoire(id);
        check(db.repertoireMoves(id, fen(a, 0)).empty() && db.repertoireWeaknesses(id).empty() &&
                  db.stats()["games"].toInt() == 2 && db.integrity() == "ok",
              "Deleting repertoire cascades its data and preserves model games");
        std::cout << checks << " repertoire checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
