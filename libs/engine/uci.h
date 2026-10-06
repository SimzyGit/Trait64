// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QTimer>
#include <QVariantList>
namespace t64 {
class Uci : public QObject {
    Q_OBJECT
  public:
    explicit Uci(QObject *parent = nullptr);
    ~Uci() override;
    void start(const QString &executable, int threads = 1, int hash = 128, int multiPV = 3, int skill = 20);
    void analyse(const QString &fen, const QString &positionCommand, bool play = false,
                 int milliseconds = 800);
    void stop();
    void shutdown();
    QString name() const { return name_; }
    bool ready() const { return state_ == Ready || state_ == Searching || state_ == Stopping; }
  signals:
    void status(const QString &text);
    void lines(const QVariantList &lines);
    void bestMove(const QString &uci, const QString &fen);

  private:
    enum State { Off, Handshake, WaitingReady, Ready, Searching, Stopping } state_ = Off;
    QProcess process_;
    QTimer timeout_;
    QByteArray buffer_;
    QString name_, activeFen_, pendingFen_, pendingPosition_;
    bool pending_ = false, pendingPlay_ = false, activePlay_ = false;
    int threads_ = 1, hash_ = 128, multiPV_ = 3, skill_ = 20, moveTime_ = 800;
    QMap<int, QVariantMap> variations_;
    QSet<QString> options_;
    void command(const QString &text);
    void consume();
    void parse(const QString &line);
    void beginSearch();
};
} // namespace t64
