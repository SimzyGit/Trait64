// SPDX-License-Identifier: AGPL-3.0-or-later
#include "database.h"
#include "queue.h"
#include "uci.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>

int checks = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
bool waitFor(const std::function<bool()> &test, int timeout = 20000) {
    QElapsedTimer timer;
    timer.start();
    while (!test() && timer.elapsed() < timeout)
        QTest::qWait(10);
    return test();
}
QVariantMap job(t64::AnalysisQueue &queue, qint64 id) {
    for (const auto &v : queue.jobs())
        if (v.toMap()["id"].toLongLong() == id)
            return v.toMap();
    return {};
}
qint64 added(t64::AnalysisQueue &queue, int count) {
    check(waitFor([&] { return queue.jobs().size() == count; }), "Job was not persisted");
    return queue.jobs().first().toMap()["id"].toLongLong();
}
void complete(t64::AnalysisQueue &queue, qint64 id) {
    check(waitFor([&] {
              const auto s = job(queue, id)["status"];
              return s == "completed" || s == "failed";
          }),
          "Background analysis timed out");
    if (job(queue, id)["status"] != "completed")
        throw std::runtime_error(job(queue, id)["error"].toString().toStdString());
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() == 4 && args[1] == "--crash-worker") {
        t64::AnalysisQueue queue;
        queue.setLibrary(args[2]);
        QObject::connect(&queue, &t64::AnalysisQueue::changed, &app, [&] {
            if (!queue.jobs().empty() && queue.jobs().first().toMap()["done"].toInt() >= 1)
                std::cout << "CHECKPOINT\n" << std::flush;
        });
        queue.enqueue("1.e4 e5 2.Nf3 Nc6 3.Bb5 a6 *",
                      {{"enginePath", args[3]}, {"milliseconds", 2000}, {"hash", 16}, {"multiPV", 1}}, true);
        return app.exec();
    }
    QTemporaryDir dir;
    try {
        QString enginePath = qEnvironmentVariable("T64_STOCKFISH");
        if (enginePath.isEmpty())
            enginePath = QStandardPaths::findExecutable("stockfish");
        if (enginePath.isEmpty() && QFileInfo("/usr/games/stockfish").isExecutable())
            enginePath = "/usr/games/stockfish";
        if (enginePath.isEmpty()) {
            std::cerr << "Stockfish missing; integration skipped\n";
            return 77;
        }

        // Real NNUE files have explicit hashes; unresolved embedded resources deliberately disable cache.
        QProcess exportNet;
        exportNet.setWorkingDirectory(dir.path());
        exportNet.start(enginePath);
        check(exportNet.waitForStarted(), "Start Stockfish for its NNUE export");
        exportNet.write("export_net\nquit\n");
        check(exportNet.waitForFinished(10000) && exportNet.exitCode() == 0, "Export real embedded NNUE");
        t64::Uci probe;
        probe.start(enginePath, 1, 16, 1);
        check(waitFor([&] { return probe.ready(); }), "Read real Stockfish descriptors");
        QVariantMap options;
        for (const auto &v : probe.options()) {
            const auto d = v.toMap();
            const auto name = d["name"].toString();
            if (name == "EvalFile" || name == "EvalFileSmall") {
                const auto path = dir.path() + "/" + d["default"].toString();
                check(QFileInfo(path).isFile(), "Stockfish exports its declared NNUE networks");
                options[name] = path;
            }
        }
        probe.shutdown();
        const auto path = dir.path() + "/analysis.sqlite";
        t64::Database db(path);
        QVariantMap profile{
            {"enginePath", enginePath}, {"milliseconds", 100}, {"hash", 16}, {"multiPV", 2}, {"threads", 1},
            {"uciOptions", options}};
        const QString pgn = "[White \"Analysis fixture\"]\n[Annotator \"Original author\"]\n"
                            "1.e4 {original note} e5 (1...c5 $1 2.Nf3) 2.Nf3 *";
        const auto original = t64::parsePgn(pgn.toStdString()).front();
        t64::AnalysisQueue queue;
        queue.setLibrary(path);
        check(waitFor([&] { return queue.state()["ready"].toBool(); }),
              "Worker opens its own SQLite connection");
        {
            t64::AnalysisQueue second;
            second.setLibrary(path);
            check(waitFor([&] { return !second.state()["error"].toString().isEmpty(); }),
                  "Second process lock reported");
            check(!second.state()["ready"].toBool(), "Another owner cannot run the same library queue");
        }
        queue.enqueue(pgn, profile, true);
        auto id = added(queue, 1);
        queue.resume(id);
        queue.resume(id); // These must not stop or restart an active search.
        complete(queue, id);
        const auto all = db.analysisResults(id);
        check(all.size() == int(original.nodes.size()) && db.analysisJob(id)["done"].toInt() == all.size(),
              "Every reachable branch position is saved once");
        check(db.analysisJob(id)["identity"].toMap()["cacheable"].toBool(),
              "Explicit NNUE enables verified cache");
        for (const auto &v : all) {
            const auto row = v.toMap();
            check(row["lines"].toList().size() == 2, "Real MultiPV survives completion and persistence");
            for (const auto &pv : row["lines"].toList()) {
                auto line = pv.toMap();
                check(line["scoreType"] == "cp" && line["wdl"].toList().size() == 3 &&
                          line["nodes"].toULongLong() > 0,
                      "Exact score, WDL and provenance counters");
            }
        }
        const auto out = dir.path() + "/annotated.pgn";
        queue.exportPgn(id, out);
        check(waitFor([&] { return QFileInfo(out).size() > 0; }), "Export finishes off the GUI thread");
        QFile result(out);
        check(result.open(QIODevice::ReadOnly), "Read exported PGN");
        auto annotated = t64::parsePgn(result.readAll().toStdString()).front();
        const auto originalLine = original.mainline(), newLine = annotated.mainline();
        check(originalLine.size() == newLine.size(), "PV export must not extend the played main line");
        for (size_t i = 0; i < originalLine.size(); ++i)
            check(original.nodes[originalLine[i]].uci == annotated.nodes[newLine[i]].uci,
                  "Original main line preserved in annotated export");
        check(annotated.tags["Annotator"] == "Original author" &&
                  annotated.nodes[1].comment.find("original note") != std::string::npos &&
                  annotated.nodes[1].comment.find("[%eval") != std::string::npos &&
                  !annotated.tags["Trait64EngineSHA256"].empty(),
              "Notes, author and engine provenance survive export");
        queue.enqueue(pgn, profile, true);
        const auto cachedId = added(queue, 2);
        complete(queue, cachedId);
        for (const auto &row : db.analysisResults(cachedId))
            check(row.toMap()["cached"].toBool(),
                  "Identical history and settings reuse verified evaluations");
        auto changed = profile;
        changed["milliseconds"] = 150;
        queue.enqueue(pgn, changed, false);
        auto mainId = added(queue, 3);
        complete(queue, mainId);
        check(db.analysisResults(mainId).size() == int(originalLine.size()), "Main-line-only scope");
        for (const auto &row : db.analysisResults(mainId))
            check(!row.toMap()["cached"].toBool(), "Different search budget cannot reuse weaker results");
        const auto fen = QString::fromStdString(original.nodes[2].fen);
        const auto transposed = QString::fromStdString(t64::Game(fen.toStdString()).pgn());
        queue.enqueue(transposed, profile, true);
        auto historyId = added(queue, 4);
        complete(queue, historyId);
        check(!db.analysisResults(historyId).first().toMap()["cached"].toBool(),
              "Same FEN with different root/history has a different cache key");

        // Pause, destroy the owner, reopen and resume the same job without duplicate results.
        changed["milliseconds"] = 1000;
        queue.enqueue(pgn, changed, true);
        auto pausedId = added(queue, 5);
        check(waitFor([&] { return job(queue, pausedId)["done"].toInt() >= 1; }), "First durable checkpoint");
        queue.pause(pausedId);
        check(waitFor([&] { return job(queue, pausedId)["status"] == "paused"; }), "Pause stops active UCI");
        const auto done = db.analysisJob(pausedId)["done"].toInt();
        check(done > 0 && done < int(original.nodes.size()), "Pause preserves partial progress");
        queue.shutdown();
        queue.setLibrary(path);
        check(waitFor([&] { return job(queue, pausedId)["status"] == "paused"; }),
              "Pause survives a restart");
        queue.resume(pausedId);
        complete(queue, pausedId);
        check(db.analysisResults(pausedId).size() == int(original.nodes.size()),
              "Resume writes no duplicates");

        // Mate-in-one and terminal mate remain typed; no invented 10000-centipawn loss.
        t64::Game mate("7k/6pp/8/8/8/8/6PP/5RK1 w - - 0 1");
        mate.play(0, "f1f8");
        queue.enqueue(QString::fromStdString(mate.pgn()), profile, true);
        auto mateId = added(queue, 6);
        complete(queue, mateId);
        const auto mates = db.analysisResults(mateId);
        check(mates[0].toMap()["lines"].toList()[0].toMap()["scoreType"] == "mate",
              "Stockfish finds actual mate");
        const auto terminal = mates[1].toMap()["lines"].toList()[0].toMap();
        check(terminal["terminal"].toBool() && terminal["scoreType"] == "mate" && terminal["cp"].isNull() &&
                  terminal["winner"].toInt() == 1,
              "Terminal mate is correctly oriented and has no cp");

        // A changed binary cannot silently mix old and new engine evaluations.
        const auto copy = dir.path() + "/stockfish-copy";
        check(QFile::copy(enginePath, copy), "Copy engine identity fixture");
        changed["enginePath"] = copy;
        changed["milliseconds"] = 600;
        queue.enqueue(pgn, changed, true);
        auto changedId = added(queue, 7);
        check(waitFor([&] { return job(queue, changedId)["done"].toInt() >= 1; }),
              "Engine identity checkpoint");
        queue.pause(changedId);
        check(waitFor([&] { return job(queue, changedId)["status"] == "paused"; }),
              "Pause before binary replacement");
        const auto before = db.analysisJob(changedId)["done"].toInt();
        QFile modified(copy);
        check(modified.open(QIODevice::Append), "Open copied binary");
        modified.write("changed");
        modified.close();
        queue.resume(changedId);
        check(waitFor([&] { return job(queue, changedId)["status"] == "failed"; }),
              "Changed executable refused");
        check(db.analysisJob(changedId)["done"].toInt() == before &&
                  job(queue, changedId)["error"].toString().contains("changé"),
              "Changed identity never overwrites old results");
        t64::Game frc("k7/8/8/8/8/8/8/6KR w H - 0 1", true);
        frc.play(0, "g1h1");
        queue.enqueue(QString::fromStdString(frc.pgn()), profile, true);
        auto frcId = added(queue, 8);
        complete(queue, frcId);
        check(db.analysisResults(frcId).size() == 2,
              "Chess960 analysis includes a legal stationary-king castle");
        t64::Game longGame;
        int node = 0;
        const std::vector<std::string> cycle{"g1f3", "g8f6", "f3g1", "f6g8"};
        for (int i = 0; i < 208; ++i)
            node = longGame.play(node, cycle[i % 4]);
        queue.enqueue(QString::fromStdString(longGame.pgn()), profile, true);
        auto pagesId = added(queue, 9);
        complete(queue, pagesId);
        queue.select(pagesId);
        check(waitFor([&] {
                  return queue.state()["selected"].toLongLong() == pagesId && queue.results().size() == 200 &&
                         queue.state()["hasNext"].toBool();
              }),
              "First result page is bounded and announces the next page");
        queue.select(pagesId, 200);
        check(waitFor([&] { return queue.results().size() == 9 && !queue.state()["hasNext"].toBool(); }),
              "Second result page has exact remaining positions");
        for (const auto &study : std::vector<t64::Game>{
                 t64::Game("4k3/8/8/8/8/8/8/R3K3 w - - 100 1"),
                 t64::parsePgn("1.Nf3 Nf6 2.Ng1 Ng8 3.Nf3 Nf6 4.Ng1 Ng8 *").front()}) {
            const int count = queue.jobs().size();
            queue.enqueue(QString::fromStdString(study.pgn()), profile, false);
            const auto claimId = added(queue, count + 1);
            complete(queue, claimId);
            const auto lines = db.analysisResults(claimId).last().toMap()["lines"].toList();
            check(!lines.empty() && !lines.first().toMap()["terminal"].toBool() &&
                      lines.first().toMap()["nodes"].toULongLong() > 0,
                  "A claimable draw still receives a real engine evaluation, not a fabricated terminal score");
        }
        queue.shutdown();
        const auto first = all.first().toMap();
        db.setAnalysisStatus(id, "running");
        db.saveAnalysisResult(id, 0, first["node"].toInt(), first["fen"].toString(), first["lines"].toList());
        check(db.analysisJob(id)["done"].toInt() == all.size(),
              "Retrying a committed result cannot double-count progress");
        db.setAnalysisStatus(id, "completed");

        // Actually kill a separate application process after its first SQLite commit.
        const auto crashPath = dir.path() + "/crash.sqlite";
        QProcess child;
        child.start(app.applicationFilePath(), {"--crash-worker", crashPath, enginePath});
        check(child.waitForStarted(), "Start crash-recovery process");
        QByteArray output;
        check(waitFor(
                  [&] {
                      output += child.readAllStandardOutput();
                      return output.contains("CHECKPOINT");
                  },
                  15000),
              "Child writes a checkpoint with real Stockfish");
        child.kill();
        check(child.waitForFinished(3000), "Kill application mid-analysis");
        {
            t64::Database crash(crashPath);
            const auto crashed = crash.analysisJobs().first().toMap();
            check(crashed["status"] == "running" && crashed["done"].toInt() > 0,
                  "Crash leaves a recoverable durable checkpoint");
            t64::AnalysisQueue recovery;
            recovery.setLibrary(crashPath);
            const auto crashId = crashed["id"].toLongLong();
            check(waitFor([&] { return job(recovery, crashId)["status"] == "paused"; }),
                  "Stale process lock recovered");
            recovery.resume(crashId);
            complete(recovery, crashId);
            check(crash.analysisJob(crashId)["done"].toInt() == 7 &&
                      crash.analysisResults(crashId).size() == 7 && crash.integrity() == "ok",
                  "Crash recovery finishes every position exactly once");
        }
        // Version 6 migration creates a pre-migration backup, preserving existing games.
        const auto oldPath = dir.path() + "/old.sqlite";
        db.save(original);
        db.backup(oldPath);
        {
            auto old = QSqlDatabase::addDatabase("QSQLITE", "analysis-migration");
            old.setDatabaseName(oldPath);
            check(old.open(), "Open v6 fixture");
            QSqlQuery q(old);
            for (const auto &sql : QStringList{"DROP TABLE analysis_results", "DROP TABLE analysis_jobs",
                                               "DROP TABLE analysis_cache", "PRAGMA user_version=6"})
                check(q.exec(sql), "Prepare v6 schema");
            q.finish();
            old.close();
        }
        QSqlDatabase::removeDatabase("analysis-migration");
        t64::Database migrated(oldPath);
        check(migrated.stats()["games"].toInt() == 1 && migrated.analysisJobs().empty() &&
                  migrated.integrity() == "ok",
              "Migration 6 to 7 preserves existing games");
        check(!QDir(dir.path()).entryList({"old.sqlite.before-v7-*.sqlite"}).empty(),
              "Pre-migration backup exists");
        std::cout << checks << " persistent-analysis checks passed with real Stockfish\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
