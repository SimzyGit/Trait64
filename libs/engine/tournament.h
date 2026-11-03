// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "uci.h"
#include "chess.hpp"
#include <QTimer>
#include <memory>
#include <vector>

namespace t64 {
// Round-robin engine-vs-engine tournament. Each pairing plays both colours from the
// same opening; games are adjudicated with the full rules (mate, stalemate, repetition,
// fifty moves, insufficient material), illegal moves and engine failures lose.
class Tournament : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList standings READ standings NOTIFY changed)
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(int played READ played NOTIFY changed)
    Q_PROPERTY(int total READ total NOTIFY changed)
  public:
    struct Settings {
        int gamesPerPairing = 2; // rounded up to an even number so colours alternate
        int moveTime = 200;      // milliseconds per move
        int maxPlies = 300;      // longer games are adjudicated as draws
        QStringList openings;    // UCI move sequences, e.g. "e2e4 c7c5"
    };
    explicit Tournament(QObject *parent = nullptr);
    ~Tournament() override;
    static QStringList defaultOpenings();
    // Elo difference implied by a score fraction, bounded to ±800.
    static int eloDifference(double score);
    void start(const QVariantList &profiles, const Settings &settings);
    Q_INVOKABLE void stop();
    QVariantList standings() const;
    bool running() const { return running_; }
    QString status() const { return status_; }
    int played() const { return played_; }
    int total() const { return int(schedule_.size()); }
  signals:
    void changed();
    void gameFinished(const QString &pgn);
    void finished(int games);

  private:
    struct Player {
        std::unique_ptr<Uci> engine;
        QString name;
        double points = 0;
        int wins = 0, draws = 0, losses = 0;
        bool ready = false;
    };
    struct Pairing {
        int white = 0, black = 0;
        QString opening;
    };
    void nextGame();
    void requestMove();
    void onMove(int player, const QString &uci, const QString &fen);
    void finishGame(const QString &result, const QString &termination);
    void setStatus(const QString &text);
    std::vector<std::unique_ptr<Player>> players_;
    std::vector<Pairing> schedule_;
    Settings settings_;
    chess::Board board_;
    QStringList moves_, san_;
    QString startFen_;
    int played_ = 0, current_ = -1;
    bool running_ = false, inGame_ = false;
    QString status_, date_;
    QTimer watchdog_;
};
} // namespace t64
