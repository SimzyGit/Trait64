// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "game.h"
#include <QSqlDatabase>
#include <QVariantList>
#include <QVariantMap>

namespace t64 {
class Database {
  public:
    explicit Database(const QString &path);
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;
    qint64 save(const Game &game, qint64 id = 0, const QString &collection = {});
    int importGames(const std::vector<Game> &games);
    Game load(qint64 id) const;
    QVariantList search(const QString &query = {}, const QString &result = {}, bool favorite = false,
                        const QString &position = {}, bool trash = false) const;
    QVariantMap stats() const;
    QVariantList explorer(const QString &position) const;
    void favorite(qint64 id, bool value);
    void trash(qint64 id, bool value);
    void collection(qint64 id, const QString &value);
    QString exportPgn() const;
    void backup(const QString &path);
    QString integrity() const;
    void addCard(const QString &title, const QString &fen, const QString &moves);
    QVariantList cards() const;
    void review(qint64 id, bool success);
    void deleteCard(qint64 id);
    QString path() const { return path_; }

  private:
    QString connection_, path_;
    QSqlDatabase db_;
    void sql(const QString &query) const;
};
} // namespace t64
