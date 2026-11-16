// SPDX-License-Identifier: AGPL-3.0-or-later
// Suivre un tournoi en direct : les échiquiers d'une ronde se rafraîchissent toutes les
// vingt secondes tant que le panneau est ouvert, et la partie que l'on suit reçoit ses
// nouveaux coups dans son onglet, où le moteur peut l'analyser.
#include "controller.h"
#include <QFileInfo>
#include <sstream>

namespace {
constexpr int livePeriod = 20000;
}

void Controller::ensureBroadcasts() {
    if (broadcasts_)
        return;
    broadcasts_ = new t64::Broadcasts(this);
    liveTimer_ = new QTimer(this);
    liveTimer_->setInterval(livePeriod);
    connect(liveTimer_, &QTimer::timeout, this, &Controller::liveTick);
    connect(broadcasts_, &t64::Broadcasts::listed, this, [this](const QVariantList &list) {
        live_["list"] = list;
        live_["loading"] = false;
        live_.remove("error");
        emit liveChanged();
    });
    connect(broadcasts_, &t64::Broadcasts::roundReady, this, &Controller::liveShowRound);
    connect(broadcasts_, &t64::Broadcasts::pgnReady, this, &Controller::liveReceivedPgn);
    connect(broadcasts_, &t64::Broadcasts::failed, this, [this](const QString &error) {
        live_["error"] = error;
        live_["loading"] = false;
        emit liveChanged();
    });
}

void Controller::liveShowRound(const QVariantMap &round) {
    live_["round"] = round;
    live_["loading"] = false;
    live_.remove("error");
    // Chaque échiquier reçoit l'évaluation du moteur local, à profondeur modeste et l'un
    // après l'autre : une ronde de vingt parties coûte une seconde toutes les vingt.
    // Seules les positions nouvelles repartent au moteur.
    QVariantMap kept;
    liveQueue_.clear();
    for (const auto &game : round.value("games").toList()) {
        const auto fen = game.toMap().value("fen").toString();
        if (fen.isEmpty())
            continue;
        if (liveEvals_.contains(fen))
            kept[fen] = liveEvals_[fen];
        else if (fen != liveEvaluating_)
            liveQueue_ << fen;
    }
    liveEvals_ = kept;
    live_["evals"] = liveEvals_;
    emit liveChanged();
    const auto path = settings_.value("enginePath").toString();
    if (liveQueue_.isEmpty() || !QFileInfo(path).isExecutable())
        return;
    if (!liveEngine_) {
        liveEngine_ = new t64::Uci(this);
        connect(liveEngine_, &t64::Uci::completed, this,
                [this](const QString &fen, const QVariantList &lines) {
                    if (!lines.isEmpty() && fen == liveEvaluating_) {
                        const auto best = lines.first().toMap();
                        liveEvals_[fen] = QVariantMap{{"score", best["score"]}, {"eval", best["eval"]}};
                        live_["evals"] = liveEvals_;
                        emit liveChanged();
                    }
                    liveEvaluating_.clear();
                    liveNextEval();
                });
        connect(liveEngine_, &t64::Uci::failed, this, [this] {
            liveQueue_.clear();
            liveEvaluating_.clear();
        });
        liveEngine_->start(path, 1, 16, 1);
    }
    liveNextEval();
}

void Controller::liveNextEval() {
    if (!liveEngine_ || !liveEvaluating_.isEmpty() || liveQueue_.isEmpty())
        return;
    liveEvaluating_ = liveQueue_.takeFirst();
    liveEngine_->analyseToDepth(liveEvaluating_, "position fen " + liveEvaluating_, 14, 1500);
}

void Controller::liveList() {
    ensureBroadcasts();
    liveWatching_ = true;
    live_["loading"] = true;
    emit liveChanged();
    broadcasts_->list();
}

void Controller::liveOpenRound(const QVariantMap &entry) {
    ensureBroadcasts();
    liveWatching_ = true;
    live_["entry"] = entry;
    live_["round"] = QVariantMap();
    live_["loading"] = true;
    emit liveChanged();
    liveTick();
    liveTimer_->start();
}

void Controller::liveTick() {
    const auto entry = live_.value("entry").toMap();
    if (entry.isEmpty())
        return;
    if (liveWatching_)
        broadcasts_->round(entry["tourSlug"].toString(), entry["roundSlug"].toString(),
                           entry["roundId"].toString());
    if (!liveFollowed_.isEmpty())
        broadcasts_->pgn(entry["roundId"].toString());
    if (!liveWatching_ && liveFollowed_.isEmpty())
        liveTimer_->stop();
}

void Controller::liveFollow(int gameIndex) {
    const auto entry = live_.value("entry").toMap();
    if (entry.isEmpty() || !broadcasts_)
        return;
    livePendingGame_ = gameIndex;
    broadcasts_->pgn(entry["roundId"].toString());
}

void Controller::liveClose() {
    liveWatching_ = false;
    liveQueue_.clear();
    liveEvaluating_.clear();
    if (liveEngine_)
        liveEngine_->stop();
    if (liveTimer_ && liveFollowed_.isEmpty())
        liveTimer_->stop();
}

void Controller::liveStopFollowing() {
    liveFollowed_.clear();
    live_.remove("following");
    if (liveTimer_ && !liveWatching_)
        liveTimer_->stop();
    emit liveChanged();
}

void Controller::liveReceivedPgn(const QString &roundId, const QByteArray &pgn) {
    if (live_.value("entry").toMap()["roundId"].toString() != roundId)
        return;
    std::vector<t64::Game> games;
    try {
        std::istringstream in(pgn.toStdString());
        t64::readPgn(in, [&games](t64::Game &&g) { games.push_back(std::move(g)); });
    } catch (const std::exception &e) {
        live_["error"] = QString::fromUtf8(e.what());
        emit liveChanged();
        return;
    }
    const auto tag = [](const t64::Game &g, const char *name) {
        const auto found = g.tags.find(name);
        return found == g.tags.end() ? QString() : QString::fromStdString(found->second);
    };
    const auto key = [&](const t64::Game &g) {
        return QStringList{tag(g, "White"), tag(g, "Black"), tag(g, "Event")};
    };
    // Ouvrir une partie : celle dont les joueurs sont ceux de l'échiquier choisi.
    if (livePendingGame_ >= 0) {
        const auto boards = live_.value("round").toMap()["games"].toList();
        const auto chosen = boards.value(livePendingGame_).toMap();
        livePendingGame_ = -1;
        const auto white = chosen["white"].toMap()["name"].toString(),
                   black = chosen["black"].toMap()["name"].toString();
        for (auto &game : games)
            if (tag(game, "White") == white && tag(game, "Black") == black) {
                liveFollowed_ = key(game);
                live_["following"] = white + " — " + black;
                try {
                    replaceGame(std::move(game));
                    step(2);
                    emit showAnalysis();
                } catch (const std::exception &e) {
                    fail(e);
                }
                liveTimer_->start();
                emit liveChanged();
                return;
            }
        live_["error"] = tr("Partie introuvable dans le PGN de la ronde.");
        emit liveChanged();
        return;
    }
    // Tenir à jour la partie suivie, si c'est l'onglet affiché et que sa ligne principale
    // est toujours le début de la partie réelle : on n'écrase jamais ce qu'on y a changé.
    if (liveFollowed_.isEmpty() || key(game_) != liveFollowed_)
        return;
    for (const auto &fresh : games) {
        if (key(fresh) != liveFollowed_)
            continue;
        const int before = [this] {
            int n = 0;
            while (!game_.nodes[n].children.empty())
                n = game_.nodes[n].children[0];
            return n;
        }();
        const bool atEnd = node_ == before;
        bool added = false;
        const int mine = t64::extendMainline(game_, fresh, added);
        if (mine < 0)
            return; // la ligne principale a changé : on n'y touche pas
        const auto result = fresh.tags.find("Result");
        if (result != fresh.tags.end() && game_.tags["Result"] != result->second) {
            game_.tags["Result"] = result->second;
            added = true;
        }
        if (added) {
            refresh();
            if (atEnd)
                navigate(mine);
        }
        return;
    }
}
