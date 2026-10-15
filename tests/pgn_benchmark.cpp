// SPDX-License-Identifier: AGPL-3.0-or-later
// Opt-in measurements on the caller's actual PGN, never synthetic game metadata.
#include "database.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSysInfo>
#include <algorithm>
#include <fstream>
#include <iostream>
#ifdef __linux__
#include <sys/resource.h>
#endif

namespace {
struct LimitReached {};
void need(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
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
         {"backup", "Also measure a backup and check its integrity"}});
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
        const auto path = args.value("database"), reportPath = args.value("report");
        need(path.isEmpty() || !QFileInfo::exists(path), "Refusing to overwrite an existing database");
        need(reportPath.isEmpty() || !QFileInfo::exists(reportPath), "Refusing to overwrite a report");
        need(path.isEmpty() || limit % 250 == 0, "Import --limit must be a multiple of 250");
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
            {
                t64::Database db(path);
                jobId = db.createImportJob(input);
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
                std::sort(times.begin(), times.end());
                report[name] = QJsonObject{{"median_ms", times[2]}, {"max_ms", times.back()}};
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
            }
            measure("opening_tree", [&] { db.explorer(QString::fromStdString(chess::constants::STARTPOS)); });
            clock.restart();
            report["integrity"] = db.integrity();
            report["integrity_ms"] = clock.elapsed();
            need(report["integrity"] == "ok", "Imported database failed integrity check");
            if (args.isSet("backup")) {
                need(!QFileInfo::exists(path + ".backup"), "Backup path already exists");
                clock.restart();
                db.backup(path + ".backup");
                report["backup_ms"] = clock.elapsed();
                t64::Database restored(path + ".backup");
                need(restored.integrity() == "ok" && restored.stats()["games"] == db.stats()["games"],
                     "Backup verification failed");
                report["backup_verified"] = true;
            }
            qint64 bytes = 0;
            for (const auto &suffix : QStringList{"", "-wal", "-shm"})
                bytes += QFileInfo(path + suffix).size();
            report["database_with_journals_bytes"] = bytes;
        }
#ifdef __linux__
        rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) == 0)
            report["peak_rss_kib"] = qint64(usage.ru_maxrss);
#endif
        const auto bytes = QJsonDocument(report).toJson();
        if (reportPath.isEmpty())
            std::cout << bytes.constData();
        else {
            QSaveFile output(reportPath);
            need(output.open(QIODevice::WriteOnly) && output.write(bytes) == bytes.size() && output.commit(),
                 "Cannot write report");
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
