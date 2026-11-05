// SPDX-License-Identifier: AGPL-3.0-or-later
#include "queue.h"
#include "database.h"
#include "hardware.h"
#include "uci.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace t64 {
namespace {
void require(bool ok, const QString &message) {
    if (!ok)
        throw std::runtime_error(message.toStdString());
}
QString str(const std::string &text) {
    return QString::fromStdString(text);
}
QByteArray json(const QVariant &v) {
    return QJsonDocument::fromVariant(v).toJson(QJsonDocument::Compact);
}
QString digest(const QByteArray &data) {
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}
QString fileDigest(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Ressource du moteur illisible : " + path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    require(hash.addData(&file) && file.error() == QFileDevice::NoError, "Lecture du moteur interrompue.");
    return QString::fromLatin1(hash.result().toHex());
}
std::vector<int> nodesFor(const Game &game, bool all) {
    if (!all)
        return game.mainline();
    std::vector<int> nodes, pending{0};
    while (!pending.empty()) {
        const auto n = pending.back();
        pending.pop_back();
        nodes.push_back(n);
        const auto &children = game.nodes.at(n).children;
        for (auto it = children.rbegin(); it != children.rend(); ++it)
            pending.push_back(*it);
    }
    return nodes;
}
QString position(const Game &game, int node) {
    QStringList moves;
    for (int n = node; n > 0; n = game.nodes.at(n).parent)
        moves.prepend(str(game.nodes[n].uci));
    return "position fen " + str(game.nodes[0].fen) + (moves.empty() ? "" : " moves " + moves.join(' '));
}
QVariantMap normalizedProfile(QVariantMap profile) {
    QVariantMap result;
    const auto engine = QFileInfo(profile["enginePath"].toString());
    require(engine.isFile() && engine.isExecutable(), "Choisissez un moteur UCI dans les paramètres.");
    result["enginePath"] = engine.canonicalFilePath();
    result["threads"] = std::clamp(profile.value("threads", t64::recommendedThreads()).toInt(), 1, 32);
    result["hash"] = std::clamp(profile.value("hash", t64::recommendedHashMb()).toInt(), 16, 4096);
    result["multiPV"] = std::clamp(profile.value("multiPV", 3).toInt(), 1, 5);
    result["milliseconds"] = std::clamp(profile.value("milliseconds", 500).toInt(), 100, 10000);
    // 0 : la durée seule décide, comme avant. Au-delà, la profondeur décide et la durée plafonne.
    result["depth"] = std::clamp(profile.value("depth", 0).toInt(), 0, 99);
    result["syzygyPath"] = profile["syzygyPath"].toString();
    auto options = profile["uciOptions"].toMap();
    // Background analysis always uses full strength, without engine-side output files.
    for (const auto &name :
         QStringList{"Threads", "Hash", "MultiPV", "Skill Level", "UCI_Chess960", "SyzygyPath"})
        options.remove(name);
    options["UCI_LimitStrength"] = false;
    options["UCI_AnalyseMode"] = true;
    options["UCI_ShowWDL"] = true;
    options["Ponder"] = false;
    options["Debug Log File"] = QStringLiteral("");
    result["uciOptions"] = options;
    return result;
}
QVariantMap engineIdentity(const QVariantMap &config, const Uci &engine) {
    const auto path = config["enginePath"].toString();
    QVariantMap resources, effective;
    bool cacheable = config["syzygyPath"].toString().isEmpty();
    const auto custom = config["uciOptions"].toMap();
    for (const auto &v : engine.options()) {
        const auto descriptor = v.toMap();
        const auto name = descriptor["name"].toString(), type = descriptor["type"].toString();
        if (type == "button")
            continue;
        auto value = custom.value(name, descriptor["default"]);
        if (name == "Threads")
            value = config["threads"];
        if (name == "Hash")
            value = config["hash"];
        if (name == "MultiPV")
            value = config["multiPV"];
        if (name == "Skill Level")
            value = 20;
        if (name == "SyzygyPath")
            value = config["syzygyPath"];
        if (name == "UCI_Chess960")
            value = false; // Each position's variant is included in its key.
        effective[name] = Uci::optionValue(
            descriptor, value, QStringList{"Threads", "Hash", "MultiPV", "Skill Level"}.contains(name));
        if (type != "string" || value.toString().isEmpty() || value == "<empty>" || name == "Debug Log File")
            continue;
        if (name == "EvalFile" || name == "EvalFileSmall" || name.contains("file", Qt::CaseInsensitive)) {
            const auto resolved = QDir(QFileInfo(path).absolutePath()).absoluteFilePath(value.toString());
            if (QFileInfo(resolved).isFile())
                resources[name] = fileDigest(resolved);
            else {
                // A default NNUE may be embedded, or loaded from another directory. Do not guess.
                resources[name] = "unresolved:" + value.toString();
                cacheable = false;
            }
        } else if (name != "NumaPolicy")
            cacheable = false;
    }
    return {{"sha256", fileDigest(path)}, {"name", engine.name()},  {"options", effective},
            {"resources", resources},     {"cacheable", cacheable}, {"format", 1}};
}
QVariantList terminalScore(const chess::Board &board, const QString &fen) {
    const auto end = gameResult(board);
    if (end.first != chess::GameResultReason::CHECKMATE &&
        end.first != chess::GameResultReason::STALEMATE &&
        end.first != chess::GameResultReason::INSUFFICIENT_MATERIAL)
        return {};
    const bool mate = end.first == chess::GameResultReason::CHECKMATE;
    const int winner = mate ? (board.sideToMove() == chess::Color::WHITE ? -1 : 1) : 0;
    return {QVariantMap{{"rank", 1},
                        {"fen", fen},
                        {"scoreType", mate ? "mate" : "cp"},
                        {"cp", mate ? QVariant() : QVariant(0)},
                        {"mate", mate ? QVariant(0) : QVariant()},
                        {"winner", winner},
                        {"score", winner * 100.},
                        {"eval", mate ? (winner > 0 ? "+M0" : "−M0") : "0.00"},
                        {"depth", 0},
                        {"nodes", 0},
                        {"time", 0},
                        {"uci", ""},
                        {"san", ""},
                        {"bound", false},
                        {"terminal", true}}};
}
} // namespace

class AnalysisWorker : public QObject {
    Q_OBJECT
  public:
    explicit AnalysisWorker(QString path) : path_(std::move(path)) {}
    void guarded(const std::function<void()> &action) {
        try {
            require(db_ && owned_, "La file d’analyses n’est pas disponible : " + error_);
            action();
        } catch (const std::exception &e) {
            error_ = QString::fromUtf8(e.what());
            emit message(error_, true);
        }
        publish();
    }
    void open() {
        notify_ = new QTimer(this);
        notify_->setSingleShot(true);
        connect(notify_, &QTimer::timeout, this, &AnalysisWorker::publish);
        try {
            db_ = std::make_unique<Database>(path_);
            lock_ = std::make_unique<QLockFile>(QFileInfo(path_).canonicalFilePath() + ".analysis.lock");
            lock_->setStaleLockTime(0);
            require(lock_->tryLock(), "Une autre fenêtre Trait64 possède déjà cette file d’analyses.");
            owned_ = true;
            db_->recoverAnalyses();
            engine_ = std::make_unique<Uci>();
            connect(engine_.get(), &Uci::initialized, this, [this] {
                if (!active_)
                    return;
                try {
                    identity_ = engineIdentity(config_, *engine_);
                    db_->setAnalysisIdentity(active_, identity_);
                    next();
                } catch (const std::exception &e) {
                    fail(QString::fromUtf8(e.what()));
                }
            });
            connect(engine_.get(), &Uci::failed, this, [this](const QString &text) {
                if (active_)
                    fail(text);
            });
            connect(engine_.get(), &Uci::completed, this,
                    [this](const QString &fen, const QVariantList &lines) {
                        if (!active_ || index_ >= int(nodes_.size()) ||
                            fen != str(game_.nodes[nodes_[index_]].fen))
                            return;
                        try {
                            finish(lines, false);
                        } catch (const std::exception &e) {
                            fail(QString::fromUtf8(e.what()));
                        }
                    });
        } catch (const std::exception &e) {
            error_ = QString::fromUtf8(e.what());
            emit message(error_, true);
        }
        publish();
        schedule();
    }
    void close() {
        closing_ = true;
        if (active_) {
            const auto id = active_;
            active_ = 0;
            engine_->shutdown();
            try {
                db_->setAnalysisStatus(id, "paused");
            } catch (...) {
            }
        }
        engine_.reset();
        db_.reset();
        lock_.reset();
        owned_ = false;
    }
    void enqueue(const QString &pgn, const QVariantMap &profile, bool variations, qint64 gameId) {
        guarded([&] {
            require(pgn.size() <= 16 * 1024 * 1024, "Étude limitée à 16 Mio pour l’analyse.");
            auto games = parsePgn(pgn.toStdString());
            require(games.size() == 1, "Une seule étude est attendue.");
            const auto canonical = str(games.front().pgn());
            // Reparse the stored snapshot, so indices remain stable after export/restart.
            const auto snapshot = parsePgn(canonical.toStdString()).front();
            selected_ = db_->createAnalysisJob(canonical, normalizedProfile(profile), variations,
                                               int(nodesFor(snapshot, variations).size()), gameId);
            offset_ = 0;
            error_.clear();
            schedule();
        });
    }
    void select(qint64 id, int offset) {
        guarded([&] {
            db_->analysisJob(id, false);
            selected_ = id;
            offset_ = std::max(0, offset);
        });
    }
    void pause(qint64 id) {
        guarded([&] {
            const auto status = db_->analysisJob(id, false)["status"].toString();
            if (status == "completed")
                return;
            if (active_ == id) {
                active_ = 0;
                engine_->shutdown();
            }
            db_->setAnalysisStatus(id, "paused");
            schedule();
        });
    }
    void pauseAll() {
        guarded([&] {
            if (active_) {
                active_ = 0;
                engine_->shutdown();
            }
            for (const auto &v : db_->analysisJobs()) {
                const auto row = v.toMap();
                if (row["status"] == "queued" || row["status"] == "running")
                    db_->setAnalysisStatus(row["id"].toLongLong(), "paused");
            }
        });
    }
    void resume(qint64 id) {
        guarded([&] {
            const auto status = db_->analysisJob(id, false)["status"].toString();
            if (status == "running" || status == "queued" || status == "completed")
                return;
            db_->setAnalysisStatus(id, "queued");
            error_.clear();
            schedule();
        });
    }
    void forget(qint64 id) {
        guarded([&] {
            db_->forgetAnalysis(id);
            if (selected_ == id) {
                selected_ = 0;
                offset_ = 0;
            }
        });
    }
    void clearCache() {
        guarded([&] {
            db_->clearAnalysisCache();
            emit message("Cache vidé ; les résultats des analyses sont conservés.", false);
        });
    }
    void exportPgn(qint64 id, const QString &url, bool includeVariations) {
        guarded([&] {
            const QUrl parsed(url);
            require(parsed.isLocalFile() || parsed.scheme().isEmpty(), "Choisissez un fichier local.");
            const auto path = parsed.isLocalFile() ? parsed.toLocalFile() : url;
            db_->validateOutputPath(path);
            QSaveFile file(path);
            const auto data = annotated(id, includeVariations).toUtf8();
            require(file.open(QIODevice::WriteOnly), file.errorString());
            require(file.write(data) == data.size() && file.commit(), file.errorString());
            emit message("Analyse exportée avec ses évaluations et sa provenance.", false);
        });
    }
    void openStudy(qint64 id) {
        guarded([&] { emit studyReady(annotated(id, true)); });
    }
  signals:
    void view(const QVariantList &jobs, const QVariantList &results, const QVariantMap &state);
    void message(const QString &text, bool error);
    void studyReady(const QString &pgn);

  private:
    QString path_, error_, key_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<QLockFile> lock_;
    std::unique_ptr<Uci> engine_;
    QTimer *notify_ = nullptr;
    bool owned_ = false, closing_ = false, scheduled_ = false;
    quint64 epoch_ = 0;
    qint64 active_ = 0, selected_ = 0;
    int index_ = 0, offset_ = 0;
    Game game_;
    std::vector<int> nodes_;
    QVariantMap config_, identity_;
    void publish() {
        if (notify_)
            notify_->stop();
        QVariantList jobs, results;
        bool hasNext = false;
        QVariantMap selected;
        try {
            if (db_) {
                jobs = db_->analysisJobs();
                if (selected_) {
                    selected = db_->analysisJob(selected_, false);
                    selected.remove("pgn");
                    results = db_->analysisResults(selected_, offset_, 201);
                    hasNext = results.size() > 200;
                    if (hasNext)
                        results.removeLast();
                }
            }
        } catch (const std::exception &e) {
            error_ = QString::fromUtf8(e.what());
        }
        emit view(jobs, results,
                  {{"ready", owned_},
                   {"running", active_ != 0},
                   {"active", active_},
                   {"selected", selected_},
                   {"job", selected},
                   {"offset", offset_},
                   {"page", offset_ / 200 + 1},
                   {"hasNext", hasNext},
                   {"error", error_}});
    }
    void schedule() {
        if (scheduled_ || closing_ || !owned_)
            return;
        scheduled_ = true;
        QTimer::singleShot(0, this, [this] {
            scheduled_ = false;
            if (closing_ || active_)
                return;
            try {
                auto jobs = db_->analysisJobs();
                for (auto it = jobs.crbegin(); it != jobs.crend(); ++it) {
                    const auto row = it->toMap();
                    if (row["status"] == "queued") {
                        start(row["id"].toLongLong());
                        break;
                    }
                }
            } catch (const std::exception &e) {
                fail(QString::fromUtf8(e.what()));
            }
        });
    }
    void start(qint64 id) {
        ++epoch_;
        active_ = id;
        const auto job = db_->analysisJob(id);
        game_ = parsePgn(job["pgn"].toString().toStdString()).front();
        nodes_ = nodesFor(game_, job["variations"].toBool());
        index_ = job["done"].toInt();
        require(int(nodes_.size()) == job["total"].toInt() && index_ <= int(nodes_.size()),
                "Copie de la partie incohérente.");
        config_ = job["config"].toMap();
        db_->setAnalysisStatus(id, "running");
        publish();
        engine_->start(config_["enginePath"].toString(), config_["threads"].toInt(), config_["hash"].toInt(),
                       config_["multiPV"].toInt(), 20, config_["syzygyPath"].toString(),
                       config_["uciOptions"].toMap());
    }
    void next() {
        if (!active_ || closing_)
            return;
        if (index_ == int(nodes_.size())) {
            const auto id = active_;
            active_ = 0;
            engine_->shutdown();
            db_->setAnalysisStatus(id, "completed");
            publish();
            emit message("Analyse en arrière-plan terminée.", false);
            schedule();
            return;
        }
        const auto node = nodes_[index_];
        const auto fen = str(game_.nodes[node].fen), command = position(game_, node);
        key_ = identity_["cacheable"].toBool() ? digest(json(QVariantMap{{"identity", identity_},
                                                                         {"config", config_},
                                                                         {"fen", fen},
                                                                         {"position", command},
                                                                         {"chess960", game_.chess960}}))
                                               : QString();
        const auto terminal = terminalScore(game_.boardAt(node), fen);
        if (!terminal.empty()) {
            finish(terminal, false);
            return;
        }
        const auto cached = db_->cachedAnalysis(key_);
        // Older versions cached claimable draws as forced endings. The current board
        // is not terminal, so such an entry must be replaced by a real search.
        if (!cached.empty() && std::none_of(cached.begin(), cached.end(), [](const QVariant &line) {
                return line.toMap()["terminal"].toBool();
            })) {
            finish(cached, true);
            return;
        }
        const int depth = config_["depth"].toInt();
        if (depth > 0)
            engine_->analyseToDepth(fen, command, depth, config_["milliseconds"].toInt(), game_.chess960);
        else
            engine_->analyse(fen, command, true, config_["milliseconds"].toInt(), 24, game_.chess960);
    }
    void finish(const QVariantList &lines, bool cached) {
        require(!lines.empty() && lines.first().toMap()["rank"].toInt() == 1,
                "Aucune évaluation principale exacte reçue. Augmentez le temps par position.");
        for (const auto &v : lines) {
            const auto line = v.toMap();
            require(!line["bound"].toBool() && (line["scoreType"] == "cp" || line["scoreType"] == "mate"),
                    "Le moteur n’a fourni qu’une borne ou un score invalide.");
        }
        const auto node = nodes_.at(index_);
        db_->saveAnalysisResult(active_, index_, node, str(game_.nodes[node].fen), lines, key_, cached);
        ++index_;
        // Cache hits can finish thousands of nodes quickly; bound GUI notifications to 10 Hz.
        if (!notify_->isActive())
            notify_->start(100);
        const auto id = active_;
        const auto epoch = epoch_;
        QTimer::singleShot(0, this, [this, id, epoch] {
            if (id != active_ || epoch != epoch_ || closing_)
                return;
            try {
                next();
            } catch (const std::exception &e) {
                fail(QString::fromUtf8(e.what()));
            }
        });
    }
    void fail(const QString &text) {
        error_ = text;
        const auto id = active_;
        active_ = 0;
        if (engine_)
            engine_->shutdown();
        if (id && db_) {
            try {
                db_->setAnalysisStatus(id, "failed", text);
            } catch (const std::exception &e) {
                error_ += " · " + QString::fromUtf8(e.what());
            }
        }
        emit message(error_, true);
        publish();
        schedule();
    }
    QString annotated(qint64 id, bool includeVariations) {
        const auto job = db_->analysisJob(id);
        auto game = parsePgn(job["pgn"].toString().toStdString()).front();
        QSet<int> originalLeaves;
        for (int i = 0; i < int(game.nodes.size()); ++i)
            if (game.nodes[i].children.empty())
                originalLeaves.insert(i);
        const auto identity = job["identity"].toMap();
        game.tags["Trait64Annotator"] = ("Trait64 / " + identity["name"].toString()).toStdString();
        game.tags["Trait64EngineSHA256"] = identity["sha256"].toString().toStdString();
        game.tags["Trait64Analysis"] = json(job["config"]).toStdString();
        game.tags["Trait64AnalysisProgress"] =
            (QString::number(job["done"].toInt()) + "/" + QString::number(job["total"].toInt()))
                .toStdString();
        for (int offset = 0;; offset += 200) {
            const auto rows = db_->analysisResults(id, offset);
            if (rows.empty())
                break;
            for (const auto &v : rows) {
                const auto row = v.toMap();
                const int node = row["node"].toInt();
                require(node >= 0 && node < int(game.nodes.size()) && str(game.nodes[node].fen) == row["fen"],
                        "Résultat incompatible avec la copie de la partie.");
                const auto lines = row["lines"].toList();
                const auto score = lines.first().toMap();
                auto comment = str(game.nodes[node].comment);
                comment.remove(QRegularExpression("\\[%eval\\s+[^\\]]+\\]"));
                const auto eval = score["eval"].toString().replace("+M", "#").replace("−M", "#-");
                game.nodes[node].comment =
                    (comment.trimmed() + " [%eval " + eval + "]").trimmed().toStdString();
                if (!includeVariations)
                    continue;
                const auto originalChildren = game.nodes[node].children;
                for (const auto &pv : lines) {
                    const auto variation = pv.toMap();
                    game.nodes[node].comment += (" | PV " + QString::number(variation["rank"].toInt()) +
                                                 ": " + variation["san"].toString())
                                                    .toStdString();
                    int cursor = node;
                    for (const auto &move : variation["uci"].toString().split(' ', Qt::SkipEmptyParts)) {
                        if (originalLeaves.contains(cursor) || game.nodes.size() >= 100000)
                            break;
                        cursor = game.play(cursor, move.toStdString());
                    }
                }
                // Keep the original main line and branch order even when PVs overlap the study.
                for (size_t i = 0; i < originalChildren.size(); ++i)
                    require(game.nodes[node].children[i] == originalChildren[i],
                            "Ordre des variantes modifié.");
            }
        }
        return str(game.pgn());
    }
};

AnalysisQueue::AnalysisQueue(QObject *parent) : QObject(parent) {}
AnalysisQueue::~AnalysisQueue() {
    shutdown();
}
void AnalysisQueue::setLibrary(const QString &path) {
    shutdown();
    jobs_.clear();
    results_.clear();
    state_ = {{"ready", false}, {"running", false}, {"selected", 0}, {"page", 1}, {"hasNext", false}};
    emit changed();
    worker_ = new AnalysisWorker(path);
    const auto generation = ++generation_;
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::started, worker_, &AnalysisWorker::open);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &AnalysisWorker::view, this,
            [this, generation](const QVariantList &jobs, const QVariantList &rows, const QVariantMap &state) {
                if (generation != generation_)
                    return;
                jobs_ = jobs;
                results_ = rows;
                state_ = state;
                emit changed();
            });
    connect(worker_, &AnalysisWorker::message, this, [this, generation](const QString &text, bool error) {
        if (generation == generation_)
            emit message(text, error);
    });
    connect(worker_, &AnalysisWorker::studyReady, this, [this, generation](const QString &pgn) {
        if (generation == generation_)
            emit studyReady(pgn);
    });
    thread_.start();
}
void AnalysisQueue::shutdown() {
    if (!worker_)
        return;
    ++generation_;
    disconnect(worker_, nullptr, this, nullptr);
    QMetaObject::invokeMethod(worker_, [worker = worker_] { worker->close(); }, Qt::BlockingQueuedConnection);
    thread_.quit();
    thread_.wait();
    worker_ = nullptr;
}
void AnalysisQueue::enqueue(const QString &pgn, const QVariantMap &profile, bool variations,
                            qint64 gameId) {
    if (worker_)
        QMetaObject::invokeMethod(worker_, [w = worker_, pgn, profile, variations, gameId] {
            w->enqueue(pgn, profile, variations, gameId);
        });
}
void AnalysisQueue::select(qint64 id, int offset) {
    if (worker_)
        QMetaObject::invokeMethod(worker_, [w = worker_, id, offset] { w->select(id, offset); });
}
void AnalysisQueue::pause(qint64 id) {
    if (worker_)
        QMetaObject::invokeMethod(worker_, [w = worker_, id] { w->pause(id); });
}
void AnalysisQueue::resume(qint64 id) {
    if (worker_)
        QMetaObject::invokeMethod(worker_, [w = worker_, id] { w->resume(id); });
}
void AnalysisQueue::pauseAll() {
    if (worker_)
        QMetaObject::invokeMethod(worker_, [w = worker_] { w->pauseAll(); });
}
void AnalysisQueue::forget(qint64 id) {
    if (worker_)
        QMetaObject::invokeMethod(worker_, [w = worker_, id] { w->forget(id); });
}
void AnalysisQueue::clearCache() {
    if (worker_)
        QMetaObject::invokeMethod(worker_, [w = worker_] { w->clearCache(); });
}
void AnalysisQueue::exportPgn(qint64 id, const QString &url, bool variants) {
    if (worker_)
        QMetaObject::invokeMethod(worker_,
                                  [w = worker_, id, url, variants] { w->exportPgn(id, url, variants); });
}
void AnalysisQueue::openStudy(qint64 id) {
    if (worker_)
        QMetaObject::invokeMethod(worker_, [w = worker_, id] { w->openStudy(id); });
}
} // namespace t64
#include "queue.moc"
