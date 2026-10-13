// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QElapsedTimer>
#include <QFile>
#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QTemporaryDir>
#include <QVariantList>
#include <atomic>
#include <memory>

namespace t64 {
class Content : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList catalog READ catalog NOTIFY catalogChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString activeId READ activeId NOTIFY changed)
    Q_PROPERTY(qint64 received READ received NOTIFY changed)
    Q_PROPERTY(qint64 total READ total NOTIFY changed)
  public:
    explicit Content(const QString &root, QObject *parent = nullptr);
    ~Content() override;
    QVariantList catalog() const;
    bool busy() const { return busy_; }
    QString status() const { return status_; }
    QString activeId() const { return active_.value("id").toString(); }
    qint64 received() const { return received_; }
    qint64 total() const { return total_; }
    Q_INVOKABLE void install(const QString &id);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void useInstalled(const QString &id);
    Q_INVOKABLE void remove(const QString &id);
    static bool allowedUrl(const QUrl &url, bool engine);
    static bool compatible(const QVariantMap &entry);
    void reportImport(const QString &text) {
        status_ = text;
        emit changed();
    }
    // Also used by offline package-validation tests. Never runs downloaded code.
    static QVariantMap installPackage(const QVariantMap &entry, const QString &file, const QString &root,
                                      const std::shared_ptr<std::atomic_bool> &cancel);
  signals:
    void changed();
    void catalogChanged();
    void ready(const QString &path, const QVariantMap &entry);
    void aboutToRemove(const QString &path);

  private:
    QString root_, status_;
    QVariantList entries_;
    QVariantMap active_;
    bool busy_ = false;
    qint64 received_ = 0, total_ = 0;
    int redirects_ = 0;
    QElapsedTimer progressTimer_;
    QNetworkAccessManager network_;
    QPointer<QNetworkReply> reply_;
    std::unique_ptr<QTemporaryDir> temporary_;
    std::unique_ptr<QFile> file_;
    std::shared_ptr<std::atomic_bool> canceled_;
    QFutureWatcher<QVariantMap> worker_;
    QVariantMap entry(const QString &id) const;
    QVariantMap receipt(const QString &id) const;
    QString receiptPath(const QString &id) const;
    void request(const QUrl &url);
    void fail(const QString &reason);
};
} // namespace t64
