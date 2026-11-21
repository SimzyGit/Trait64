// SPDX-License-Identifier: AGPL-3.0-or-later
#include "comparison.h"
#include <QFileInfo>
#include <QThread>
#include <algorithm>
#include <stdexcept>
namespace t64 {
Comparison::Comparison(QObject *parent) : QObject(parent) {
    debounce_.setSingleShot(true);
    debounce_.setInterval(180);
    notify_.setSingleShot(true);
    notify_.setInterval(50);
    connect(&notify_, &QTimer::timeout, this, &Comparison::changed);
    connect(&debounce_, &QTimer::timeout, this, [this] {
        if (!running_)
            return;
        for (const auto &s : slots_)
            s->engine->analyse(fen_, position_, false, 800, s->depth, chess960_);
    });
}
Comparison::~Comparison() {
    stop();
}
QVariantList Comparison::rows() const {
    QVariantList result;
    for (const auto &s : slots_)
        result << s->data;
    return result;
}
QVariantMap Comparison::synthesis(const QVariantList &rows) {
    QMap<QString, QStringList> byMove;
    QList<double> scores;
    int engines = 0;
    bool mate = false;
    for (const auto &value : rows) {
        const auto row = value.toMap();
        const auto lines = row["lines"].toList();
        if (lines.isEmpty())
            continue;
        const auto best = lines.first().toMap();
        const auto move = best["san"].toString().section(' ', 0, 0);
        if (move.isEmpty())
            continue;
        ++engines;
        byMove[move] << row["name"].toString();
        if (best["scoreType"] == "cp")
            scores << best["score"].toDouble();
        else
            mate = true;
    }
    if (engines < 2)
        return {{"engines", engines}};
    QString chosen;
    for (auto it = byMove.cbegin(); it != byMove.cend(); ++it)
        if (chosen.isEmpty() || it.value().size() > byMove[chosen].size())
            chosen = it.key();
    double spread = 0;
    if (scores.size() >= 2) {
        const auto [low, high] = std::minmax_element(scores.cbegin(), scores.cend());
        spread = *high - *low;
    }
    QVariantList others;
    for (auto it = byMove.cbegin(); it != byMove.cend(); ++it)
        if (it.key() != chosen)
            others << QVariantMap{{"move", it.key()}, {"engines", it.value()}};
    return {{"engines", engines}, {"agree", int(byMove[chosen].size())},
            {"move", chosen},     {"spread", qRound(spread * 100) / 100.0},
            {"mate", mate},       {"others", others}};
}
void Comparison::scheduleChanged() {
    if (!notify_.isActive())
        notify_.start();
}
void Comparison::start(const QVariantList &profiles, const QString &fen, const QString &position,
                       bool chess960) {
    if (profiles.size() < 2 || profiles.size() > 4)
        throw std::runtime_error("Sélectionnez entre deux et quatre profils UCI.");
    for (const auto &profile : profiles)
        if (!QFileInfo(profile.toMap()["enginePath"].toString()).isExecutable())
            throw std::runtime_error("Un profil pointe vers un moteur introuvable.");
    stop();
    slots_.clear();
    running_ = true;
    const int cpuShare = std::max(1, std::min(32, QThread::idealThreadCount()) / int(profiles.size()));
    const int memoryShare = 1024 / profiles.size();
    for (const auto &profile : profiles) {
        const auto p = profile.toMap();
        auto slot = std::make_unique<Slot>();
        slot->engine = std::make_unique<Uci>();
        slot->depth = std::clamp(p.value("analysisDepth", 24).toInt(), 1, 99);
        const int threads = std::clamp(p.value("threads", 1).toInt(), 1, cpuShare);
        const int hash = std::clamp(p.value("hash", 128).toInt(), 16, memoryShare);
        slot->data = {{"name", p.value("name", "Moteur UCI")},
                      {"status", "Connexion…"},
                      {"lines", QVariantList()},
                      {"threads", threads},
                      {"hash", hash},
                      {"fen", fen}};
        auto *s = slot.get();
        connect(s->engine.get(), &Uci::status, this, [this, s](const QString &status) {
            s->data["status"] = status;
            scheduleChanged();
        });
        connect(s->engine.get(), &Uci::failed, this, [this, s](const QString &reason) {
            s->data["status"] = reason;
            s->data["error"] = true;
            scheduleChanged();
        });
        connect(s->engine.get(), &Uci::lines, this, [this, s](const QVariantList &lines) {
            if (!running_ || (!lines.empty() && lines.first().toMap()["fen"].toString() != fen_))
                return;
            s->data["lines"] = lines;
            s->data["fen"] = fen_;
            scheduleChanged();
        });
        connect(s->engine.get(), &Uci::completed, this,
                [this, s](const QString &answered, const QVariantList &lines) {
                    if (!running_ || answered != fen_)
                        return;
                    s->data["lines"] = lines;
                    s->data["status"] = s->engine->name() + " · analyse terminée";
                    scheduleChanged();
                });
        slots_.push_back(std::move(slot));
        auto options = p["uciOptions"].toMap();
        options["UCI_LimitStrength"] = false;
        s->engine->start(p["enginePath"].toString(), threads, hash, p.value("multiPV", 3).toInt(), 20,
                         p["syzygyPath"].toString(), options);
    }
    fen_.clear();
    setPosition(fen, position, chess960);
}
void Comparison::setPosition(const QString &fen, const QString &position, bool chess960) {
    if (!running_ || (fen == fen_ && position == position_ && chess960 == chess960_))
        return;
    fen_ = fen;
    position_ = position;
    chess960_ = chess960;
    for (const auto &s : slots_) {
        s->engine->stop();
        s->data["lines"] = QVariantList();
        s->data["fen"] = fen;
    }
    debounce_.start();
    emit changed();
}
void Comparison::stop() {
    running_ = false;
    debounce_.stop();
    notify_.stop();
    for (const auto &s : slots_) {
        s->engine->shutdown();
        s->data["status"] = "Arrêté";
    }
    notify_.stop();
    emit changed();
}
} // namespace t64
