// SPDX-License-Identifier: AGPL-3.0-or-later
// L'exercice de Stoyko : devant une position, on écrit toutes les variantes que l'on
// calcule, puis le moteur juge la fin de chacune et dit ce qu'il joue lui-même. On voit
// ainsi où le calcul s'est trompé, sans que le moteur ait soufflé pendant l'exercice.
#include "controller.h"
#include <QFileInfo>
#include <stdexcept>

namespace {
QString s(const std::string &text) {
    return QString::fromStdString(text);
}
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
// Le texte d'une variante, de la position de départ à `leaf`, numéros compris.
QString lineText(const t64::Game &game, int from, int leaf) {
    std::vector<int> path;
    for (int n = leaf; n != from && n > 0; n = game.nodes[n].parent)
        path.insert(path.begin(), n);
    QStringList words;
    for (size_t i = 0; i < path.size(); ++i) {
        const auto &node = game.nodes[path[i]];
        const auto &before = game.boardOf(node.parent);
        const bool white = before.sideToMove() == chess::Color::WHITE;
        const int number = before.fullMoveNumber();
        if (white)
            words << QString::number(number) + ".";
        else if (i == 0)
            words << QString::number(number) + "…";
        words << QString::fromStdString(node.san);
    }
    return words.join(' ');
}
} // namespace

void Controller::startStoyko() {
    try {
        require(!trainingActive(), "Terminez d’abord l’entraînement en cours.");
        const auto path = settings_.value("enginePath").toString();
        require(QFileInfo(path).isExecutable(), "Choisissez d’abord un moteur (Préférences → Moteurs).");
        // Toutes les fins de variantes sous la position courante, la ligne principale d'abord.
        QVariantList lines;
        std::vector<int> pending{node_};
        while (!pending.empty() && lines.size() < 40) {
            const int n = pending.back();
            pending.pop_back();
            const auto &children = game_.nodes[n].children;
            if (children.empty() && n != node_)
                lines << QVariantMap{{"text", lineText(game_, node_, n)}, {"fen", s(game_.nodes[n].fen)}};
            for (auto it = children.rbegin(); it != children.rend(); ++it)
                pending.push_back(*it);
        }
        require(!lines.isEmpty(),
                "Écrivez d’abord vos variantes depuis cette position, puis lancez la vérification.");
        stopStoyko();
        stoyko_ = {{"running", true},
                   {"start", s(game_.nodes[node_].fen)},
                   {"lines", lines},
                   {"evals", QVariantMap{}}};
        stoykoQueue_.clear();
        stoykoQueue_ << s(game_.nodes[node_].fen);
        for (const auto &line : lines)
            if (!stoykoQueue_.contains(line.toMap()["fen"].toString()))
                stoykoQueue_ << line.toMap()["fen"].toString();
        // Un moteur changé dans les préférences, ou tombé en panne, est remplacé.
        if (stoykoEngine_ && stoykoEngine_->property("t64Path").toString() != path) {
            stoykoEngine_->deleteLater();
            stoykoEngine_ = nullptr;
        }
        if (!stoykoEngine_) {
            stoykoEngine_ = new t64::Uci(this);
            stoykoEngine_->setProperty("t64Path", path);
            connect(stoykoEngine_, &t64::Uci::completed, this,
                    [this](const QString &fen, const QVariantList &found) {
                        if (fen != stoykoEvaluating_)
                            return;
                        auto evals = stoyko_["evals"].toMap();
                        QVariantMap result;
                        if (!found.isEmpty()) {
                            const auto best = found.first().toMap();
                            result = {
                                {"score", best["score"]}, {"eval", best["eval"]}, {"best", best["san"]}};
                        } else {
                            // Plus aucun coup : le moteur ne dit rien, l'échiquier suffit.
                            const auto board = t64::checkedBoard(fen.toStdString());
                            const bool mated = board.inCheck();
                            const bool whiteMated = board.sideToMove() == chess::Color::WHITE;
                            result = {{"score", mated ? (whiteMated ? -100 : 100) : 0},
                                      {"eval", mated ? tr("mat") : tr("pat")}};
                        }
                        evals[fen] = result;
                        stoyko_["evals"] = evals;
                        stoykoEvaluating_.clear();
                        stoykoNext();
                    });
            connect(stoykoEngine_, &t64::Uci::failed, this, [this](const QString &error) {
                stoykoQueue_.clear();
                stoykoEvaluating_.clear();
                stoyko_["running"] = false;
                stoyko_["error"] = error;
                stoykoEngine_->setProperty("t64Path", QString());
                emit stoykoChanged();
            });
            stoykoEngine_->start(path, settings_.value("threads", 1).toInt(),
                                 settings_.value("hash", 64).toInt(), 1);
        }
        emit stoykoChanged();
        stoykoNext();
    } catch (const std::exception &e) {
        stoyko_ = {{"error", QString::fromUtf8(e.what())}};
        emit stoykoChanged();
        fail(e);
    }
}

void Controller::stoykoNext() {
    if (!stoykoEvaluating_.isEmpty())
        return;
    if (stoykoQueue_.isEmpty()) {
        stoyko_["running"] = false;
        emit stoykoChanged();
        return;
    }
    emit stoykoChanged();
    stoykoEvaluating_ = stoykoQueue_.takeFirst();
    stoykoEngine_->analyseToDepth(stoykoEvaluating_, "position fen " + stoykoEvaluating_, 18, 3000);
}

void Controller::stopStoyko() {
    stoykoQueue_.clear();
    stoykoEvaluating_.clear();
    if (stoykoEngine_)
        stoykoEngine_->stop();
    if (stoyko_["running"].toBool()) {
        stoyko_["running"] = false;
        emit stoykoChanged();
    }
}
