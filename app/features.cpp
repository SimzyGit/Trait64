// SPDX-License-Identifier: AGPL-3.0-or-later
#include "accuracy.h"
#include "controller.h"
#include "explanation.h"
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextDocument>
#include <QUrl>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>

namespace {
QString qs(const std::string &v) {
    return QString::fromStdString(v);
}
void need(bool ok, const QString &text) {
    if (!ok)
        throw std::runtime_error(text.toStdString());
}
QString clockText(qint64 ms) {
    ms = std::max<qint64>(0, ms);
    return QString("%1:%2").arg(ms / 60000).arg((ms / 1000) % 60, 2, 10, QChar('0'));
}
QString position(const t64::Game &g, int node) {
    QStringList moves;
    for (int n = node; n > 0; n = g.nodes[n].parent)
        moves.prepend(qs(g.nodes[n].uci));
    return "position fen " + qs(g.nodes[0].fen) + (moves.empty() ? "" : " moves " + moves.join(' '));
}
} // namespace
Controller::Snapshot Controller::snapshot() const {
    size_t bytes = sizeof(game_);
    for (const auto &n : game_.nodes)
        bytes += sizeof(n) + n.fen.size() + n.san.size() + n.uci.size() + n.comment.size() +
                 4 * (n.children.size() + n.nags.size());
    return {game_, node_, gameId_, dirty_, bytes};
}
void Controller::restore(const Snapshot &v) {
    game_ = v.game;
    node_ = v.node;
    gameId_ = v.id;
    dirty_ = v.dirty;
    selected_ = -1;
    commentNode_ = -1;
    clockResult_.clear();
    forgetEngineLines();
    emit engineChanged();
    refresh();
    autosave_.start();
}
void Controller::remember() {
    commentNode_ = -1;
    cancelReview();
    if (trainingActive())
        return;
    auto v = snapshot();
    // Bound history memory per study, including large imports.
    if (v.bytes > 16 * 1024 * 1024) {
        undo_.clear();
        redo_.clear();
        return;
    }
    undo_.push_back(std::move(v));
    redo_.clear();
    size_t bytes = 0;
    for (const auto &u : undo_)
        bytes += u.bytes;
    while (undo_.size() > 64 || bytes > 16 * 1024 * 1024) {
        bytes -= undo_.front().bytes;
        undo_.pop_front();
    }
}
void Controller::undo() {
    if (trainingActive() || undo_.empty() || clockRunning_)
        return;
    stopPlaying();
    cancelReview();
    review_.clear();
    reviewQuality_.clear();
    emit reviewChanged();
    auto id = gameId_;
    redo_.push_back(snapshot());
    auto v = std::move(undo_.back());
    undo_.pop_back();
    v.id = id;
    v.dirty = true;
    restore(v);
}
void Controller::redo() {
    if (trainingActive() || redo_.empty() || clockRunning_)
        return;
    stopPlaying();
    cancelReview();
    review_.clear();
    reviewQuality_.clear();
    emit reviewChanged();
    auto id = gameId_;
    undo_.push_back(snapshot());
    auto v = std::move(redo_.back());
    redo_.pop_back();
    v.id = id;
    v.dirty = true;
    restore(v);
}
void Controller::storeTab(bool history) {
    if (studies_.empty())
        studies_.emplace_back();
    auto &v = studies_[activeTab_];
    v.position = snapshot();
    v.review = review_;
    if (history) {
        v.undo = undo_;
        v.redo = redo_;
    }
}
QVariantList Controller::tabs() const {
    QVariantList list;
    for (int i = 0; i < int(studies_.size()); ++i) {
        const auto &g = i == activeTab_ ? game_ : studies_[i].position.game;
        auto tag = [&](const char *s) {
            auto it = g.tags.find(s);
            return it == g.tags.end() ? QString() : qs(it->second);
        };
        list << QVariantMap{{"title", tag("White") + " — " + tag("Black")},
                            {"active", i == activeTab_},
                            {"dirty", i == activeTab_ ? dirty_ : studies_[i].position.dirty}};
    }
    return list;
}
void Controller::switchTab(int index) {
    if (index < 0 || index >= int(studies_.size()) || index == activeTab_)
        return;
    try {
        if (trainingActive())
            finishTraining();
        cancelReview();
        stopPlaying();
        storeTab();
        activeTab_ = index;
        undo_ = studies_[index].undo;
        redo_ = studies_[index].redo;
        review_ = studies_[index].review;
        restore(studies_[index].position);
        writeSession();
        emit reviewChanged();
        emit showAnalysis();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::closeTab(int index) {
    if (index < 0 || index >= int(studies_.size()))
        return;
    try {
        need(!busy_, "Attendez la fin du traitement avant de fermer une étude.");
        if (trainingActive())
            finishTraining();
        storeTab();
        auto &v = studies_[index].position;
        if (v.dirty) {
            v.id = db_->save(v.game, v.id);
            v.dirty = false;
        }
        if (studies_.size() == 1) {
            cancelReview();
            stopPlaying();
            game_ = t64::Game();
            node_ = 0;
            gameId_ = 0;
            dirty_ = false;
            undo_.clear();
            redo_.clear();
            review_.clear();
            reviewQuality_.clear();
            storeTab();
            refresh();
            writeSession();
            refreshLibrary();
            emit reviewChanged();
            return;
        }
        cancelReview();
        stopPlaying();
        studies_.erase(studies_.begin() + index);
        if (index < activeTab_)
            --activeTab_;
        else if (index == activeTab_)
            activeTab_ = std::min(index, int(studies_.size()) - 1);
        undo_ = studies_[activeTab_].undo;
        redo_ = studies_[activeTab_].redo;
        review_ = studies_[activeTab_].review;
        restore(studies_[activeTab_].position);
        writeSession();
        refreshLibrary();
        emit reviewChanged();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::duplicateStudy() {
    try {
        auto copy = game_;
        copy.tags["Event"] += " — copie";
        replaceGame(std::move(copy));
        dirty_ = true;
        refresh();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::initFeatures() {
    taskPoll_.setInterval(250);
    connect(&taskPoll_, &QTimer::timeout, this, &Controller::busyChanged);
    studies_.push_back({snapshot(), {}, {}, {}});
    QFile f(sessionPath_);
    if (f.open(QIODevice::ReadOnly) && f.size() <= 64 * 1024 * 1024) {
        try {
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(f.readAll(), &error);
            // The constructor already preserved malformed JSON; do not create a second recovery copy.
            const auto json = error.error == QJsonParseError::NoError && document.isObject()
                                  ? document.object()
                                  : QJsonObject();
            auto tabs = json["tabs"].toArray();
            if (json.contains("tabs")) {
                need(json["tabs"].isArray() && !tabs.empty() && tabs.size() <= 16,
                     "Liste des onglets de session invalide.");
                std::vector<Study> recovered;
                for (auto v : tabs) {
                    auto tab = v.toObject();
                    auto games = t64::parsePgn(tab["pgn"].toString().toStdString());
                    need(games.size() == 1, "Onglet de session invalide.");
                    int node = 0;
                    need(tab["path"].isArray(), "Chemin de session invalide.");
                    for (auto m : tab["path"].toArray()) {
                        const auto &children = games[0].nodes[node].children;
                        const auto found = std::find_if(children.begin(), children.end(), [&](int child) {
                            return m.isString() && games[0].nodes[child].uci == m.toString().toStdString();
                        });
                        need(found != children.end(), "Position absente de l’étude enregistrée.");
                        node = *found;
                    }
                    qint64 id =
                        json["library"].toString() == db_->path() ? tab["id"].toVariant().toLongLong() : 0;
                    recovered.push_back(
                        {{std::move(games[0]), node, id, tab["dirty"].toBool(), 0}, {}, {}, {}});
                }
                studies_ = std::move(recovered);
                activeTab_ = std::clamp(json["activeTab"].toInt(), 0, int(studies_.size()) - 1);
                const auto &v = studies_[activeTab_].position;
                game_ = v.game;
                node_ = v.node;
                gameId_ = v.id;
                dirty_ = v.dirty;
            }
        } catch (const std::exception &) {
            // Preserve the full original before falling back to the legacy active study.
            f.close();
            const auto preserved =
                sessionPath_ + ".unreadable-tabs-" + QString::number(QDateTime::currentMSecsSinceEpoch());
            need(QFile::copy(sessionPath_, preserved),
                 "Impossible de préserver les onglets de session illisibles.");
            QTimer::singleShot(0, this, [this, preserved] {
                emit message(tr("Certains onglets n’ont pas été restaurés. Session originale conservée : ") +
                                 preserved,
                             true);
            });
        }
    }
    connect(&reviewer_, &t64::Uci::completed, this, &Controller::finishReviewPosition);
    connect(&reviewer_, &t64::Uci::failed, this, [this](const QString &error) {
        if (reviewing_) {
            reviewing_ = false;
            reviewError_ = error;
            emit reviewChanged();
            emit message(error, true);
        }
    });
    connect(&engine_, &t64::Uci::failed, this, [this](const QString &error) {
        playing_ = false;
        analysis_ = false;
        clockRunning_ = false;
        localGame_ = false;
        clock_.stop();
        refresh();
        emit message(error, true);
    });
    clock_.setInterval(100);
    connect(&clock_, &QTimer::timeout, this, &Controller::tickClock);
}
void Controller::setPosition(const QString &fen, bool frc) {
    try {
        replaceGame(t64::Game(fen.trimmed().toStdString(), frc));
        dirty_ = true;
        writeSession();
        refresh();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::newChess960(int number) {
    setPosition(qs(t64::chess960Fen(std::clamp(number, 0, 959))), true);
}
QVariantList Controller::annotations() const {
    QVariantList out;
    QString comment = qs(game_.nodes[node_].comment);
    auto matches = QRegularExpression("\\[%c(al|sl)\\s+([^\\]]+)\\]").globalMatch(comment);
    while (matches.hasNext()) {
        auto match = matches.next();
        bool arrow = match.captured(1) == "al";
        for (const auto &token : match.captured(2).split(',')) {
            QString x = token.trimmed();
            if (!QRegularExpression(arrow ? "^[GRBY][a-h][1-8][a-h][1-8]$" : "^[GRBY][a-h][1-8]$")
                     .match(x)
                     .hasMatch())
                continue;
            out << QVariantMap{
                {"from", x.mid(1, 2)}, {"to", arrow ? x.mid(3, 2) : x.mid(1, 2)}, {"color", x.left(1)}};
        }
    }
    return out;
}
void Controller::toggleAnnotation(const QString &from, const QString &to, const QString &color) {
    if (trainingActive() || !QRegularExpression("^[a-h][1-8]$").match(from).hasMatch() ||
        !QRegularExpression("^[a-h][1-8]$").match(to).hasMatch() ||
        !QStringList{"G", "R", "B", "Y"}.contains(color))
        return;
    auto list = annotations();
    bool removed = false;
    for (int i = 0; i < list.size(); ++i) {
        auto a = list[i].toMap();
        if (a["from"] == from && a["to"] == to) {
            list.removeAt(i);
            if (a["color"] == color)
                removed = true;
            break;
        }
    }
    if (!removed)
        list << QVariantMap{{"from", from}, {"to", to}, {"color", color}};
    remember();
    QString comment = qs(game_.nodes[node_].comment);
    comment.remove(QRegularExpression("\\[%c(?:al|sl)\\s+[^\\]]+\\]"));
    comment = comment.trimmed();
    QStringList arrows, circles;
    for (const auto &v : list) {
        auto a = v.toMap();
        if (a["from"] == a["to"])
            circles << a["color"].toString() + a["from"].toString();
        else
            arrows << a["color"].toString() + a["from"].toString() + a["to"].toString();
    }
    if (!arrows.empty())
        comment += " [%cal " + arrows.join(',') + "]";
    if (!circles.empty())
        comment += " [%csl " + circles.join(',') + "]";
    game_.nodes[node_].comment = comment.trimmed().toStdString();
    refresh(true);
}
void Controller::clearAnnotations() {
    if (trainingActive())
        return;
    remember();
    QString text = qs(game_.nodes[node_].comment);
    text.remove(QRegularExpression("\\[%c(?:al|sl)\\s+[^\\]]+\\]"));
    game_.nodes[node_].comment = text.trimmed().toStdString();
    refresh(true);
}
void Controller::mergeGame(qint64 id) {
    try {
        need(!trainingActive(), "Terminez l’entraînement.");
        auto copy = game_;
        copy.merge(db_->load(id));
        remember();
        game_ = std::move(copy);
        review_.clear();
        reviewQuality_.clear();
        refresh(true);
        emit reviewChanged();
        emit message(tr("Variantes et commentaires fusionnés dans l’étude courante."));
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::searchAdvanced(const QVariantMap &filters) {
    advancedFilters_ = openingFilters(filters);
    requestLibrarySearch();
}
void Controller::inspectPlayer(const QString &name) {
    try {
        playerProfile_ = db_->player(name);
        emit libraryChanged();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::rebuildIndexes() {
    if (busy_)
        return;
    const auto path = db_->path();
    busy_ = true;
    cancelTask_->store(false);
    taskProgress_->store(0);
    taskPoll_.start();
    emit busyChanged();
    const auto canceled = cancelTask_;
    const auto progress = taskProgress_;
    importer_.setFuture(QtConcurrent::run([path, canceled, progress] {
        ImportResult r;
        r.operation = "reindex";
        try {
            t64::Database db(path);
            r.count = db.rebuildIndexes([&](int n) {
                progress->store(n);
                return !canceled->load();
            });
        } catch (const std::exception &e) {
            r.error = QString::fromUtf8(e.what());
        }
        return r;
    }));
}
void Controller::findDuplicates() {
    try {
        auto duplicates = db_->duplicates();
        QStringList text;
        for (const auto &d : duplicates) {
            auto v = d.toMap();
            text << v["white"].toString() + " — " + v["black"].toString() + " : identifiants " +
                        v["ids"].toString();
        }
        emit message(text.empty() ? tr("Aucun doublon de ligne principale.") : text.join('\n'));
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::nextDueCard() {
    for (const auto &v : cards_) {
        auto card = v.toMap();
        if (card["due"].toString() <= QDate::currentDate().toString(Qt::ISODate)) {
            startCard(card["id"].toLongLong());
            return;
        }
    }
    emit message(tr("Toutes les cartes dues ont été révisées."));
}
void Controller::saveEngineProfile(const QString &name) {
    if (name.trimmed().isEmpty())
        return;
    auto list = settings_.value("engineProfiles").toList();
    QVariantMap p{{"name", name.trimmed().left(80)}};
    for (const auto &k : QStringList{"enginePath", "threads", "hash", "multiPV", "skill", "playElo",
                                     "moveTime", "analysisDepth", "syzygyPath", "uciOptions"})
        p[k] = preferences()[k];
    for (int i = 0; i < list.size(); ++i)
        if (list[i].toMap()["name"] == p["name"]) {
            list.removeAt(i);
            break;
        }
    if (list.size() >= 100) {
        emit message(tr("100 profils maximum."), true);
        return;
    }
    list << p;
    settings_.setValue("engineProfiles", list);
    emit preferencesChanged();
}
void Controller::compareEngines(const QVariantList &indices) {
    try {
        need(!trainingActive() && !busy_, "Terminez l’entraînement ou le traitement en cours.");
        const auto available = settings_.value("engineProfiles").toList();
        QVariantList profiles;
        QSet<int> seen;
        for (const auto &value : indices) {
            const int index = value.toInt();
            need(index >= 0 && index < available.size(), "Profil moteur introuvable.");
            if (!seen.contains(index)) {
                profiles << available[index];
                seen.insert(index);
            }
        }
        cancelReview();
        stopPlaying();
        comparison_.start(profiles, qs(game_.nodes[node_].fen), positionCommand(), game_.chess960);
    } catch (const std::exception &e) {
        fail(e);
    }
}
QStringList Controller::bookOpenings(int count, int plies) {
    // Weighted random walks through the book give varied, still theoretical, openings.
    QStringList lines;
    auto *random = QRandomGenerator::global();
    for (int attempt = 0; attempt < count * 4 && lines.size() < count; ++attempt) {
        chess::Board board;
        QStringList line;
        for (int ply = 0; ply < plies; ++ply) {
            const auto moves = book_.moves(board);
            int total = 0;
            for (const auto &entry : moves)
                total += entry.weight;
            if (total == 0)
                break;
            int pick = random->bounded(total);
            for (const auto &entry : moves)
                if ((pick -= entry.weight) < 0) {
                    line << qs(chess::uci::moveToUci(entry.move));
                    board.makeMove(entry.move);
                    break;
                }
        }
        if (line.size() >= 2 && !lines.contains(line.join(' ')))
            lines << line.join(' ');
    }
    return lines;
}
void Controller::startTournament(const QVariantList &indices, int gamesPerPairing, int moveTime, int maxPlies,
                                 bool bookOpenings) {
    try {
        need(!trainingActive() && !busy(), "Terminez l’entraînement ou le traitement en cours.");
        need(!tournament_.running(), "Un tournoi est déjà en cours.");
        const auto available = settings_.value("engineProfiles").toList();
        QVariantList profiles;
        QSet<int> seen;
        for (const auto &value : indices) {
            const int index = value.toInt();
            need(index >= 0 && index < available.size(), "Profil moteur introuvable.");
            if (!seen.contains(index)) {
                profiles << available[index];
                seen.insert(index);
            }
        }
        t64::Tournament::Settings options{gamesPerPairing, moveTime, maxPlies, {}};
        if (bookOpenings) {
            need(book_.isOpen(), "Indiquez d’abord un livre Polyglot dans Paramètres.");
            const int pairs = int(profiles.size() * (profiles.size() - 1) / 2);
            options.openings = this->bookOpenings(std::max(1, pairs * std::max(1, gamesPerPairing / 2)), 8);
            need(!options.openings.isEmpty(), "Le livre ne contient pas la position initiale.");
        }
        comparison_.stop();
        stopPlaying();
        const auto stamp = QDateTime::currentDateTime();
        QDir().mkpath(dataPath_ + "/tournaments");
        tournamentFile_ = dataPath_ + "/tournaments/tournoi-" + stamp.toString("yyyyMMdd-HHmmss") + ".pgn";
        tournamentTitle_ = "Tournoi de moteurs — " + stamp.toString("dd/MM/yyyy HH:mm");
        QFile::remove(tournamentFile_);
        tournament_.start(profiles, options);
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::addComparisonLine(int engine, int variation) {
    try {
        need(!trainingActive(), "Terminez l’entraînement.");
        const auto rows = comparison_.rows();
        need(engine >= 0 && engine < rows.size(), "Moteur indisponible.");
        const auto lines = rows[engine].toMap()["lines"].toList();
        need(variation >= 0 && variation < lines.size(), "Variante indisponible.");
        const auto line = lines[variation].toMap();
        need(line["fen"].toString() == qs(game_.nodes[node_].fen), "L’analyse concerne une autre position.");
        auto candidate = game_;
        int n = node_;
        for (const auto &move : line["uci"].toString().split(' ', Qt::SkipEmptyParts))
            n = candidate.play(n, move.toStdString());
        remember();
        game_ = std::move(candidate);
        refresh(true);
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::loadEngineProfile(int index) {
    auto list = settings_.value("engineProfiles").toList();
    if (index < 0 || index >= list.size())
        return;
    auto p = list[index].toMap();
    for (auto it = p.begin(); it != p.end(); ++it)
        if (it.key() != "name" && it.key() != "uciOptions")
            setPreference(it.key(), it.value());
    settings_.setValue("uciOptions", p.value("uciOptions", QVariantMap()));
    emit preferencesChanged();
    startEngine();
}
void Controller::removeEngineProfile(int index) {
    auto list = settings_.value("engineProfiles").toList();
    if (index < 0 || index >= list.size())
        return;
    list.removeAt(index);
    settings_.setValue("engineProfiles", list);
    emit preferencesChanged();
}
QVariantMap Controller::reviewState() const {
    const int n = int(reviewNodes_.size());
    return {{"running", reviewing_},
            {"done", reviewDone_},
            {"total", reviewTwoPasses_ && n > 1 ? 2 * n : n},
            {"pass", reviewPass_ + 1},
            {"passes", reviewTwoPasses_ ? 2 : 1},
            {"error", reviewError_},
            {"milliseconds", reviewMilliseconds_}};
}
void Controller::analyseGame(int milliseconds, bool twoPasses) {
    if (trainingActive())
        return;
    cancelReview();
    stopPlaying();
    review_.clear();
    reviewQuality_.clear();
    reviewError_.clear();
    ++reviewEpoch_;
    reviewGame_ = game_;
    reviewNodes_ = game_.mainline();
    reviewScores_ = QVariantList(int(reviewNodes_.size()));
    reviewIndex_ = 0;
    reviewPass_ = 0;
    reviewDone_ = 0;
    reviewTwoPasses_ = twoPasses;
    reviewMilliseconds_ = std::clamp(milliseconds, 100, 10000);
    reviewing_ = true;
    auto options = settings_.value("uciOptions").toMap();
    options["UCI_LimitStrength"] = false;
    reviewer_.start(settings_.value("enginePath").toString(), settings_.value("threads", 1).toInt(),
                    settings_.value("hash", 128).toInt(), 1, 20, settings_.value("syzygyPath").toString(),
                    options);
    if (reviewing_)
        requestReviewPosition();
    emit reviewChanged();
}
void Controller::cancelReview() {
    if (!reviewing_)
        return;
    reviewing_ = false;
    ++reviewEpoch_;
    reviewer_.stop();
    reviewError_ = "Analyse interrompue ; résultats partiels conservés.";
    emit reviewChanged();
}
void Controller::requestReviewPosition() {
    if (!reviewing_)
        return;
    const int n = int(reviewNodes_.size());
    if (reviewPass_ == 0 && reviewIndex_ >= n && reviewTwoPasses_ && n > 1) {
        // Second pass, from the end: positions near the end are now in the engine's hash,
        // so every earlier position is searched with what was learnt about its future.
        reviewPass_ = 1;
        reviewIndex_ = n - 1;
        emit reviewChanged();
    }
    if ((reviewPass_ == 0 && reviewIndex_ >= n) || (reviewPass_ == 1 && reviewIndex_ < 0)) {
        reviewing_ = false;
        reviewer_.shutdown();
        emit reviewChanged();
        emit message(reviewTwoPasses_ ? tr("Bilan terminé (début → fin, puis fin → début).")
                                      : tr("Analyse de la ligne principale terminée."));
        return;
    }
    const int node = reviewNodes_[reviewIndex_];
    auto board = reviewGame_.boardAt(node);
    auto end = t64::gameResult(board);
    if (end.first == chess::GameResultReason::CHECKMATE || end.first == chess::GameResultReason::STALEMATE ||
        end.first == chess::GameResultReason::INSUFFICIENT_MATERIAL) {
        double score = end.first == chess::GameResultReason::CHECKMATE
                           ? (board.sideToMove() == chess::Color::WHITE ? -100. : 100.)
                           : 0.;
        const auto fen = qs(reviewGame_.nodes[node].fen);
        const int epoch = reviewEpoch_;
        QTimer::singleShot(0, this, [this, fen, epoch, score] {
            if (epoch != reviewEpoch_)
                return;
            finishReviewPosition(fen, {QVariantMap{{"score", score},
                                                   {"scoreType", score == 0 ? "cp" : "mate"},
                                                   {"cp", score == 0 ? QVariant(0) : QVariant()},
                                                   {"mate", score == 0 ? QVariant() : QVariant(0)},
                                                   {"eval", score == 0  ? "0.00"
                                                            : score > 0 ? "+M0"
                                                                        : "−M0"},
                                                   {"depth", 0},
                                                   {"uci", ""},
                                                   {"bound", false}}});
        });
        return;
    }
    // The first of two passes is a quick sketch (a third of the budget); the second is the full search.
    const int budget =
        reviewTwoPasses_ && reviewPass_ == 0 ? std::max(100, reviewMilliseconds_ / 3) : reviewMilliseconds_;
    reviewer_.analyse(qs(reviewGame_.nodes[node].fen), position(reviewGame_, node), true, budget, 24,
                      reviewGame_.chess960);
}
void Controller::rebuildReview() {
    // One row per move whose position before and after are both evaluated.
    review_.clear();
    reviewQuality_.clear();
    // Évaluations de la ligne principale, remises dans la forme que le module de calcul
    // attend. Les séquences de mat n'y entrent pas : une évaluation en nombre de coups
    // avant mat ne se convertit pas en chances de gain.
    std::vector<t64::NodeScore> scores(reviewScores_.size());
    for (int i = 0; i < reviewScores_.size(); ++i) {
        const auto entry = reviewScores_[i].toMap();
        if (entry.isEmpty() || entry["scoreType"] != "cp")
            continue;
        scores[i] = {true, entry["cp"].toDouble()};
    }
    for (int i = 1; i < int(reviewNodes_.size()); ++i) {
        const auto previous = reviewScores_[i - 1].toMap(), score = reviewScores_[i].toMap();
        if (previous.isEmpty() || score.isEmpty())
            continue;
        const int node = reviewNodes_[i], parent = reviewNodes_[i - 1];
        const bool white = reviewGame_.boardAt(parent).sideToMove() == chess::Color::WHITE;
        const bool lossKnown = previous["scoreType"] == "cp" && score["scoreType"] == "cp";
        const double loss =
            lossKnown ? std::max(0., (previous["cp"].toDouble() - score["cp"].toDouble()) * (white ? 1 : -1))
                      : 0.;
        QString label = !lossKnown    ? "Séquence de mat"
                        : loss >= 250 ? "Gaffe"
                        : loss >= 100 ? "Erreur"
                        : loss >= 50  ? "Imprécision"
                                      : "Correct";
        // Le point coloré de l'analyse, à la manière de ChessBase. Les seuils vivent
        // dans accuracy.cpp, avec la précision : une seule table pour les deux.
        const bool playedBest = !previous["uci"].toString().isEmpty() &&
                                previous["uci"].toString() == qs(reviewGame_.nodes[node].uci);
        const char *quality = t64::moveQuality(loss, lossKnown, playedBest);
        QVariant mark;
        if (lossKnown) {
            // Les évaluations sont comptées du point de vue des Blancs : on les retourne
            // pour celui qui joue, car ce sont ses chances de gain qui le notent.
            const double sign = white ? 1 : -1;
            mark = qRound(10 * t64::moveAccuracy(t64::winPercent(previous["cp"].toDouble() * sign),
                                                 t64::winPercent(score["cp"].toDouble() * sign))) /
                   10.;
        }
        review_ << QVariantMap{{"node", node},
                               {"parent", parent},
                               {"move", qs(reviewGame_.nodes[node].san)},
                               {"white", white},
                               {"score", score["score"]},
                               {"eval", score["eval"]},
                               {"loss", lossKnown ? QVariant(qRound(loss)) : QVariant()},
                               {"lossKnown", lossKnown},
                               {"scoreType", score["scoreType"]},
                               {"label", label},
                               {"quality", QString::fromLatin1(quality)},
                               {"accuracy", mark},
                               {"best", previous["uci"]},
                               {"played", qs(reviewGame_.nodes[node].uci)},
                               {"reply", score["uci"]},
                               {"variant960", reviewGame_.chess960},
                               {"beforeEval", previous["eval"]},
                               {"beforeDepth", previous["depth"]},
                               {"engine", reviewer_.name()},
                               {"milliseconds", reviewMilliseconds_},
                               {"pass", score["pass"]},
                               {"fen", qs(reviewGame_.nodes[parent].fen)},
                               {"depth", score["depth"]}};
    }
    reviewQuality_.clear();
    for (const auto &row : review_) {
        const auto entry = row.toMap();
        reviewQuality_[QString::number(entry["node"].toInt())] = entry["quality"];
    }
    const auto summary = t64::summarize(reviewGame_, reviewNodes_, scores);
    const char *keys[] = {"opening", "middlegame", "endgame", "all"};
    reviewAccuracy_.clear();
    for (int side = 0; side < 2; ++side) {
        // -1 se lit « pas mesurable » : une phase que la partie n'a pas atteinte, ou dont
        // aucun coup n'a reçu d'évaluation exacte des deux côtés.
        auto reading = [](double value) {
            return value < 0 ? QVariant() : QVariant(qRound(value * 10) / 10.);
        };
        QVariantMap side_;
        for (int part = 0; part < 3; ++part) {
            side_[keys[part]] = reading(summary.phase[side][part]);
            side_[QString(keys[part]) + "Moves"] = summary.moves[side][part];
        }
        side_["all"] = reading(summary.all[side]);
        side_["allMoves"] = summary.moves[side][3];
        side_["blunders"] = summary.blunders[side];
        side_["mistakes"] = summary.mistakes[side];
        side_["inaccuracies"] = summary.inaccuracies[side];
        reviewAccuracy_[side == 0 ? "white" : "black"] = side_;
    }
}
void Controller::finishReviewPosition(const QString &fen, const QVariantList &lines) {
    if (!reviewing_ || reviewIndex_ < 0 || reviewIndex_ >= int(reviewNodes_.size()) ||
        fen != qs(reviewGame_.nodes[reviewNodes_[reviewIndex_]].fen))
        return;
    if (lines.empty() || lines.first().toMap()["bound"].toBool()) {
        // A second-pass miss keeps the first pass's figure instead of stopping the review.
        if (reviewPass_ == 1 && !reviewScores_[reviewIndex_].toMap().isEmpty()) {
            --reviewIndex_;
            ++reviewDone_;
        } else {
            reviewing_ = false;
            reviewError_ =
                "Le moteur n’a pas fourni une évaluation exacte ; relancez avec davantage de temps.";
            emit reviewChanged();
            return;
        }
    } else {
        auto score = lines.first().toMap();
        score["pass"] = reviewPass_ + 1;
        reviewScores_[reviewIndex_] = score;
        rebuildReview();
        reviewPass_ == 0 ? ++reviewIndex_ : --reviewIndex_;
        ++reviewDone_;
    }
    emit reviewChanged();
    const int epoch = reviewEpoch_;
    QTimer::singleShot(0, this, [this, epoch] {
        if (epoch == reviewEpoch_)
            requestReviewPosition();
    });
}
QVariantMap Controller::explainReview(int index, bool alternative) const {
    try {
        need(index >= 0 && index < review_.size(), "Sélectionnez un coup analysé.");
        const auto row = review_[index].toMap();
        need(!row["played"].toString().isEmpty(), "Relancez le bilan pour obtenir les suites.");
        QStringList line =
            (alternative ? row["best"].toString() : row["played"].toString() + " " + row["reply"].toString())
                .split(' ', Qt::SkipEmptyParts);
        need(!line.empty(), "Aucune suite moteur disponible pour cette position.");
        const bool truncated = line.size() > 128;
        line = line.mid(0, 128);
        std::vector<std::string> moves;
        for (const auto &move : line)
            moves.push_back(move.toStdString());
        const auto facts = t64::explainLine(
            t64::checkedBoard(row["fen"].toString().toStdString(), row["variant960"].toBool()), moves);
        const QStringList names{"pion", "cavalier", "fou", "tour", "dame", "roi", ""};
        QVariantList steps;
        for (const auto &move : facts.moves) {
            QStringList events;
            if (move.castle)
                events << "Roque : déplacement du roi et de la tour.";
            if (move.captured != chess::PieceType::NONE)
                events << QString("Capture%1 : %2 en %3.")
                              .arg(move.enPassant ? " en passant" : "", names[int(move.captured)],
                                   qs(move.captureSquare));
            if (move.promotion != chess::PieceType::NONE)
                events << "Promotion en " + names[int(move.promotion)] + ".";
            if (move.mate)
                events << "Échec et mat : aucune réponse légale.";
            else if (move.check)
                events << "Échec au roi adverse.";
            else if (move.stalemate)
                events << "Pat : aucun coup légal, sans échec.";
            for (const auto &tactic : move.tactics) {
                QStringList attackers, targets;
                for (const auto &piece : tactic.attackers)
                    attackers << names[int(piece.type)] + " " + qs(piece.square);
                for (const auto &piece : tactic.targets)
                    targets << names[int(piece.type)] + " " + qs(piece.square);
                QString label;
                switch (tactic.kind) {
                case t64::TacticalKind::Fork:
                    label = "Fourchette / attaque multiple";
                    break;
                case t64::TacticalKind::AbsolutePin:
                    label = "Clouage au roi (déplacement limité à cet alignement)";
                    break;
                case t64::TacticalKind::KingSkewer:
                    label = "Enfilade du roi";
                    break;
                case t64::TacticalKind::DiscoveredCheck:
                    label = "Échec à la découverte";
                    break;
                case t64::TacticalKind::DoubleCheck:
                    label = "Échec double";
                    break;
                }
                events << QString("%1 : %2 → %3.").arg(label, attackers.join(", "), targets.join(", "));
            }
            if (!move.tactics.empty())
                events << "Motifs observés ; aucun gain matériel forcé n’est démontré.";
            if (events.empty())
                events << "Le matériel ne change pas.";
            steps << QVariantMap{{"uci", qs(move.uci)},
                                 {"san", qs(move.san)},
                                 {"fen", qs(move.fen)},
                                 {"side", move.white ? "Blancs" : "Noirs"},
                                 {"facts", events.join(' ')},
                                 {"material", move.material},
                                 {"marks", qs(t64::tacticalAnnotations(move.tactics))},
                                 {"motifCount", int(move.tactics.size())}};
        }
        return {{"available", true},
                {"steps", steps},
                {"uci", line.join(' ')},
                {"fen", row["fen"]},
                {"variant960", row["variant960"]},
                {"move", row["move"]},
                {"label", row["label"]},
                {"beforeEval", row["beforeEval"]},
                {"afterEval", row["eval"]},
                {"lossKnown", row["lossKnown"]},
                {"loss", row["loss"]},
                {"engine", row["engine"]},
                {"milliseconds", row["milliseconds"]},
                {"depth", row[alternative ? "beforeDepth" : "depth"]},
                {"initialMaterial", facts.initialMaterial},
                {"finalMaterial", facts.moves.back().material},
                {"truncated", truncated}};
    } catch (const std::exception &e) {
        return {{"available", false}, {"error", QString::fromUtf8(e.what())}};
    }
}
bool Controller::openReviewLine(int index, bool alternative) {
    try {
        need(!trainingActive() && !reviewing_, "Terminez l’exercice ou le bilan avant d’ouvrir une copie.");
        const auto details = explainReview(index, alternative);
        need(details["available"].toBool(), details["error"].toString());
        t64::Game copy(details["fen"].toString().toStdString(), details["variant960"].toBool());
        copy.tags["Event"] = alternative ? "Proposition moteur avant le coup" : "Coup joué et réponse moteur";
        copy.tags["Annotator"] = details["engine"].toString().toStdString();
        copy.nodes[0].comment = QString("Suite calculée par %1, %2 ms par position, profondeur %3. "
                                        "Départ depuis un diagramme ; historique antérieur non reproduit.")
                                    .arg(details["engine"].toString())
                                    .arg(details["milliseconds"].toInt())
                                    .arg(details["depth"].toInt())
                                    .toStdString();
        int node = 0;
        for (const auto &value : details["steps"].toList()) {
            const auto step = value.toMap();
            node = copy.play(node, step["uci"].toString().toStdString());
            copy.nodes[node].comment =
                (step["facts"].toString() + " " + step["marks"].toString()).trimmed().toStdString();
        }
        replaceGame(std::move(copy));
        return true;
    } catch (const std::exception &e) {
        fail(e);
        return false;
    }
}
void Controller::annotateReview() {
    if (reviewing_ || review_.empty() || trainingActive())
        return;
    remember();
    for (const auto &v : review_) {
        auto row = v.toMap();
        int node = row["node"].toInt();
        if (node < 0 || node >= int(game_.nodes.size()))
            continue;
        QString comment = qs(game_.nodes[node].comment);
        comment.remove(QRegularExpression("\\[%eval\\s+[^\\]]+\\]"));
        QString eval = row["eval"].toString();
        eval.replace("−M", "#-");
        eval.replace("+M", "#");
        comment += " [%eval " + eval + "]";
        game_.nodes[node].comment = comment.trimmed().toStdString();
        int nag = row["loss"].toInt() >= 250   ? 4
                  : row["loss"].toInt() >= 100 ? 2
                  : row["loss"].toInt() >= 50  ? 6
                                               : 0;
        if (nag && game_.nodes[node].nags.empty())
            game_.nodes[node].nags.push_back(nag);
    }
    refresh(true);
    emit message(tr("Évaluations et erreurs ajoutées aux annotations PGN."));
}
void Controller::createMistakeCards() {
    try {
        need(!busy_, "Traitement en cours.");
        need(!game_.chess960, "Les cartes de tactique sont limitées aux échecs classiques.");
        int count = 0;
        for (const auto &v : review_) {
            auto row = v.toMap();
            if (row["loss"].toInt() < 100 || row["best"].toString().isEmpty())
                continue;
            QStringList line = row["best"].toString().split(' ', Qt::SkipEmptyParts);
            while (line.size() > 5)
                line.removeLast();
            db_->addCard("Après " + row["move"].toString() + " : trouver mieux", row["fen"].toString(),
                         line.join(' '));
            ++count;
        }
        refreshLibrary();
        emit message(tr("%1 carte(s) créée(s) depuis les erreurs analysées.").arg(count));
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::startLocalGame(int minutes, int increment, int opponent, bool humanWhite) {
    if (trainingActive())
        return;
    stopPlaying();
    remember();
    review_.clear();
    reviewQuality_.clear();
    emit reviewChanged();
    game_.tags["Result"] = "*";
    clockResult_.clear();
    minutes = std::clamp(minutes, 1, 180);
    increment = std::clamp(increment, 0, 120);
    whiteMs_ = blackMs_ = minutes * 60000;
    incrementMs_ = increment * 1000;
    localGame_ = opponent == 0;
    playing_ = !localGame_;
    humanWhite_ = humanWhite;
    flipped_ = !humanWhite;
    analysis_ = false;
    clockRunning_ = true;
    clockElapsed_.start();
    clock_.start();
    game_.tags["TimeControl"] = std::to_string(minutes * 60) + "+" + std::to_string(increment);
    if (playing_)
        startEngine();
    refresh(true);
}
void Controller::tickClock() {
    if (!clockRunning_)
        return;
    const auto elapsed = clockElapsed_.restart();
    const bool white = game_.boardAt(node_).sideToMove() == chess::Color::WHITE;
    auto &remaining = white ? whiteMs_ : blackMs_;
    remaining = std::max<qint64>(0, remaining - elapsed);
    if (!remaining) {
        clockRunning_ = false;
        playing_ = false;
        localGame_ = false;
        clock_.stop();
        engine_.stop();
        auto board = game_.boardAt(node_);
        bool opponentHasPiece = false;
        for (int i = 0; i < 64; ++i) {
            auto piece = board.at(chess::Square(i));
            if (piece != chess::Piece::NONE && piece.color() != board.sideToMove() &&
                piece.type() != chess::PieceType::KING)
                opponentHasPiece = true;
        }
        game_.tags["Result"] = opponentHasPiece ? (white ? "0-1" : "1-0") : "1/2-1/2";
        clockResult_ = (white ? "Blancs" : "Noirs") + QString(" : temps écoulé");
        refresh(true);
    }
    state_["whiteClock"] = clockText(whiteMs_);
    state_["blackClock"] = clockText(blackMs_);
    state_["clockRunning"] = clockRunning_;
    emit changed();
}
void Controller::afterClockMove(bool white) {
    if (!clockRunning_)
        return;
    auto &remaining = white ? whiteMs_ : blackMs_;
    remaining += incrementMs_;
    clockElapsed_.restart();
}
void Controller::resignGame() {
    if (!playing_ && !localGame_)
        return;
    remember();
    bool white = playing_ ? humanWhite_ : game_.boardAt(node_).sideToMove() == chess::Color::WHITE;
    stopPlaying();
    game_.tags["Result"] = white ? "0-1" : "1-0";
    clockResult_ = "Abandon";
    refresh(true);
}
void Controller::claimDraw() {
    if (!playing_ && !localGame_)
        return;
    auto b = game_.boardAt(node_);
    auto e = t64::gameResult(b);
    if (e.second == chess::GameResult::NONE) {
        emit message(tr("Aucune nulle réclamable dans cette position."));
        return;
    }
    remember();
    stopPlaying();
    game_.tags["Result"] = "1/2-1/2";
    clockResult_ = "Partie nulle";
    refresh(true);
}
void Controller::detectOpening() {
    // Dataset is generated from the CC0 Lichess opening catalogue, loaded lazily.
    static QMap<QString, QPair<QString, QString>> names;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        QFile f(":/assets/openings/positions.tsv");
        if (f.open(QIODevice::ReadOnly)) {
            while (!f.atEnd()) {
                auto fields = QString::fromUtf8(f.readLine()).trimmed().split('\t');
                if (fields.size() >= 3)
                    names.insert(fields[0], {fields[1], fields[2]});
            }
        }
    }
    state_["opening"] = "";
    state_["openingEco"] = "";
    state_["openingName"] = "";
    if (game_.chess960)
        return;
    for (int n = node_;; n = game_.nodes[n].parent) {
        auto it = names.constFind(qs(t64::positionKey(game_.nodes[n].fen)));
        if (it != names.cend()) {
            state_["opening"] = it->first + " · " + it->second;
            state_["openingEco"] = it->first;
            state_["openingName"] = it->second;
            break;
        }
        if (!n)
            break;
    }
}
void Controller::exportStudyPdf(const QString &url) {
    try {
        db_->validateOutputPath(localPath(url));
        QSaveFile file(localPath(url));
        need(file.open(QIODevice::WriteOnly), file.errorString());
        {
            QPdfWriter writer(&file);
            writer.setResolution(96);
            writer.setPageSize(QPageSize(QPageSize::A4));
            writer.setTitle(state_["title"].toString());
            QTextDocument doc;
            doc.setDefaultFont(QFont("DejaVu Sans", 10));
            QString html = "<h1>" + state_["title"].toString().toHtmlEscaped() + "</h1><p>" +
                           state_["opening"].toString().toHtmlEscaped() +
                           "</p><table cellspacing='0' cellpadding='0'>";
            auto board = game_.boardAt(node_);
            const QString glyphs = "♙♘♗♖♕♔♟♞♝♜♛♚";
            for (int row = 7; row >= 0; --row) {
                html += "<tr>";
                for (int col = 0; col < 8; ++col) {
                    auto p = board.at(chess::Square(row * 8 + col));
                    QString piece =
                        p == chess::Piece::NONE
                            ? " "
                            : QString(glyphs[int(p.type()) + (p.color() == chess::Color::WHITE ? 0 : 6)]);
                    html += QString("<td width='35' height='35' align='center' bgcolor='%1' "
                                    "style='font-size:24pt;color:#111'>%2</td>")
                                .arg((row + col) % 2 ? "#eee4d0" : "#9ca88e", piece);
                }
                html += "</tr>";
            }
            html += "</table><p>" + qs(game_.nodes[node_].fen).toHtmlEscaped() + "</p>";
            for (const auto &v : notation_) {
                auto n = v.toMap();
                html += "<p style='margin-left:" + QString::number(std::min(8, n["depth"].toInt()) * 12) +
                        "px'><b>" + n["text"].toString().toHtmlEscaped() + "</b> " +
                        n["comment"].toString().toHtmlEscaped().replace("\n", "<br>") + "</p>";
            }
            doc.setHtml(html);
            doc.print(&writer);
        }
        need(file.commit(), file.errorString());
        emit message(tr("Étude exportée en PDF."));
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::identifyOpening() {
    if (trainingActive())
        return;
    const auto eco = state_["openingEco"].toString(), name = state_["openingName"].toString();
    if (eco.isEmpty()) {
        emit message(tr("Aucune ouverture nommée trouvée sur ce chemin."));
        return;
    }
    remember();
    game_.tags["ECO"] = eco.toStdString();
    game_.tags["Opening"] = name.toStdString();
    refresh(true);
    emit message(tr("Ouverture ajoutée aux informations de la partie."));
}
QByteArray Controller::diagramSvg() const {
    // Same geometry, colours, pieces and annotations as the PNG export, as scalable vectors.
    const auto pref = preferences();
    const int margin = 32, cell = 96, size = 8 * cell + 2 * margin;
    auto esc = [](QString text) {
        return text.replace('&', "&amp;").replace('<', "&lt;").replace('"', "&quot;");
    };
    QString svg =
        QString(
            "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%1\" viewBox=\"0 0 %1 %1\">\n"
            "<rect width=\"%1\" height=\"%1\" fill=\"%2\"/>\n")
            .arg(size)
            .arg(esc(pref["background"].toString()));
    const auto board = game_.boardAt(node_);
    const auto style = pref["pieceStyle"].toString();
    int pieceIndex = 0;
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col) {
            const int sq = flipped_ ? row * 8 + 7 - col : (7 - row) * 8 + col;
            const int x = margin + col * cell, y = margin + row * cell;
            svg += QString("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%3\" fill=\"%4\"/>\n")
                       .arg(x)
                       .arg(y)
                       .arg(cell)
                       .arg(esc(pref[(row + col) % 2 ? "boardDark" : "boardLight"].toString()));
            const auto piece = board.at(chess::Square(sq));
            if (piece == chess::Piece::NONE)
                continue;
            const bool white = piece.color() == chess::Color::WHITE;
            if (style == "unicode") {
                svg += QString(
                           "<text x=\"%1\" y=\"%2\" font-family=\"DejaVu Sans\" font-size=\"66\" fill=\"%3\" "
                           "stroke=\"#26372e\" stroke-width=\"1.5\">%4</text>\n")
                           .arg(x + 10)
                           .arg(y + cell - 18)
                           .arg(white ? "#fffaf0" : "#23302e")
                           .arg(QString("♟♞♝♜♛♚")[int(piece.type())]);
                continue;
            }
            const auto code = (white ? QString("w") : QString("b")) + QString("PNBRQK")[int(piece.type())];
            QFile file(style == "custom" ? pref["customPiecePath"].toString() + "/" + code + ".svg"
                                         : ":/assets/pieces/" + style + "/" + code + ".svg");
            need(file.open(QIODevice::ReadOnly), "Pièce illisible pendant l’export.");
            QString piece_svg = QString::fromUtf8(file.readAll());
            piece_svg.remove(QRegularExpression("<\\?xml[^>]*\\?>|<!DOCTYPE[^>]*>"));
            const auto root = QRegularExpression("<svg\\b[^>]*>").match(piece_svg);
            const auto end = piece_svg.lastIndexOf("</svg>");
            need(root.hasMatch() && end > root.capturedEnd(0), "Pièce SVG invalide.");
            const auto tag = root.captured(0);
            // A group, not a nested <svg>: SVG Tiny renderers (Qt, many printers) ignore nesting.
            double box[4] = {0, 0, 45, 45};
            const auto view = QRegularExpression("viewBox=\"([^\"]+)\"").match(tag);
            if (view.hasMatch()) {
                const auto parts = view.captured(1).split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
                for (int k = 0; k < 4 && k < parts.size(); ++k)
                    box[k] = parts[k].toDouble();
            } else {
                for (const auto &[name, slot] : {std::pair{"width", 2}, std::pair{"height", 3}}) {
                    const auto m = QRegularExpression(QString("\\b%1=\"([0-9.]+)").arg(name)).match(tag);
                    if (m.hasMatch())
                        box[slot] = m.captured(1).toDouble();
                }
            }
            const double scale = (cell - 8) / std::max(1.0, std::max(box[2], box[3]));
            piece_svg = QString("<g transform=\"translate(%1 %2) scale(%3) translate(%4 %5)\">")
                            .arg(x + 4)
                            .arg(y + 4)
                            .arg(scale)
                            .arg(-box[0])
                            .arg(-box[1]) +
                        piece_svg.mid(root.capturedEnd(0), end - root.capturedEnd(0)) + "</g>";
            // Pieces may define gradients or clip paths: keep their ids unique in the diagram.
            const auto prefix = QString("p%1-").arg(pieceIndex++);
            piece_svg.replace(QRegularExpression("\\bid=\"([^\"]+)\""), "id=\"" + prefix + "\\1\"");
            piece_svg.replace(QRegularExpression("url\\(#([^)]+)\\)"), "url(#" + prefix + "\\1)");
            piece_svg.replace(QRegularExpression("href=\"#([^\"]+)\""), "href=\"#" + prefix + "\\1\"");
            svg += piece_svg.trimmed() + "\n";
        }
    auto center = [&](const QString &square) {
        const int n = (square[1].unicode() - '1') * 8 + square[0].unicode() - 'a';
        const int row = flipped_ ? n / 8 : 7 - n / 8, col = flipped_ ? 7 - n % 8 : n % 8;
        return QPointF(margin + (col + .5) * cell, margin + (row + .5) * cell);
    };
    const QMap<QString, QString> colours{
        {"G", "#599c46"}, {"R", "#d75050"}, {"B", "#4289cc"}, {"Y", "#dcac35"}};
    for (const auto &v : annotations()) {
        const auto a = v.toMap();
        const auto colour = colours.value(a["color"].toString(), "#599c46");
        const auto from = center(a["from"].toString()), to = center(a["to"].toString());
        if (from == to) {
            svg += QString(
                       "<circle cx=\"%1\" cy=\"%2\" r=\"38\" fill=\"none\" stroke=\"%3\" stroke-width=\"10\" "
                       "stroke-opacity=\".8\"/>\n")
                       .arg(from.x())
                       .arg(from.y())
                       .arg(colour);
            continue;
        }
        const double angle = std::atan2(to.y() - from.y(), to.x() - from.x());
        const QPointF base = to - QPointF(18 * std::cos(angle), 18 * std::sin(angle));
        const QPointF left = to - QPointF(30 * std::cos(angle - .5), 30 * std::sin(angle - .5));
        const QPointF right = to - QPointF(30 * std::cos(angle + .5), 30 * std::sin(angle + .5));
        svg +=
            QString(
                "<g fill=\"%1\" stroke=\"%1\" opacity=\".8\"><line x1=\"%2\" y1=\"%3\" x2=\"%4\" y2=\"%5\" "
                "stroke-width=\"10\" stroke-linecap=\"round\"/><polygon stroke=\"none\" points=\"%6,%7 %8,%9 "
                "%10,%11\"/></g>\n")
                .arg(colour)
                .arg(from.x())
                .arg(from.y())
                .arg(base.x())
                .arg(base.y())
                .arg(to.x())
                .arg(to.y())
                .arg(left.x())
                .arg(left.y())
                .arg(right.x())
                .arg(right.y());
    }
    if (pref["coordinates"].toBool())
        for (int i = 0; i < 8; ++i) {
            svg += QString("<text x=\"%1\" y=\"%2\" font-family=\"DejaVu Sans\" font-size=\"17\" fill=\"%3\" "
                           "text-anchor=\"middle\">%4</text>\n")
                       .arg(margin + i * cell + cell / 2)
                       .arg(size - margin / 2 + 6)
                       .arg(esc(pref["foreground"].toString()))
                       .arg(QChar('a' + (flipped_ ? 7 - i : i)));
            svg += QString("<text x=\"%1\" y=\"%2\" font-family=\"DejaVu Sans\" font-size=\"17\" fill=\"%3\" "
                           "text-anchor=\"middle\">%4</text>\n")
                       .arg(margin / 2)
                       .arg(margin + i * cell + cell / 2 + 6)
                       .arg(esc(pref["foreground"].toString()))
                       .arg(flipped_ ? i + 1 : 8 - i);
        }
    return (svg + "</svg>\n").toUtf8();
}
void Controller::exportDiagram(const QString &url) {
    try {
        db_->validateOutputPath(localPath(url));
        if (localPath(url).endsWith(".svg", Qt::CaseInsensitive)) {
            QSaveFile file(localPath(url));
            need(file.open(QIODevice::WriteOnly), file.errorString());
            const auto bytes = diagramSvg();
            need(file.write(bytes) == bytes.size() && file.commit(), file.errorString());
            emit message(tr("Diagramme SVG exporté avec votre apparence et vos flèches."));
            return;
        }
        const auto pref = preferences();
        const int margin = 32, cell = 96, size = 8 * cell + 2 * margin;
        QImage diagram(size, size, QImage::Format_ARGB32_Premultiplied);
        diagram.fill(QColor(pref["background"].toString()));
        QPainter painter(&diagram);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setFont(QFont("DejaVu Sans", 13));
        const auto board = game_.boardAt(node_);
        for (int row = 0; row < 8; ++row)
            for (int col = 0; col < 8; ++col) {
                const int sq = flipped_ ? row * 8 + 7 - col : (7 - row) * 8 + col;
                const QRect box(margin + col * cell, margin + row * cell, cell, cell);
                painter.fillRect(box, QColor(pref[(row + col) % 2 ? "boardDark" : "boardLight"].toString()));
                const auto piece = board.at(chess::Square(sq));
                if (piece == chess::Piece::NONE)
                    continue;
                const bool white = piece.color() == chess::Color::WHITE;
                QString code = (white ? QString("w") : QString("b")) + QString("PNBRQK")[int(piece.type())];
                const auto style = pref["pieceStyle"].toString();
                if (style == "unicode") {
                    QFont font("DejaVu Sans", 60);
                    QPainterPath outline;
                    outline.addText(box.left() + 10, box.bottom() - 18, font,
                                    QString("♟♞♝♜♛♚")[int(piece.type())]);
                    painter.setPen(QPen(QColor("#26372e"), 1.5));
                    painter.setBrush(white ? QColor("#fffaf0") : QColor("#23302e"));
                    painter.drawPath(outline);
                } else {
                    const auto path = style == "custom"
                                          ? pref["customPiecePath"].toString() + "/" + code + ".svg"
                                          : ":/assets/pieces/" + style + "/" + code + ".svg";
                    QImageReader reader(path);
                    reader.setScaledSize(QSize(88, 88));
                    auto image = reader.read();
                    need(!image.isNull(), "Pièce illisible pendant l’export.");
                    painter.drawImage(box.adjusted(4, 4, -4, -4), image);
                }
            }
        auto center = [&](QString square) {
            int n = (square[1].unicode() - '1') * 8 + square[0].unicode() - 'a';
            int row = flipped_ ? n / 8 : 7 - n / 8, col = flipped_ ? 7 - n % 8 : n % 8;
            return QPointF(margin + (col + .5) * cell, margin + (row + .5) * cell);
        };
        for (const auto &v : annotations()) {
            auto a = v.toMap();
            QColor color = QMap<QString, QColor>{{"G", QColor("#599c46")},
                                                 {"R", QColor("#d75050")},
                                                 {"B", QColor("#4289cc")},
                                                 {"Y", QColor("#dcac35")}}
                               .value(a["color"].toString());
            color.setAlpha(205);
            painter.setPen(QPen(color, 10, Qt::SolidLine, Qt::RoundCap));
            painter.setBrush(Qt::NoBrush);
            auto from = center(a["from"].toString()), to = center(a["to"].toString());
            if (from == to)
                painter.drawEllipse(from, 38, 38);
            else {
                QLineF line(from, to);
                const double angle = std::atan2(line.dy(), line.dx());
                QPointF base = to - QPointF(18 * std::cos(angle), 18 * std::sin(angle));
                painter.drawLine(from, base);
                painter.setPen(Qt::NoPen);
                painter.setBrush(color);
                QPolygonF head;
                head << to << to - QPointF(30 * std::cos(angle - .5), 30 * std::sin(angle - .5))
                     << to - QPointF(30 * std::cos(angle + .5), 30 * std::sin(angle + .5));
                painter.drawPolygon(head);
            }
        }
        if (pref["coordinates"].toBool()) {
            painter.setPen(QColor(pref["foreground"].toString()));
            for (int i = 0; i < 8; ++i) {
                painter.drawText(QRect(margin + i * cell, size - margin, cell, margin), Qt::AlignCenter,
                                 QString(QChar('a' + (flipped_ ? 7 - i : i))));
                painter.drawText(QRect(0, margin + i * cell, margin, cell), Qt::AlignCenter,
                                 QString::number(flipped_ ? i + 1 : 8 - i));
            }
        }
        painter.end();
        QSaveFile file(localPath(url));
        need(file.open(QIODevice::WriteOnly), file.errorString());
        need(diagram.save(&file, "PNG"), "Écriture PNG impossible.");
        need(file.commit(), file.errorString());
        emit message(tr("Diagramme PNG exporté avec votre apparence et vos flèches."));
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::setEngineOption(const QString &name, const QVariant &value) {
    try {
        const QStringList managed{"Threads", "Hash", "MultiPV", "Skill Level", "UCI_Chess960", "SyzygyPath"};
        need(!managed.contains(name), "Utilisez les réglages principaux pour cette option.");
        for (const auto &row : engineOptions()) {
            const auto option = row.toMap();
            if (option["name"] != name)
                continue;
            const auto clean = t64::Uci::optionValue(option, value);
            auto values = settings_.value("uciOptions").toMap();
            values[name] = clean == "<empty>" ? QString() : clean;
            settings_.setValue("uciOptions", values);
            emit preferencesChanged();
            return;
        }
        need(false, "Démarrez ce moteur pour lire ses options UCI.");
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::pressEngineOption(const QString &name) {
    if (!engine_.pressOption(name))
        emit message(tr("Arrêtez l’analyse avant cette commande UCI."), true);
}
