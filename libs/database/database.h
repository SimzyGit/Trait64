// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "game.h"
#include <QIODevice>
#include <QSqlDatabase>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
#include <memory>

namespace t64 {
using RepertoireProgress = std::function<bool(qint64)>;
struct ImportProgress {
    qint64 job = 0, bytes = 0, total = 0, processed = 0, added = 0, committed = 0, committedBytes = 0;
    bool verifying = true;
};
class Database {
  public:
    explicit Database(const QString &path);
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;
    qint64 save(const Game &game, qint64 id = 0, const QString &collection = {});
    int importGames(const std::vector<Game> &games, const std::function<bool(int)> &progress = {});
    int importPgnFile(const QString &path, const std::function<bool(int)> &progress,
                      const QVariantMap &source = {});
    QVariantList sources() const;
    QVariantList importHistory() const;
    qint64 createImportJob(const QString &path, const QVariantMap &source = {});
    QVariantMap runImportJob(qint64 id, const std::function<bool(const ImportProgress &)> &progress = {},
                             const QString &replacement = {}, int batchSize = 250, int workers = 1);
    QVariantList importJobs() const;
    QVariantMap importJob(qint64 id) const;
    void forgetImportJob(qint64 id);
    qint64 createAnalysisJob(const QString &pgn, const QVariantMap &config, bool variations, int total);
    QVariantList analysisJobs() const;
    QVariantMap analysisJob(qint64 id, bool includePgn = true) const;
    QVariantList analysisResults(qint64 id, int offset = 0, int limit = 200) const;
    void setAnalysisStatus(qint64 id, const QString &status, const QString &error = QStringLiteral(""));
    void setAnalysisIdentity(qint64 id, const QVariantMap &identity);
    void recoverAnalyses();
    void forgetAnalysis(qint64 id);
    void saveAnalysisResult(qint64 id, int ordinal, int node, const QString &fen, const QVariantList &lines,
                            const QString &cacheKey = {}, bool cached = false);
    QVariantList cachedAnalysis(const QString &key) const;
    void clearAnalysisCache();
    qint64 createRepertoire(const QString &name, bool white, const Game &root,
                            const RepertoireProgress &progress = {}, qint64 model = 0);
    QVariantList repertoires() const;
    QVariantMap repertoire(qint64 id) const;
    void renameRepertoire(qint64 id, const QString &name);
    void deleteRepertoire(qint64 id);
    void addToRepertoire(qint64 id, const Game &game, qint64 model = 0,
                         const RepertoireProgress &progress = {});
    int importRepertoireFile(qint64 id, const QString &path, const RepertoireProgress &progress = {});
    QVariantMap repertoireOverview(qint64 id, const std::shared_ptr<std::atomic_bool> &canceled = {}) const;
    void importRepertoire(qint64 id, const std::vector<Game> &games);
    QVariantList repertoireMoves(qint64 id, const QString &fen) const;
    void editRepertoireMove(qint64 id, const QString &fen, const QString &uci, const QString &note,
                            bool principal);
    void removeRepertoireMove(qint64 id, const QString &fen, const QString &uci);
    Game exportRepertoire(qint64 id, const RepertoireProgress &progress = {}) const;
    QVariantMap nextRepertoirePosition(qint64 id, bool includeFuture = false) const;
    void reviewRepertoire(qint64 id, const QString &fen, bool success);
    QVariantList repertoireWeaknesses(qint64 id) const;
    QVariantList repertoireGaps(qint64 id) const;
    void exportSource(QIODevice &output, qint64 source) const;
    Game load(qint64 id) const;
    QVariantList search(const QString &query = {}, const QString &result = {}, bool favorite = false,
                        const QString &position = {}, bool trash = false, const QVariantMap &filters = {},
                        const std::shared_ptr<std::atomic_bool> &canceled = {}) const;
    QVariantMap stats() const;
    QVariantList explorer(const QString &position) const;
    QVariantList positionStatistics(const QString &fen, const QVariantMap &filters = {},
                                    const std::shared_ptr<std::atomic_bool> &canceled = {}) const;
    void favorite(qint64 id, bool value);
    void trash(qint64 id, bool value);
    void collection(qint64 id, const QString &value);
    QString exportPgn() const;
    void exportPgn(QIODevice &output) const;
    void validateOutputPath(const QString &path) const;
    void backup(const QString &path);
    QString integrity(const std::shared_ptr<std::atomic_bool> &canceled = {}) const;
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
    void upgradeV4(int originalVersion);
    void upgradeV5(int originalVersion);
    void upgradeV6(int originalVersion);
    void upgradeV7(int originalVersion);
    qint64 registerSource(const QVariantMap &source);
    void linkSource(qint64 source, qint64 game);
    void recordImport(qint64 source, const QString &file, const QString &digest, int processed, int added);
    QString connection_, path_;
    QSqlDatabase db_;
    void sql(const QString &query) const;
};
} // namespace t64
