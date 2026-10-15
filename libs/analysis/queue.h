// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QObject>
#include <QThread>
#include <QVariantList>
#include <QVariantMap>

namespace t64 {
class AnalysisWorker;
class AnalysisQueue : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList jobs READ jobs NOTIFY changed)
    Q_PROPERTY(QVariantList results READ results NOTIFY changed)
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
  public:
    explicit AnalysisQueue(QObject *parent = nullptr);
    ~AnalysisQueue() override;
    void setLibrary(const QString &path);
    void shutdown();
    void enqueue(const QString &pgn, const QVariantMap &profile, bool variations);
    QVariantList jobs() const { return jobs_; }
    QVariantList results() const { return results_; }
    QVariantMap state() const { return state_; }
    Q_INVOKABLE void select(qint64 id, int offset = 0);
    Q_INVOKABLE void pause(qint64 id);
    Q_INVOKABLE void resume(qint64 id);
    Q_INVOKABLE void pauseAll();
    Q_INVOKABLE void forget(qint64 id);
    Q_INVOKABLE void clearCache();
    Q_INVOKABLE void exportPgn(qint64 id, const QString &url, bool includeVariations = true);
    Q_INVOKABLE void openStudy(qint64 id);
  signals:
    void changed();
    void message(const QString &text, bool error = false);
    void studyReady(const QString &pgn);

  private:
    AnalysisWorker *worker_ = nullptr;
    QThread thread_;
    quint64 generation_ = 0;
    QVariantList jobs_, results_;
    QVariantMap state_{
        {"ready", false}, {"running", false}, {"selected", 0}, {"page", 1}, {"hasNext", false}};
};
} // namespace t64
