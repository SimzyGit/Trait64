// SPDX-License-Identifier: AGPL-3.0-or-later
#include "database.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QProcess>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <cstdlib>
#include <iostream>

namespace {
int checks = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
// Un arrêt brutal n'a ni message ni pile exploitable sur la machine d'intégration : ce
// jalon, vidé aussitôt, encadre la dernière étape atteinte. Il reste discret quand tout
// va bien et devient la seule trace utile quand le processus meurt.
void milestone(const char *where) {
    std::cout << "[step] " << where << std::endl;
}
void write(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    check(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(), "Write PGN fixture");
}
template <class F> void rejects(F action) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception &) {
        failed = true;
    }
    check(failed, "Invalid operation accepted");
}
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (app.arguments().value(1) == "--crash") {
        t64::Database db(app.arguments().at(2));
        auto job = db.createImportJob(app.arguments().at(3));
        db.runImportJob(
            job,
            [](const t64::ImportProgress &p) {
                if (!p.verifying && p.processed == 5)
                    std::_Exit(23); // Real process death with a transaction and lock still open.
                return true;
            },
            {}, 3, 4);
        return 24;
    }
    try {
        QTemporaryDir dir;
        check(dir.isValid(), "Temporary directory");
        const auto path = dir.filePath("games.pgn"), database = dir.filePath("library.sqlite");
        QByteArray pgn;
        for (int i = 0; i < 12; ++i)
            pgn += "[Event \"Partie " + QByteArray::number(i) +
                   "\"]\r\n{Début} 1.e4 (1.d4 d5) e5 2.Nf3 *\r\n\r\n";
        write(path, pgn);
        t64::Database db(database);
        milestone("resumable import, pause and resume");
        auto id = db.createImportJob(path);
        // Interrompre un import à quatre lecteurs fait remonter une exception à travers
        // tout le lecteur parallèle : c'est le chemin qui plantait sous Windows, les
        // futures étant détruites avant que les fils ne soient joints.
        milestone("  pausing a four-reader import mid-flight");
        auto paused = db.runImportJob(
            id, [](const t64::ImportProgress &p) { return p.verifying || p.processed < 5; }, {}, 3, 4);
        milestone("  pause returned");
        check(paused["status"] == "paused" && paused["processed"].toInt() == 3 &&
                  db.stats()["games"].toInt() == 3 && db.importHistory().empty(),
              "Pause keeps only atomically committed batches, without a completed history entry");
        const auto offset = paused["offset"].toLongLong();
        check(offset > 0 && offset < pgn.size() && paused["sha256"].toString().size() == 64,
              "Checkpoint stores byte offset and full SHA-256");
        {
            milestone("  reopening the database");
            t64::Database reopened(database);
            bool resumedAtCheckpoint = false;
            milestone("  resuming from the checkpoint");
            auto done = reopened.runImportJob(id, [&](const t64::ImportProgress &p) {
                if (!p.verifying && p.processed == 3 && p.bytes == offset)
                    resumedAtCheckpoint = true;
                return true;
            });
            check(resumedAtCheckpoint && done["status"] == "completed" && done["processed"].toInt() == 12 &&
                      done["added"].toInt() == 12 && done["offset"].toLongLong() == pgn.size(),
                  "Reopen resumes at checkpoint and completes every game");
            milestone("  re-running a completed job");
            reopened.runImportJob(id);
            check(reopened.importHistory().size() == 1 && reopened.stats()["games"].toInt() == 12,
                  "Completed job cannot duplicate history or games");
            check(reopened.sources().first().toMap()["games"].toInt() == 12 && reopened.integrity() == "ok",
                  "Source links, positions and counts survive resume");
        }
        milestone("  resumable phase complete");
        milestone("duplicate import");
        auto duplicate = db.runImportJob(db.createImportJob(path));
        check(duplicate["added"].toInt() == 0 && duplicate["processed"].toInt() == 12,
              "Repeated import reports exact duplicates");
        milestone("source file changed under a paused job");
        auto changedId = db.createImportJob(path);
        db.runImportJob(changedId, [](const t64::ImportProgress &p) { return p.verifying; });
        auto changed = pgn;
        changed.replace("Partie 0", "Partie X"); // Same size: metadata-only checks are insufficient.
        write(path, changed);
        check(db.runImportJob(changedId)["status"] == "failed" && db.stats()["games"].toInt() == 12,
              "Same-size altered PGN is refused by SHA-256 without changing games");
        const auto relocated = dir.filePath("déplacé.pgn");
        write(relocated, pgn);
        check(db.runImportJob(changedId, {}, relocated)["status"] == "completed",
              "Identical relocated original can be selected to resume");
        milestone("locked source file");
        auto locked = db.createImportJob(relocated);
        {
            QLockFile lock(QFileInfo(database).canonicalFilePath() + ".import.lock");
            lock.setStaleLockTime(0);
            check(lock.tryLock(), "Lock test fixture");
            rejects([&] { db.runImportJob(locked); });
            rejects([&] { db.forgetImportJob(locked); });
        }
        auto bad = dir.filePath("invalid.pgn");
        write(bad, pgn + "[Event \"Invalid\"]\n1.e5 *\n");
        // Quatre lecteurs sur un PGN volontairement invalide : une exception traverse la
        // pile pendant que les fils tournent. C'est l'étape qui plante sous Windows.
        milestone("parallel import of a malformed PGN with four readers");
        auto failed = db.runImportJob(db.createImportJob(bad), {}, {}, 5, 4);
        milestone("parallel import returned");
        check(failed["status"] == "failed" && failed["processed"].toInt() == 10 &&
                  !failed["error"].toString().isEmpty(),
              "Malformed game preserves earlier checkpoints and reports failure");
        milestone("empty PGN");
        auto empty = dir.filePath("empty.pgn");
        write(empty, "\n \r\n");
        check(db.runImportJob(db.createImportJob(empty))["status"] == "failed", "Empty PGN rejected");
        milestone("pause during verification");
        auto verifyPause = db.createImportJob(relocated);
        check(db.runImportJob(verifyPause, [](const t64::ImportProgress &) { return false; })["status"] ==
                  "paused",
              "Pause during fingerprint verification is recoverable");
        db.forgetImportJob(verifyPause);
        check(db.stats()["games"].toInt() == 12, "Forgetting checkpoint never deletes games");
        // Mort réelle d'un processus pendant un lot. Sous Windows, le verrouillage de
        // fichier est contraignant là où POSIX est permissif : c'est un candidat sérieux
        // pour le plantage, d'où un jalon de part et d'autre du redémarrage.
        milestone("process death during a batch");
        const auto crashDb = dir.filePath("crash.sqlite");
        QProcess worker;
        worker.start(QCoreApplication::applicationFilePath(), {"--crash", crashDb, relocated});
        check(worker.waitForFinished(20000) && worker.exitCode() == 23, "Worker actually died during batch");
        milestone("reopening the database the dead process left behind");
        t64::Database recovered(crashDb);
        auto checkpoint = recovered.importJobs().first().toMap();
        check(recovered.stats()["games"].toInt() == 3 && checkpoint["processed"].toInt() == 3,
              "SQLite rolls back the incomplete batch after process death");
        milestone("resuming the checkpoint left by the dead process");
        auto recovery = recovered.runImportJob(checkpoint["id"].toLongLong());
        check(recovery["status"] == "completed" && recovered.stats()["games"].toInt() == 12 &&
                  recovery["added"].toInt() == 12 && recovered.integrity() == "ok",
              "Stale lock recovered, checkpoint resumed and all indexes remain valid after crash");
        std::cout << checks << " resumable import checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
