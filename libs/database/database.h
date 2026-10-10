// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "game.h"
#include <QIODevice>
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
    int importGames(const std::vector<Game> &games, const std::function<bool(int)> &progress = {});
    int importPgnFile(const QString &path, const std::function<bool(int)> &progress);
    Game load(qint64 id) const;
    QVariantList search(const QString &query = {}, const QString &result = {}, bool favorite = false,
                        const QString &position = {}, bool trash = false,
                        const QVariantMap &filters = {}) const;
    QVariantMap stats() const;
    QVariantList explorer(const QString &position) const;
    void favorite(qint64 id, bool value);
    void trash(qint64 id, bool value);
    void collection(qint64 id, const QString &value);
    QString exportPgn() const;
    void exportPgn(QIODevice &output) const;
    void validateOutputPath(const QString &path) const;
    void backup(const QString &path);
    QString integrity() const;
    void addCard(const QString &title, const QString &fen, const QString &moves);
    QVariantList cards() const;
    void review(qint64 id, bool success);
    void deleteCard(qint64 id);
    QVariantList players() const;
    QVariantMap player(const QString &name) const;
    QVariantList duplicates() const;
    int rebuildIndexes(const std::function<bool(int)> &progress = {});
    QString path() const { return path_; }

  private:
    bool enhanced_ = false, fts_ = false;
    void upgradeV3(int originalVersion);
    QString connection_, path_;
    QSqlDatabase db_;
    void sql(const QString &query) const;
};
} // namespace t64
