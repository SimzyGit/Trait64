// SPDX-License-Identifier: AGPL-3.0-or-later
#include "tournament.h"
#include "game.h"
#include <QDate>
#include <QFileInfo>
#include <QThread>
#include <cmath>

namespace t64 {
Tournament::Tournament(QObject *parent) : QObject(parent) {
    watchdog_.setSingleShot(true);
    connect(&watchdog_, &QTimer::timeout, this, [this] {
        // An engine that never answers forfeits the game instead of stalling the tournament.
        if (!inGame_) return;
        const bool white = board_.sideToMove() == chess::Color::WHITE;
        finishGame(white ? "0-1" : "1-0", tr("%1 ne répond plus").arg(players_[white ? schedule_[current_].white
                                                                                         : schedule_[current_].black]->name));
    });
}
Tournament::~Tournament() { stop(); }

QStringList Tournament::defaultOpenings() {
    // Balanced, common main lines: varied structures without forcing a decisive opening.
    return {"e2e4 e7e5 g1f3 b8c6 f1b5", "e2e4 c7c5 g1f3 d7d6 d2d4", "e2e4 e7e6 d2d4 d7d5",
            "e2e4 c7c6 d2d4 d7d5", "d2d4 d7d5 c2c4 e7e6 b1c3", "d2d4 g8f6 c2c4 g7g6 b1c3",
            "d2d4 g8f6 c2c4 e7e6 g1f3", "c2c4 e7e5 b1c3 g8f6", "g1f3 d7d5 g2g3 g8f6",
            "e2e4 e7e5 g1f3 g8f6"};
}

int Tournament::eloDifference(double score) {
    const double s = std::clamp(score, 0.01, 0.99);
    return std::clamp(int(std::lround(-400.0 * std::log10(1.0 / s - 1.0))), -800, 800);
}

void Tournament::start(const QVariantList &profiles, const Settings &settings) {
    if (profiles.size() < 2 || profiles.size() > 8)
        throw std::runtime_error("Sélectionnez entre deux et huit profils UCI.");
    for (const auto &profile : profiles)
        if (!QFileInfo(profile.toMap()["enginePath"].toString()).isExecutable())
            throw std::runtime_error("Un profil pointe vers un moteur introuvable.");
    stop();
    settings_ = settings;
    settings_.gamesPerPairing = std::clamp(settings.gamesPerPairing + settings.gamesPerPairing % 2, 2, 100);
    settings_.moveTime = std::clamp(settings.moveTime, 100, 10000);
    settings_.maxPlies = std::clamp(settings.maxPlies, 20, 1000);
    if (settings_.openings.isEmpty()) settings_.openings = defaultOpenings();
    for (const auto &line : settings_.openings) {
        chess::Board board;
        for (const auto &uci : line.split(' ', Qt::SkipEmptyParts)) {
            chess::Movelist legal;
            chess::movegen::legalmoves(legal, board);
            const auto it = std::find_if(legal.begin(), legal.end(), [&](const chess::Move &m) {
                return QString::fromStdString(chess::uci::moveToUci(m)) == uci;
            });
            if (it == legal.end()) throw std::runtime_error("Ouverture invalide : " + line.toStdString());
            board.makeMove(*it);
        }
    }
    // Only two engines think at once; give each half of the processors.
    const int threads = std::max(1, std::min(32, QThread::idealThreadCount()) / 2);
    const int hash = std::max(16, 1024 / int(profiles.size()));
    players_.clear();
    for (int i = 0; i < profiles.size(); ++i) {
        const auto p = profiles[i].toMap();
        auto player = std::make_unique<Player>();
        player->name = p.value("name", tr("Moteur %1").arg(i + 1)).toString();
        player->engine = std::make_unique<Uci>();
        auto *engine = player->engine.get();
        connect(engine, &Uci::initialized, this, [this, i] {
            players_[i]->ready = true;
            if (!inGame_ && running_ && current_ < 0) nextGame();
        });
        connect(engine, &Uci::bestMove, this, [this, i](const QString &uci, const QString &fen) { onMove(i, uci, fen); });
        connect(engine, &Uci::failed, this, [this, i](const QString &reason) {
            if (!running_) return;
            if (!inGame_) {
                setStatus(players_[i]->name + " : " + reason);
                stop();
                return;
            }
            const auto &pairing = schedule_[current_];
            if (pairing.white == i || pairing.black == i)
                finishGame(pairing.white == i ? "0-1" : "1-0", players_[i]->name + " : " + reason);
        });
        auto options = p["uciOptions"].toMap();
        options["UCI_LimitStrength"] = false;
        engine->start(p["enginePath"].toString(), std::clamp(p.value("threads", 1).toInt(), 1, threads),
                      std::clamp(p.value("hash", 128).toInt(), 16, hash), 1, 20, p["syzygyPath"].toString(), options);
        players_.push_back(std::move(player));
    }
    schedule_.clear();
    int opening = 0;
    for (int a = 0; a < int(players_.size()); ++a)
        for (int b = a + 1; b < int(players_.size()); ++b)
            for (int g = 0; g < settings_.gamesPerPairing; g += 2) {
                const auto &line = settings_.openings[opening++ % settings_.openings.size()];
                schedule_.push_back({a, b, line});
                schedule_.push_back({b, a, line});
            }
    played_ = 0;
    current_ = -1;
    date_ = QDate::currentDate().toString("yyyy.MM.dd");
    running_ = true;
    setStatus(tr("Démarrage des moteurs…"));
}

void Tournament::stop() {
    watchdog_.stop();
    const bool wasRunning = running_;
    running_ = false;
    inGame_ = false;
    for (auto &player : players_) player->engine->shutdown();
    if (wasRunning) {
        setStatus(tr("Arrêté après %1 partie(s).").arg(played_));
        emit finished(played_);
    }
}

void Tournament::nextGame() {
    for (const auto &player : players_)
        if (!player->ready) return;
    if (++current_ >= int(schedule_.size())) {
        running_ = false;
        for (auto &player : players_) player->engine->shutdown();
        setStatus(tr("Tournoi terminé : %1 parties.").arg(played_));
        emit finished(played_);
        return;
    }
    const auto &pairing = schedule_[current_];
    board_ = chess::Board();
    startFen_ = QString::fromStdString(board_.getFen());
    moves_.clear();
    san_.clear();
    for (const auto &uci : pairing.opening.split(' ', Qt::SkipEmptyParts)) {
        const auto move = chess::uci::uciToMove(board_, uci.toStdString());
        san_ << QString::fromStdString(chess::uci::moveToSan(board_, move));
        board_.makeMove(move);
        moves_ << uci;
    }
    inGame_ = true;
    setStatus(tr("Partie %1/%2 : %3 – %4")
                  .arg(current_ + 1)
                  .arg(schedule_.size())
                  .arg(players_[pairing.white]->name, players_[pairing.black]->name));
    requestMove();
}

void Tournament::requestMove() {
    const auto &pairing = schedule_[current_];
    const int mover = board_.sideToMove() == chess::Color::WHITE ? pairing.white : pairing.black;
    if (!players_[mover]->engine->ready()) {
        QTimer::singleShot(0, this, [this, mover, white = board_.sideToMove() == chess::Color::WHITE] {
            if (inGame_) finishGame(white ? "0-1" : "1-0", tr("%1 est arrêté").arg(players_[mover]->name));
        });
        return;
    }
    const QString position = "position startpos" + (moves_.isEmpty() ? QString() : " moves " + moves_.join(' '));
    watchdog_.start(settings_.moveTime + 15000);
    players_[mover]->engine->analyse(QString::fromStdString(board_.getFen()), position, true, settings_.moveTime);
}

void Tournament::onMove(int player, const QString &uci, const QString &fen) {
    if (!running_ || !inGame_) return;
    const auto &pairing = schedule_[current_];
    const bool white = board_.sideToMove() == chess::Color::WHITE;
    if (player != (white ? pairing.white : pairing.black) || fen != QString::fromStdString(board_.getFen())) return;
    watchdog_.stop();
    chess::Movelist legal;
    chess::movegen::legalmoves(legal, board_);
    chess::Move move = chess::Move::NO_MOVE;
    for (const auto &candidate : legal)
        if (QString::fromStdString(chess::uci::moveToUci(candidate)) == uci) move = candidate;
    if (move == chess::Move::NO_MOVE)
        return finishGame(white ? "0-1" : "1-0", tr("coup illégal %1 de %2").arg(uci, players_[player]->name));
    san_ << QString::fromStdString(chess::uci::moveToSan(board_, move));
    board_.makeMove(move);
    moves_ << uci;
    const auto [reason, result] = gameResult(board_);
    using Reason = chess::GameResultReason;
    if (reason == Reason::CHECKMATE) return finishGame(white ? "1-0" : "0-1", tr("mat"));
    if (reason == Reason::STALEMATE) return finishGame("1/2-1/2", tr("pat"));
    if (reason == Reason::INSUFFICIENT_MATERIAL) return finishGame("1/2-1/2", tr("matériel insuffisant"));
    if (reason == Reason::FIFTY_MOVE_RULE) return finishGame("1/2-1/2", tr("règle des 50 coups"));
    if (reason == Reason::THREEFOLD_REPETITION) return finishGame("1/2-1/2", tr("triple répétition"));
    if (moves_.size() >= settings_.maxPlies)
        return finishGame("1/2-1/2", tr("nulle adjugée après %1 demi-coups").arg(settings_.maxPlies));
    requestMove();
}

void Tournament::finishGame(const QString &result, const QString &termination) {
    watchdog_.stop();
    inGame_ = false;
    const auto &pairing = schedule_[current_];
    auto &white = *players_[pairing.white];
    auto &black = *players_[pairing.black];
    if (result == "1-0") { white.points += 1; ++white.wins; ++black.losses; }
    else if (result == "0-1") { black.points += 1; ++black.wins; ++white.losses; }
    else { white.points += .5; black.points += .5; ++white.draws; ++black.draws; }
    ++played_;
    auto tag = [](const QString &name, const QString &value) {
        QString clean = value;
        clean.replace('\\', "\\\\").replace('"', "\\\"").replace('\n', ' ');
        return "[" + name + " \"" + clean + "\"]\n";
    };
    QString pgn = tag("Event", tr("Tournoi de moteurs Trait64")) + tag("Site", "Trait64") + tag("Date", date_) +
                  tag("Round", QString::number(current_ + 1)) + tag("White", white.name) + tag("Black", black.name) +
                  tag("Result", result) + tag("TimeControl", QString("%1 ms/coup").arg(settings_.moveTime)) +
                  tag("Termination", termination) + "\n";
    QStringList text;
    for (int i = 0; i < san_.size(); ++i) {
        if (i % 2 == 0) text << QString::number(i / 2 + 1) + ".";
        text << san_[i];
    }
    text << "{" + termination + "}" << result;
    pgn += text.join(' ') + "\n\n";
    emit gameFinished(pgn);
    emit changed();
    if (running_) QTimer::singleShot(0, this, [this] { if (running_ && !inGame_) nextGame(); });
}

void Tournament::setStatus(const QString &text) {
    status_ = text;
    emit changed();
}

QVariantList Tournament::standings() const {
    std::vector<const Player *> order;
    for (const auto &player : players_) order.push_back(player.get());
    std::stable_sort(order.begin(), order.end(), [](auto *a, auto *b) { return a->points > b->points; });
    QVariantList rows;
    for (const auto *player : order) {
        const int games = player->wins + player->draws + player->losses;
        rows << QVariantMap{{"name", player->name}, {"points", player->points}, {"games", games},
                            {"wins", player->wins}, {"draws", player->draws}, {"losses", player->losses},
                            {"elo", games ? eloDifference(player->points / games) : 0}};
    }
    return rows;
}
} // namespace t64
