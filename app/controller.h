// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "database.h"
#include "uci.h"
#include <QFutureWatcher>
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <memory>

class Controller : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(QVariantList squares READ squares NOTIFY changed)
    Q_PROPERTY(QVariantList notation READ notation NOTIFY changed)
    Q_PROPERTY(QVariantList games READ games NOTIFY libraryChanged)
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList openings READ openings NOTIFY changed)
    Q_PROPERTY(QVariantList cards READ cards NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList engineLines READ engineLines NOTIFY engineChanged)
    Q_PROPERTY(QString engineStatus READ engineStatus NOTIFY engineChanged)
    Q_PROPERTY(QVariantMap preferences READ preferences NOTIFY preferencesChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
  public:
    explicit Controller(const QString &dataPath, QObject *parent = nullptr);
    ~Controller() override;
    QVariantMap state() const { return state_; }
    QVariantList squares() const { return squares_; }
    QVariantList notation() const { return notation_; }
    QVariantList games() const { return games_; }
    QVariantMap stats() const { return stats_; }
    QVariantList openings() const { return openings_; }
    QVariantList cards() const { return cards_; }
    QVariantList engineLines() const { return engineLines_; }
    QString engineStatus() const { return engineStatus_; }
    QVariantMap preferences() const;
    bool busy() const { return busy_; }
    bool smokeTest(QString *error);
    Q_INVOKABLE void newGame();
    Q_INVOKABLE void loadGame(qint64 id);
    Q_INVOKABLE void saveGame(bool copy = false);
    Q_INVOKABLE void setFen(const QString &fen);
    Q_INVOKABLE void importText(const QString &text);
    Q_INVOKABLE void importFile(const QString &url);
    Q_INVOKABLE void exportFile(const QString &url, bool all = false);
    Q_INVOKABLE void backup(const QString &url);
    Q_INVOKABLE void openBackup(const QString &url);
    Q_INVOKABLE void clickSquare(const QString &square);
    Q_INVOKABLE void tryMove(const QString &from, const QString &to);
    Q_INVOKABLE void move(const QString &uci);
    Q_INVOKABLE void navigate(int node);
    Q_INVOKABLE void step(int direction);
    Q_INVOKABLE void flip();
    Q_INVOKABLE void setComment(const QString &comment);
    Q_INVOKABLE void setNag(int nag);
    Q_INVOKABLE void setTags(const QVariantMap &tags);
    Q_INVOKABLE void promote();
    Q_INVOKABLE void deleteVariation();
    Q_INVOKABLE void search(const QString &query, const QString &result, bool favorite, bool position,
                            bool trash);
    Q_INVOKABLE void favorite(qint64 id, bool value);
    Q_INVOKABLE void trash(qint64 id, bool value);
    Q_INVOKABLE void setCollection(qint64 id, const QString &collection);
    Q_INVOKABLE void copyText(const QString &what);
    Q_INVOKABLE void setPreference(const QString &key, const QVariant &value);
    Q_INVOKABLE void startEngine();
    Q_INVOKABLE void toggleAnalysis();
    Q_INVOKABLE void addEngineLine(int index);
    Q_INVOKABLE void playComputer(bool humanWhite);
    Q_INVOKABLE void stopPlaying();
    Q_INVOKABLE void createCard(const QString &title);
    Q_INVOKABLE void startCard(qint64 id);
    Q_INVOKABLE void revealCard();
    Q_INVOKABLE void finishTraining();
    Q_INVOKABLE void deleteCard(qint64 id);
    Q_INVOKABLE void loadExamples();
    Q_INVOKABLE void verifyDatabase();
    Q_INVOKABLE void runPerft(int depth);
    Q_INVOKABLE void openDataFolder();
  signals:
    void changed();
    void libraryChanged();
    void engineChanged();
    void preferencesChanged();
    void busyChanged();
    void message(const QString &text, bool error = false);
    void promotionNeeded(const QString &prefix);
    void showAnalysis();

  private:
    struct ImportResult {
        int count = 0, duplicates = 0;
        QString error;
    };
    QString dataPath_, sessionPath_;
    std::unique_ptr<t64::Database> db_;
    t64::Game game_, beforeTraining_;
    int node_ = 0, selected_ = -1, beforeNode_ = 0;
    qint64 gameId_ = 0, beforeId_ = 0, cardId_ = 0;
    QStringList solution_;
    int solutionIndex_ = 0;
    bool trainingFailed_ = false, trainingComplete_ = false, beforeDirty_ = false;
    bool flipped_ = false, dirty_ = false, busy_ = false, analysis_ = false, playing_ = false,
         humanWhite_ = true;
    QString query_, resultFilter_;
    bool favoritesOnly_ = false, positionFilter_ = false, trashFilter_ = false;
    QVariantMap state_, stats_;
    QVariantList squares_, notation_, games_, openings_, cards_, engineLines_;
    QString engineStatus_ = "Moteur déconnecté";
    QSettings settings_;
    QTimer autosave_, analysisTimer_;
    t64::Uci engine_;
    QFutureWatcher<ImportResult> importer_;
    void refresh(bool changedContent = false);
    void refreshLibrary();
    void checkpoint();
    void writeSession();
    void replaceGame(t64::Game game, qint64 id = 0);
    void importBytes(const QByteArray &bytes);
    void requestEngine();
    QString positionCommand() const;
    QString localPath(const QString &url) const;
    void fail(const std::exception &e);
};
