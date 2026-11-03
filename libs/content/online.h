// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace t64 {
// Downloads a player's public games from Lichess or Chess.com as one PGN file.
// Only public APIs are used: no account, no token.
class OnlineGames : public QObject {
    Q_OBJECT
  public:
    explicit OnlineGames(QObject *parent = nullptr, QNetworkAccessManager *network = nullptr);
    static bool validUser(const QString &user);
    bool busy() const { return !reply_.isNull(); }
    // site: "lichess" or "chesscom"; sinceMs > 0 keeps only newer Lichess games.
    void fetch(const QString &site, const QString &user, int max, const QString &destination, qint64 sinceMs = 0);
    void cancel();
    // Keeps the last `max` games of a PGN text (games start with an [Event tag).
    static QByteArray lastGames(const QByteArray &pgn, int max, int *count = nullptr);

  signals:
    void finished(const QString &path, int games);
    void failed(const QString &message);

  private:
    void get(const QUrl &url, const char *accept, std::function<void(const QByteArray &)> done);
    void nextArchive();
    void finish(const QByteArray &pgn);
    void fail(const QString &message);
    QNetworkAccessManager *network_;
    QPointer<QNetworkReply> reply_;
    QString user_, destination_;
    int max_ = 0;
    QStringList archives_;
    QByteArray collected_;
};
} // namespace t64
