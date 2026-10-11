// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include <QDate>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
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
    engineLines_.clear();
    emit engineChanged();
    refresh();
    autosave_.start();
}
void Controller::remember() {
    commentNode_ = -1;
    cancelReview();
    if (cardId_)
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
    if (cardId_ || undo_.empty() || clockRunning_)
        return;
    stopPlaying();
    cancelReview();
    review_.clear();
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
    if (cardId_ || redo_.empty() || clockRunning_)
        return;
    stopPlaying();
    cancelReview();
    review_.clear();
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
        if (cardId_)
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
        if (cardId_)
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
            auto json = QJsonDocument::fromJson(f.readAll()).object();
            auto tabs = json["tabs"].toArray();
            if (!tabs.empty() && tabs.size() <= 16) {
                std::vector<Study> recovered;
                for (auto v : tabs) {
                    auto tab = v.toObject();
                    auto games = t64::parsePgn(tab["pgn"].toString().toStdString());
                    need(games.size() == 1, "Onglet de session invalide.");
                    int node = 0;
                    for (auto m : tab["path"].toArray())
                        node = games[0].play(node, m.toString().toStdString());
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
                emit message("Certains onglets n’ont pas été restaurés. Session originale conservée : " +
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
    if (cardId_ || !QRegularExpression("^[a-h][1-8]$").match(from).hasMatch() ||
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
    if (cardId_)
        return;
    remember();
    QString text = qs(game_.nodes[node_].comment);
    text.remove(QRegularExpression("\\[%c(?:al|sl)\\s+[^\\]]+\\]"));
    game_.nodes[node_].comment = text.trimmed().toStdString();
    refresh(true);
}
void Controller::mergeGame(qint64 id) {
    try {
        need(!cardId_, "Terminez l’entraînement.");
        auto copy = game_;
        copy.merge(db_->load(id));
        remember();
        game_ = std::move(copy);
        review_.clear();
        refresh(true);
        emit reviewChanged();
        emit message("Variantes et commentaires fusionnés dans l’étude courante.");
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::searchAdvanced(const QVariantMap &filters) {
    advancedFilters_ = filters;
    refreshLibrary();
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
        emit message(text.empty() ? "Aucun doublon de ligne principale." : text.join('\n'));
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
    emit message("Toutes les cartes dues ont été révisées.");
}
void Controller::saveEngineProfile(const QString &name) {
    if (name.trimmed().isEmpty())
        return;
    auto list = settings_.value("engineProfiles").toList();
    QVariantMap p{{"name", name.trimmed().left(80)}};
    for (const auto &k : QStringList{"enginePath", "threads", "hash", "multiPV", "skill", "moveTime",
                                     "analysisDepth", "syzygyPath", "uciOptions"})
        p[k] = preferences()[k];
    for (int i = 0; i < list.size(); ++i)
        if (list[i].toMap()["name"] == p["name"]) {
            list.removeAt(i);
            break;
        }
    if (list.size() >= 20) {
        emit message("20 profils maximum.", true);
        return;
    }
    list << p;
    settings_.setValue("engineProfiles", list);
    emit preferencesChanged();
}
void Controller::compareEngines(const QVariantList &indices) {
    try {
        need(!cardId_ && !busy_, "Terminez l’entraînement ou le traitement en cours.");
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
void Controller::addComparisonLine(int engine, int variation) {
    try {
        need(!cardId_, "Terminez l’entraînement.");
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
    return {{"running", reviewing_},
            {"done", reviewIndex_},
            {"total", int(reviewNodes_.size())},
            {"error", reviewError_},
            {"milliseconds", reviewMilliseconds_}};
}
void Controller::analyseGame(int milliseconds) {
    if (cardId_)
        return;
    cancelReview();
    stopPlaying();
    review_.clear();
    reviewScores_.clear();
    reviewError_.clear();
    ++reviewEpoch_;
    reviewGame_ = game_;
    reviewNodes_ = game_.mainline();
    reviewIndex_ = 0;
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
    if (reviewIndex_ >= int(reviewNodes_.size())) {
        reviewing_ = false;
        reviewer_.shutdown();
        emit reviewChanged();
        emit message("Analyse de la ligne principale terminée.");
        return;
    }
    const int node = reviewNodes_[reviewIndex_];
    auto board = reviewGame_.boardAt(node);
    auto end = board.isGameOver();
    if (end.second != chess::GameResult::NONE) {
        double score = end.first == chess::GameResultReason::CHECKMATE
                           ? (board.sideToMove() == chess::Color::WHITE ? -100. : 100.)
                           : 0.;
        const auto fen = qs(reviewGame_.nodes[node].fen);
        const int epoch = reviewEpoch_;
        QTimer::singleShot(0, this, [this, fen, epoch, score] {
            if (epoch != reviewEpoch_)
                return;
            finishReviewPosition(fen, {QVariantMap{{"score", score},
                                                   {"eval", score == 0  ? "0.00"
                                                            : score > 0 ? "+M0"
                                                                        : "−M0"},
                                                   {"depth", 0},
                                                   {"uci", ""},
                                                   {"bound", false}}});
        });
        return;
    }
    reviewer_.analyse(qs(reviewGame_.nodes[node].fen), position(reviewGame_, node), true, reviewMilliseconds_,
                      24, reviewGame_.chess960);
}
void Controller::finishReviewPosition(const QString &fen, const QVariantList &lines) {
    if (!reviewing_ || reviewIndex_ >= int(reviewNodes_.size()) ||
        fen != qs(reviewGame_.nodes[reviewNodes_[reviewIndex_]].fen))
        return;
    if (lines.empty() || lines.first().toMap()["bound"].toBool()) {
        reviewing_ = false;
        reviewError_ = "Le moteur n’a pas fourni une évaluation exacte ; relancez avec davantage de temps.";
        emit reviewChanged();
        return;
    }
    QVariantMap score = lines.first().toMap();
    reviewScores_ << score;
    if (reviewIndex_ > 0) {
        int node = reviewNodes_[reviewIndex_], parent = reviewNodes_[reviewIndex_ - 1];
        auto previous = reviewScores_[reviewIndex_ - 1].toMap();
        const bool white = reviewGame_.boardAt(parent).sideToMove() == chess::Color::WHITE;
        const double loss =
            std::max(0., (previous["score"].toDouble() - score["score"].toDouble()) * (white ? 100 : -100));
        QString label = loss >= 250   ? "Gaffe"
                        : loss >= 100 ? "Erreur"
                        : loss >= 50  ? "Imprécision"
                                      : "Correct";
        review_ << QVariantMap{{"node", node},
                               {"parent", parent},
                               {"move", qs(reviewGame_.nodes[node].san)},
                               {"white", white},
                               {"score", score["score"]},
                               {"eval", score["eval"]},
                               {"loss", qRound(loss)},
                               {"label", label},
                               {"best", previous["uci"]},
                               {"fen", qs(reviewGame_.nodes[parent].fen)},
                               {"depth", score["depth"]}};
    }
    ++reviewIndex_;
    emit reviewChanged();
    const int epoch = reviewEpoch_;
    QTimer::singleShot(0, this, [this, epoch] {
        if (epoch == reviewEpoch_)
            requestReviewPosition();
    });
}
void Controller::annotateReview() {
    if (reviewing_ || review_.empty() || cardId_)
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
    emit message("Évaluations et erreurs ajoutées aux annotations PGN.");
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
        emit message(QString("%1 carte(s) créée(s) depuis les erreurs analysées.").arg(count));
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::startLocalGame(int minutes, int increment, int opponent, bool humanWhite) {
    if (cardId_)
        return;
    stopPlaying();
    remember();
    review_.clear();
    emit reviewChanged();
    game_.tags["Result"] = "*";
    clockResult_.clear();
    whiteMs_ = blackMs_ = std::clamp(minutes, 1, 180) * 60000;
    incrementMs_ = std::clamp(increment, 0, 120) * 1000;
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
    auto e = b.isGameOver();
    if (e.second == chess::GameResult::NONE) {
        emit message("Aucune nulle réclamable dans cette position.");
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
        emit message("Étude exportée en PDF.");
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::identifyOpening() {
    if (cardId_)
        return;
    const auto eco = state_["openingEco"].toString(), name = state_["openingName"].toString();
    if (eco.isEmpty()) {
        emit message("Aucune ouverture nommée trouvée sur ce chemin.");
        return;
    }
    remember();
    game_.tags["ECO"] = eco.toStdString();
    game_.tags["Opening"] = name.toStdString();
    refresh(true);
    emit message("Ouverture ajoutée aux informations de la partie.");
}
void Controller::exportDiagram(const QString &url) {
    try {
        db_->validateOutputPath(localPath(url));
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
        emit message("Diagramme PNG exporté avec votre apparence et vos flèches.");
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
        emit message("Arrêtez l’analyse avant cette commande UCI.", true);
}
