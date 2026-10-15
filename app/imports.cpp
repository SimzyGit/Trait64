// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include <QtConcurrent>

QVariantMap Controller::importProgress() const {
    std::lock_guard guard(importView_->mutex);
    const auto &p = importView_->progress;
    const double rate =
        importView_->elapsed > 0 ? 1000.0 * (p.bytes - importView_->startBytes) / importView_->elapsed : 0;
    return {{"job", p.job},
            {"bytes", p.bytes},
            {"total", p.total},
            {"processed", p.processed},
            {"committed", p.committed},
            {"added", p.added},
            {"verifying", p.verifying},
            {"rate", rate},
            {"remaining", rate > 0 ? (p.total - p.bytes) / rate : 0}};
}

void Controller::importResumable(const QString &url) {
    try {
        if (busy_)
            throw std::runtime_error("Un traitement est déjà en cours.");
        const auto id = db_->createImportJob(localPath(url));
        resumeImport(id);
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::resumeImport(qint64 id, const QString &replacement) {
    try {
        if (busy_)
            throw std::runtime_error("Un traitement est déjà en cours.");
        const auto job = db_->importJob(id);
        if (job["status"] == "completed")
            throw std::runtime_error("Cet import est déjà terminé.");
        const auto file = replacement.isEmpty() ? QString() : localPath(replacement);
        if (trainingActive())
            finishTraining();
        busy_ = resumableImport_ = true;
        cancelTask_->store(false);
        taskProgress_->store(job["processed"].toInt());
        {
            std::lock_guard guard(importView_->mutex);
            importView_->progress = {id,
                                     0,
                                     job["bytes"].toLongLong(),
                                     job["processed"].toLongLong(),
                                     job["added"].toLongLong(),
                                     job["processed"].toLongLong(),
                                     job["offset"].toLongLong(),
                                     true};
            importView_->elapsed = 0;
            importView_->startBytes = job["offset"].toLongLong();
        }
        const int workers = std::clamp(settings_.value("importWorkers", 2).toInt(), 1, 8);
        const auto databasePath = db_->path();
        const auto canceled = cancelTask_;
        const auto progress = taskProgress_;
        const auto view = importView_;
        taskPoll_.start();
        emit busyChanged();
        refreshLibrary();
        importer_.setFuture(QtConcurrent::run([=] {
            ImportResult r;
            r.operation = "resumable";
            QElapsedTimer elapsed;
            try {
                t64::Database database(databasePath);
                r.job = database.runImportJob(
                    id,
                    [&](const t64::ImportProgress &p) {
                        std::lock_guard guard(view->mutex);
                        if (!p.verifying && !elapsed.isValid()) {
                            elapsed.start();
                            view->startBytes = p.bytes;
                        }
                        view->progress = p;
                        view->elapsed = elapsed.isValid() ? elapsed.elapsed() : 0;
                        progress->store(int(p.processed));
                        return !canceled->load();
                    },
                    file, 250, workers);
                r.error = r.job["error"].toString();
                r.count = r.job["added"].toInt();
                r.duplicates = r.job["processed"].toInt() - r.count;
            } catch (const std::exception &e) {
                r.error = QString::fromUtf8(e.what());
            }
            return r;
        }));
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::forgetImport(qint64 id) {
    try {
        if (busy_)
            throw std::runtime_error("Attendez la fin du traitement.");
        db_->forgetImportJob(id);
        refreshLibrary();
    } catch (const std::exception &e) {
        fail(e);
    }
}
