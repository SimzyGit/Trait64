// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include "unpack.h"

namespace {
// Les suffixes qu'une étude peut porter. Partagés par l'import d'un fichier et celui
// d'un dossier : le nom de l'étude se déduit du même suffixe qui l'a fait retenir.
const QStringList &readableStudySuffixes() {
    static const QStringList suffixes{".pgn", ".pgn.gz", ".pgn.zst", ".pgn.bz2", ".pgn.xz", ".pgn.zstd"};
    return suffixes;
}
} // namespace
#include <QDirIterator>
#include <QSaveFile>
#include <QtConcurrent>

namespace {
void require(bool ok, const QString &error) {
    if (!ok)
        throw std::runtime_error(error.toStdString());
}
} // namespace

QVariantMap Controller::repertoireTask() const {
    auto result = repertoireTask_;
    result["positions"] = repertoireProgress_->load();
    return result;
}

void Controller::initRepertoires() {
    repertoiresEnabled_ = true;
    repertoirePoll_.setInterval(100);
    connect(&repertoirePoll_, &QTimer::timeout, this, &Controller::repertoireTaskChanged);
    connect(&repertoireReader_, &QFutureWatcher<RepertoireResult>::finished, this, [this] {
        repertoireReadActive_ = false;
        const auto result = repertoireReader_.result();
        if (result.generation == repertoireGeneration_ && !result.canceled) {
            if (result.error.isEmpty()) {
                repertoires_ = result.overview["list"].toList();
                repertoireState_ = result.overview["detail"].toMap();
                selectedRepertoire_ = repertoireState_["id"].toLongLong();
            }
            repertoireState_["loading"] = false;
            repertoireState_["error"] = result.error;
            refreshRepertoires(false);
        }
        if (repertoireReadPending_ && repertoiresEnabled_)
            startRepertoireRead();
    });
    connect(&repertoireWriter_, &QFutureWatcher<RepertoireResult>::finished, this, [this] {
        const auto result = repertoireWriter_.result();
        repertoirePoll_.stop();
        busy_ = false;
        repertoireTask_["running"] = false;
        repertoireTask_["canceled"] = result.canceled;
        repertoireTask_["error"] = result.error;
        repertoireTask_["status"] = result.canceled ? "Traitement annulé. Aucun ajout partiel conservé."
                                    : result.error.isEmpty() ? result.message
                                                             : "Traitement interrompu.";
        emit busyChanged();
        emit repertoireTaskChanged();
        if (!result.error.isEmpty())
            emit message(result.error, true);
        else if (!result.canceled) {
            if (result.selected)
                selectedRepertoire_ = result.selected;
            if (!result.pgn.isEmpty()) {
                analysisQueue_.enqueue(result.pgn, result.config, true);
                emit showAnalysisQueue();
            } else
                emit message(result.message);
        }
        refreshRepertoires();
    });
}

void Controller::startRepertoireRead() {
    const auto path = db_->path();
    const auto id = selectedRepertoire_;
    const auto generation = repertoireGeneration_;
    const auto canceled = std::make_shared<std::atomic_bool>(false);
    cancelRepertoireRead_ = canceled;
    repertoireReadActive_ = true;
    repertoireReadPending_ = false;
    repertoireReader_.setFuture(QtConcurrent::run([path, id, generation, canceled] {
        RepertoireResult result;
        result.generation = generation;
        try {
            if (!canceled->load()) {
                t64::Database db(path);
                result.overview = db.repertoireOverview(id, canceled);
            }
        } catch (const std::exception &e) {
            result.error = QString::fromUtf8(e.what());
        }
        result.canceled = canceled->load();
        return result;
    }));
}

void Controller::cancelRepertoireOverview() {
    ++repertoireGeneration_;
    if (cancelRepertoireRead_)
        cancelRepertoireRead_->store(true);
    repertoireReadPending_ = false;
    repertoireState_["loading"] = false;
    repertoireState_["gapsLoaded"] = false;
    repertoireState_["gaps"] = QVariantList();
    repertoireState_["weaknesses"] = QVariantList();
    repertoireState_["error"] = "Actualisation annulée. Utilisez Actualiser pour recalculer.";
    emit repertoiresChanged();
}

void Controller::reloadRepertoires() {
    refreshRepertoires();
}

void Controller::startRepertoireTask(
    const QString &operation,
    std::function<RepertoireResult(t64::Database &, const t64::RepertoireProgress &)> work) {
    require(!busy_ && !trainingActive(), "Terminez le traitement ou l’entraînement.");
    const auto path = db_->path();
    const auto canceled = std::make_shared<std::atomic_bool>(false);
    const auto progress = std::make_shared<std::atomic<qint64>>(0);
    cancelTask_ = canceled;
    repertoireProgress_ = progress;
    repertoireTask_ = {{"running", true}, {"status", operation}, {"canceled", false}};
    busy_ = true;
    emit busyChanged();
    emit repertoireTaskChanged();
    repertoirePoll_.start();
    repertoireWriter_.setFuture(QtConcurrent::run([path, canceled, progress, work = std::move(work)] {
        RepertoireResult result;
        try {
            require(!canceled->load(), "Traitement du répertoire annulé.");
            t64::Database db(path);
            result = work(db, [canceled, progress](qint64 count) {
                progress->store(count);
                return !canceled->load();
            });
            // Once a transaction/file is committed, a late cancel must not report a rollback.
        } catch (const std::exception &e) {
            if (canceled->load())
                result.canceled = true;
            else
                result.error = QString::fromUtf8(e.what());
        }
        return result;
    }));
}

void Controller::createRepertoire(const QString &name, bool white) {
    try {
        const auto game = game_;
        const auto model = dirty_ ? 0 : gameId_;
        startRepertoireTask("Création du répertoire…",
                            [name, white, game, model](auto &db, const auto &progress) {
                                RepertoireResult result;
                                result.selected = db.createRepertoire(name, white, game, progress, model);
                                result.message = "Répertoire créé depuis l’étude et ses variantes.";
                                return result;
                            });
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::addStudyToRepertoire(qint64 id) {
    try {
        const auto game = game_;
        const auto model = dirty_ ? 0 : gameId_;
        startRepertoireTask("Fusion de l’étude…", [id, game, model](auto &db, const auto &progress) {
            db.addToRepertoire(id, game, model, progress);
            RepertoireResult result;
            result.selected = id;
            result.message = "L’étude et ses variantes ont été fusionnées dans le répertoire.";
            return result;
        });
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::importRepertoire(qint64 id, const QString &url, bool white) {
    try {
        const auto path = localPath(url);
        // Sans étude choisie, le fichier en crée une, nommée d'après lui : exiger d'en
        // avoir créé une au préalable n'apportait rien qu'une étape de plus.
        startRepertoireTask("Import du répertoire…", [id, path, white](auto &db, const auto &progress) {
            auto target = id;
            if (target <= 0) {
                auto name = QFileInfo(path).fileName();
                for (const auto &suffix : readableStudySuffixes())
                    if (name.endsWith(suffix, Qt::CaseInsensitive)) {
                        name.chop(suffix.size());
                        break;
                    }
                target = db.createRepertoire(name.isEmpty() ? QStringLiteral("Étude") : name, white,
                                             t64::Game{}, progress);
            }
            int count = 0;
            try {
                count =
                    db.importRepertoirePgn(target, t64::unpackIfCompressed(path, 16 * 1024 * 1024), progress);
            } catch (const std::exception &) {
                // L'étude naît avant la lecture : un fichier illisible laisserait
                // sinon une coquille vide derrière lui.
                if (target != id)
                    try {
                        db.deleteRepertoire(target);
                    } catch (const std::exception &) {
                    }
                throw;
            }
            RepertoireResult result;
            result.selected = target;
            result.message = tr("%1 partie(s) ajoutée(s), avec variantes et transpositions.").arg(count);
            return result;
        });
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::importRepertoireFolder(const QString &url, bool white) {
    try {
        const auto root = localPath(url);
        startRepertoireTask("Import du dossier d’études…", [root, white](auto &db, const auto &progress) {
            // En Croissant et Pawn Appétit rangent chaque répertoire dans son propre PGN,
            // souvent répartis en sous-dossiers. On reprend l'arborescence telle quelle :
            // un répertoire par fichier, nommé d'après son chemin relatif.
            // On compare les suffixes soi-même : les filtres de QDirIterator ignorent la
            // casse sous Windows et la respectent ailleurs, si bien qu'un « .PGN » passait
            // d'un système à l'autre. Et on retient au passage les bases d'un autre
            // logiciel, pour pouvoir dire ce qu'il y a plutôt que « rien trouvé ».
            const auto &readable = readableStudySuffixes();
            static const QStringList foreignBases{".db3", ".si4", ".si5",  ".sg4",
                                                  ".cbh", ".cbv", ".scid", ".sqlite"};
            QStringList files;
            QSet<QString> foreign;
            int visited = 0;
            QDirIterator walk(root, QDir::Files, QDirIterator::Subdirectories);
            while (walk.hasNext() && files.size() < 500 && ++visited <= 20000) {
                const auto path = walk.next();
                const auto name = QFileInfo(path).fileName().toLower();
                const auto matches = [&name](const QStringList &suffixes) {
                    return std::any_of(suffixes.cbegin(), suffixes.cend(),
                                       [&name](const QString &s) { return name.endsWith(s); });
                };
                if (matches(readable))
                    files << path;
                else if (matches(foreignBases))
                    foreign.insert(QFileInfo(name).suffix());
            }
            if (files.isEmpty() && !foreign.isEmpty()) {
                auto kinds = QStringList(foreign.cbegin(), foreign.cend());
                kinds.sort();
                require(false, tr("Ce dossier ne contient que des bases %1, que Trait64 ne lit pas "
                                  "encore. Exportez-les en PGN depuis votre logiciel.")
                                   .arg(kinds.join(", ")));
            }
            require(!files.isEmpty(), "Aucun fichier de parties dans ce dossier, ni dans ses "
                                      "sous-dossiers (PGN, au besoin compressé).");
            files.sort();
            const QDir base(root);
            RepertoireResult result;
            int created = 0, games = 0;
            QStringList refused;
            for (const auto &file : files) {
                auto name = base.relativeFilePath(file);
                // On retire le suffixe reconnu en entier : ne couper que la dernière
                // extension laissait « Française.pgn » comme nom d'une étude compressée.
                for (const auto &suffix : readable)
                    if (name.endsWith(suffix, Qt::CaseInsensitive)) {
                        name.chop(suffix.size());
                        break;
                    }
                // Un fichier illisible ne doit pas emporter la migration entière : on le met
                // de côté et on le nomme à la fin. Le répertoire naît avant la lecture, donc
                // un échec laisserait une coquille vide derrière lui : on la retire.
                qint64 id = 0;
                try {
                    id = db.createRepertoire(name, white, t64::Game{}, progress);
                    // 16 Mio dépliés : la même limite que l'import d'un fichier seul.
                    games +=
                        db.importRepertoirePgn(id, t64::unpackIfCompressed(file, 16 * 1024 * 1024), progress);
                    result.selected = id;
                    ++created;
                } catch (const std::exception &) {
                    if (id)
                        try {
                            db.deleteRepertoire(id);
                        } catch (const std::exception &) {
                        }
                    refused << name;
                }
            }
            require(created > 0, "Aucun fichier de ce dossier n’a pu être lu comme répertoire.");
            result.message = tr("%1 répertoire(s) créé(s), %2 partie(s) lue(s).").arg(created).arg(games);
            if (!refused.isEmpty())
                result.message += tr(" Laissés de côté : %1.").arg(refused.join(", "));
            return result;
        });
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::exportRepertoire(qint64 id, const QString &url) {
    try {
        const auto path = localPath(url);
        startRepertoireTask("Export du répertoire…", [id, path](auto &db, const auto &progress) {
            db.validateOutputPath(path);
            const auto game = db.exportRepertoire(id, progress);
            const auto bytes = QByteArray::fromStdString(game.pgn());
            require(progress(game.nodes.size()), "Export annulé.");
            QSaveFile output(path);
            require(output.open(QIODevice::WriteOnly), output.errorString());
            require(output.write(bytes) == bytes.size(), output.errorString());
            require(progress(game.nodes.size()), "Export annulé.");
            require(output.commit(), output.errorString());
            RepertoireResult result;
            result.message =
                "Répertoire exporté en PGN. Les transpositions sont signalées dans les commentaires.";
            return result;
        });
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::analyseRepertoire(qint64 id, int milliseconds, int profile) {
    try {
        require(analysisQueue_.state()["ready"].toBool(), "La file d’analyses n’est pas encore disponible.");
        const auto config = analysisConfig(milliseconds, profile);
        startRepertoireTask("Préparation de l’analyse du répertoire…",
                            [id, config](auto &db, const auto &progress) {
                                const auto game = db.exportRepertoire(id, progress);
                                RepertoireResult result;
                                result.pgn = QString::fromStdString(game.pgn());
                                require(progress(game.nodes.size()), "Analyse annulée.");
                                result.config = config;
                                result.message = "Copie du répertoire transmise à la file d’analyses.";
                                return result;
                            });
    } catch (const std::exception &e) {
        fail(e);
    }
}
