// SPDX-License-Identifier: AGPL-3.0-or-later
// Opt-in measurements on the caller's actual PGN, never synthetic game metadata.
#include "database.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>
#include <algorithm>
#include <condition_variable>
#include <fstream>
#include <future>
#include <iostream>
#include <thread>
#ifdef __linux__
#include <sys/resource.h>
#endif

namespace {
struct LimitReached {};
void need(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
QString newOutput(const QString &path) {
    const QFileInfo file(path);
    need(!file.exists() && !file.isSymLink(), "Refusing to overwrite an existing output");
    const auto directory = file.dir().canonicalPath();
    need(!directory.isEmpty(), "Output parent directory must exist");
    return QDir(directory).filePath(file.fileName());
}
QJsonObject distribution(std::vector<double> times) {
    QJsonArray samples;
    for (auto t : times)
        samples.append(t);
    std::sort(times.begin(), times.end());
    QJsonObject out{{"samples", qint64(times.size())}};
    if (!times.empty()) {
        out["median_ms"] = times[times.size() / 2];
        out["p95_ms"] = times[(times.size() * 95 + 99) / 100 - 1];
        out["max_ms"] = times.back();
    }
    // Ordinary reads have five samples; the long concurrent probe keeps only
    // its summary so reports stay small even after hours of import.
    if (times.size() <= 5)
        out["samples_ms"] = samples;
    return out;
}
// A second connection exercises readers during the writer's actual transactions.
// This measures database contention, not the rendering latency of the Qt UI.
class ReadProbe {
  public:
    ReadProbe(const QString &path, bool enabled) {
        if (!enabled)
            return;
        std::promise<void> ready;
        auto initialized = ready.get_future();
        worker_ = std::thread([this, path, ready = std::move(ready)]() mutable {
            bool announced = false;
            try {
                t64::Database reader(path);
                ready.set_value();
                announced = true;
                for (;;) {
                    QElapsedTimer timer;
                    timer.start();
                    reader.search({}, {}, false, {}, false, {{"limit", 200}});
                    times_.push_back(timer.nsecsElapsed() / 1e6);
                    std::unique_lock lock(mutex_);
                    if (wake_.wait_for(lock, std::chrono::milliseconds(100), [this] { return stopped_; }))
                        break;
                }
            } catch (...) {
                error_ = std::current_exception();
                if (!announced)
                    ready.set_exception(error_);
            }
        });
        try {
            initialized.get();
        } catch (...) {
            stop();
            throw;
        }
    }
    ~ReadProbe() { stop(); }
    QJsonObject result() {
        stop();
        if (error_)
            std::rethrow_exception(error_);
        return distribution(times_);
    }

  private:
    void stop() {
        {
            std::lock_guard lock(mutex_);
            stopped_ = true;
        }
        wake_.notify_all();
        if (worker_.joinable())
            worker_.join();
    }
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stopped_ = false;
    std::vector<double> times_;
    std::exception_ptr error_;
    std::thread worker_;
};
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCommandLineParser args;
    args.setApplicationDescription("Trait64: real-PGN parsing/import/search benchmark (opt-in)");
    args.addHelpOption();
    args.addOptions(
        {{{"i", "input"}, "Uncompressed PGN file", "file"},
         {"database", "New database path for import mode; omit for parse mode", "file"},
         {"threads", "Parser workers (1..8)", "number", "1"},
         {"limit", "Maximum parsed games or committed import games (multiple of 250)", "number", "0"},
         {"pause-at", "Pause, close and resume once after this committed count", "number", "0"},
         {"source-url", "Source provenance for the report (no download)", "url"},
         {"report", "New JSON report file; defaults to stdout", "file"},
         {"backup", "Measure backup, restore to a new file, and verify both"},
         {"reindex", "Also measure a full transactional rebuild of all position indexes"},
         {"probe-reads", "Measure a separate reader during import (one page every 100 ms)"}});
    args.process(app);
    try {
        const auto input = QFileInfo(args.value("input")).canonicalFilePath();
        need(!input.isEmpty(), "An existing --input PGN is required");
        bool ok;
        const int workers = args.value("threads").toInt(&ok);
        need(ok && workers >= 1 && workers <= 8, "--threads must be 1..8");
        const qint64 limit = args.value("limit").toLongLong(&ok);
        need(ok && limit >= 0, "Invalid --limit");
        const qint64 pauseAt = args.value("pause-at").toLongLong(&ok);
        need(ok && pauseAt >= 0 && (!limit || pauseAt < limit), "Invalid --pause-at");
        const auto path = args.isSet("database") ? newOutput(args.value("database")) : QString{};
        const auto reportPath = args.isSet("report") ? newOutput(args.value("report")) : QString{};
        need(path.isEmpty() || limit % 250 == 0, "Import --limit must be a multiple of 250");
        need(!path.isEmpty() || (!pauseAt && !args.isSet("backup") && !args.isSet("reindex") &&
                                !args.isSet("probe-reads")),
             "--pause-at, --backup, --reindex and --probe-reads require --database");
        // Validate all destinations before creating SQLite files. Resolving the existing
        // parent also catches aliases through symlinked directories and '..' components.
        if (!path.isEmpty()) {
            QStringList destinations{path, path + "-wal", path + "-shm", path + "-journal",
                                     path + ".import.lock"};
            if (args.isSet("backup"))
                for (const auto &copy : QStringList{".backup", ".restored"})
                    for (const auto &suffix : QStringList{"", "-wal", "-shm", "-journal"})
                        destinations.push_back(path + copy + suffix);
            for (const auto &destination : destinations)
                need(newOutput(destination) != reportPath, "Report overlaps a database or journal path");
        }
        QElapsedTimer clock;
        clock.start();
        QFile source(input);
        need(source.open(QIODevice::ReadOnly), "Cannot read input");
        QCryptographicHash fileHash(QCryptographicHash::Sha256);
        need(fileHash.addData(&source), "Cannot hash input");
        const auto hashTime = clock.elapsed();
        source.close();
        QJsonObject report{{"input", input},
                           {"source_url", args.value("source-url")},
                           {"input_sha256", QString::fromLatin1(fileHash.result().toHex())},
                           {"input_bytes", QFileInfo(input).size()},
                           {"hash_ms", hashTime},
                           {"workers", workers},
                           {"concurrent_reader", args.isSet("probe-reads")},
                           {"limit", limit},
                           {"os", QSysInfo::prettyProductName()},
                           {"cpu_arch", QSysInfo::currentCpuArchitecture()},
                           {"qt", QT_VERSION_STR},
                           {"version", T64_VERSION}};
        clock.restart();
        if (path.isEmpty()) {
            std::ifstream stream(QFile::encodeName(input).constData(), std::ios::binary);
            qint64 count = 0, nodes = 0;
            QCryptographicHash canonical(QCryptographicHash::Sha256);
            try {
                t64::readPgnParallel(stream, workers, [&](t64::Game &&game, uint64_t) {
                    ++count;
                    nodes += qint64(game.nodes.size());
                    canonical.addData(QByteArray::fromStdString(game.pgn()));
                    if (limit && count >= limit)
                        throw LimitReached{};
                });
            } catch (const LimitReached &) {
            }
            report["mode"] = "parse + canonical PGN hash (no database)";
            report["elapsed_ms"] = clock.elapsed();
            report["games"] = count;
            report["nodes"] = nodes;
            report["canonical_sha256"] = QString::fromLatin1(canonical.result().toHex());
        } else {
            qint64 jobId;
            QVariantMap job;
            std::unique_ptr<ReadProbe> probe;
            {
                t64::Database db(path);
                jobId = db.createImportJob(input);
                probe = std::make_unique<ReadProbe>(path, args.isSet("probe-reads"));
                job = db.runImportJob(
                    jobId,
                    [&](const t64::ImportProgress &p) {
                        return p.verifying ||
                               ((!pauseAt || p.committed < pauseAt) && (!limit || p.committed < limit));
                    },
                    {}, 250, workers);
            }
            report["first_pass_ms"] = clock.elapsed();
            if (pauseAt && job["status"] == "paused") {
                t64::Database reopened(path);
                report["pause_committed"] = job["processed"].toLongLong();
                job = reopened.runImportJob(
                    jobId,
                    [&](const t64::ImportProgress &p) {
                        return p.verifying || !limit || p.committed < limit;
                    },
                    {}, 250, workers);
            }
            report["elapsed_ms"] = clock.elapsed();
            if (args.isSet("probe-reads"))
                report["concurrent_first_page_200"] = probe->result();
            probe.reset();
            report["mode"] = "checkpointed import including file verification, full positions and indexes";
            report["job"] = QJsonObject::fromVariantMap(job);
            need(job["status"] == "completed" || (job["status"] == "paused" && limit > 0),
                 qPrintable(job["error"].toString()));
            clock.restart();
            t64::Database db(path);
            report["reopen_ms"] = clock.nsecsElapsed() / 1e6;
            report["games"] = db.stats()["games"].toLongLong();
            auto measure = [&](const QString &name, auto operation) {
                std::vector<double> times;
                for (int i = 0; i < 5; ++i) {
                    clock.restart();
                    operation();
                    times.push_back(clock.nsecsElapsed() / 1e6);
                }
                report[name] = distribution(std::move(times));
            };
            QVariantList page;
            measure("first_page_200", [&] { page = db.search({}, {}, false, {}, false, {{"limit", 200}}); });
            if (!page.empty()) {
                const auto cursor = page.last().toMap();
                const auto player = page.first().toMap()["white"];
                measure("next_page_200",
                        [&] { db.search({}, {}, false, {}, false, {{"cursor", cursor}, {"limit", 200}}); });
                measure("exact_player", [&] {
                    db.search({}, {}, false, {}, false, {{"white", player}, {"exactNames", true}});
                });
                measure("load_one_game", [&] { db.load(page.first().toMap()["id"].toLongLong()); });
                measure("player_fragment", [&] { db.search(player.toString().left(4)); });
                const auto game = db.load(page.first().toMap()["id"].toLongLong());
                const auto lastFen = QString::fromStdString(game.nodes[game.mainline().back()].fen);
                measure("last_position", [&] { db.search({}, {}, false, lastFen); });
            }
            measure("opening_tree", [&] { db.explorer(QString::fromStdString(chess::constants::STARTPOS)); });
            if (args.isSet("reindex")) {
                clock.restart();
                report["reindexed_games"] = db.rebuildIndexes();
                report["reindex_ms"] = clock.elapsed();
            }
            clock.restart();
            report["integrity"] = db.integrity();
            report["integrity_ms"] = clock.elapsed();
            need(report["integrity"] == "ok", "Imported database failed integrity check");
            if (args.isSet("backup")) {
                need(!QFileInfo::exists(path + ".backup"), "Backup path already exists");
                clock.restart();
                db.backup(path + ".backup");
                report["backup_ms"] = clock.elapsed();
                {
                    t64::Database backup(path + ".backup");
                    need(backup.integrity() == "ok" && backup.stats()["games"] == db.stats()["games"],
                         "Backup verification failed");
                }
                report["backup_verified"] = true;
                // Copy the closed backup into an independent new database, then
                // open and validate it. Never restore over a user's existing file.
                clock.restart();
                need(QFile::copy(path + ".backup", path + ".restored"), "Cannot restore backup");
                report["restore_copy_ms"] = clock.elapsed();
                t64::Database restored(path + ".restored");
                need(restored.integrity() == "ok" && restored.stats() == db.stats(),
                     "Restoration verification failed");
                if (!page.empty()) {
                    const auto id = page.first().toMap()["id"].toLongLong();
                    need(restored.load(id).pgn() == db.load(id).pgn(), "Restored PGN differs");
                }
                report["restore_ms"] = clock.elapsed();
                report["restore_verified"] = true;
            }
            qint64 bytes = 0;
            for (const auto &suffix : QStringList{"", "-wal", "-shm"})
                bytes += QFileInfo(path + suffix).size();
            report["database_with_journals_bytes"] = bytes;
        }
        const double seconds = report["elapsed_ms"].toDouble() / 1000.0;
        if (seconds > 0)
            report["games_per_second"] = report["games"].toDouble() / seconds;
#ifdef __linux__
        QFile status("/proc/self/status");
        if (status.open(QIODevice::ReadOnly)) {
            const auto lines = status.readAll().split('\n');
            for (const auto &line : lines)
                if (line.startsWith("VmRSS:"))
                    report["end_rss_kib"] = line.simplified().split(' ').value(1).toLongLong();
        }
        rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) == 0)
            report["peak_rss_kib"] = qint64(usage.ru_maxrss);
#endif
        const auto bytes = QJsonDocument(report).toJson();
        if (reportPath.isEmpty())
            std::cout << bytes.constData();
        else {
            // NewOnly also protects a report created by another process during the run.
            QFile output(reportPath);
            need(output.open(QIODevice::WriteOnly | QIODevice::NewOnly) &&
                     output.write(bytes) == bytes.size() && output.flush(),
                 "Cannot write report");
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
