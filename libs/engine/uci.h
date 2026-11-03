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
    void start(const QString &executable, int threads = 1, int hash = 128, int multiPV = 3, int skill = 20,
               const QString &syzygyPath = {}, const QVariantMap &customOptions = {});
    QVariantList options() const { return descriptors_; }
    // Playing strength in Elo for the next start (0 = full strength / Skill Level only).
    // Applied through UCI_LimitStrength + UCI_Elo when the engine offers them.
    void setLimitElo(int elo) { limitElo_ = elo > 0 ? elo : 0; }
    // Elo actually applied after the handshake (clamped to the engine's range), 0 if none.
    int appliedElo() const { return appliedElo_; }
    QString executable() const { return process_.program(); }
    static QVariantMap parseOption(const QString &line);
    static QVariantMap parseInfo(const QString &line, const QString &fen, bool chess960 = false);
    static QString optionValue(const QVariantMap &descriptor, const QVariant &value, bool clampSpin = false);
    bool pressOption(const QString &name);
    void analyse(const QString &fen, const QString &positionCommand, bool play = false,
                 int milliseconds = 800, int depth = 24, bool chess960 = false);
    // Bilan de masse : on veut un meilleur coup, comme en partie, mais c'est la profondeur
    // qui décide de l'arrêt. Le temps ne sert que de plafond, pour qu'une position coriace
    // ne retienne pas la file. Les parties jouées, elles, restent bornées par le seul temps.
    void analyseToDepth(const QString &fen, const QString &positionCommand, int depth,
                        int millisecondsCeiling, bool chess960 = false);
    void stop();
    void shutdown();
    QString name() const { return name_; }
    bool ready() const { return state_ == Ready || state_ == Searching || state_ == Stopping; }
  signals:
    void initialized();
    void status(const QString &text);
    void lines(const QVariantList &lines);
    void bestMove(const QString &uci, const QString &fen);
    void completed(const QString &fen, const QVariantList &lines);
    void failed(const QString &reason);
    void optionsChanged();

  private:
    enum State { Off, Handshake, WaitingReady, Ready, Searching, Stopping } state_ = Off;
    int limitElo_ = 0, appliedElo_ = 0;
    QProcess process_;
    QTimer timeout_;
    QByteArray buffer_;
    QString name_, activeFen_, pendingFen_, pendingPosition_, syzygyPath_;
    bool pending_ = false, pendingPlay_ = false, activePlay_ = false;
    bool pendingDepthCeiling_ = false, activeDepthCeiling_ = false;
    bool pending960_ = false, active960_ = false;
    int depth_ = 24;
    int threads_ = 1, hash_ = 128, multiPV_ = 3, skill_ = 20, moveTime_ = 800;
    QMap<int, QVariantMap> variations_, exactVariations_;
    QSet<QString> options_;
    QVariantList descriptors_;
    QVariantMap customOptions_;
    quint64 generation_ = 0;
    void start(const QString &fen, const QString &positionCommand, bool play, int milliseconds,
               int depth, bool chess960, bool depthCeiling);
    void command(const QString &text);
    void consume();
    void parse(const QString &line);
    void beginSearch();
};
} // namespace t64
