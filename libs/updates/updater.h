// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "releases.h"
#include <QNetworkAccessManager>
#include <QPointer>
#include <QProcess>
#include <QSaveFile>
#include <QSettings>
#include <QVariantMap>
#include <functional>
#include <memory>
namespace t64 {
class Updater : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
  public:
    // L'écart entre deux vérifications automatiques pendant que le logiciel tourne.
    // Six heures : assez pour apprendre une version le jour de sa sortie, assez peu
    // pour ne rien peser sur le quota de l'API GitHub (soixante appels par heure).
    static constexpr int recheckInterval = 6 * 60 * 60 * 1000;
    explicit Updater(const QString &dataPath, QObject *parent = nullptr,
                     QNetworkAccessManager *network = nullptr);
    ~Updater() override;
    QVariantMap state() const;
    std::function<QString()> beforeInstall;
    // Vérifie peu après le démarrage, puis à intervalles tant que le logiciel tourne.
    // Les délais ne sont des paramètres que pour s'éprouver sans attendre six heures.
    void startAutomaticCheck(int firstDelay = 3500, int interval = recheckInterval);
    Q_INVOKABLE void check(bool manual = true);
    Q_INVOKABLE void setChannel(const QString &channel);
    Q_INVOKABLE void setAutomatic(bool enabled);
    Q_INVOKABLE void useGithubCli(bool enabled);
    Q_INVOKABLE void setSessionToken(const QString &token);
    Q_INVOKABLE void later();
    Q_INVOKABLE void ignore();
    Q_INVOKABLE void downloadAndInstall();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void restart();
  signals:
    void changed();
    void offered();

  private:
    QSettings settings_;
    QNetworkAccessManager *network_;
    QPointer<QNetworkReply> reply_;
    QProcess auth_, installer_;
    QString dataPath_, status_ = "idle", message_, token_, suffix_, kind_, target_, downloadPath_, backup_;
    Release candidate_;
    QJsonArray releases_;
    std::unique_ptr<QSaveFile> download_;
    qint64 received_ = 0;
    bool manual_ = false;
    int epoch_ = 0, page_ = 1;
    QString channel() const;
    bool busy() const;
    void fail(const QString &message);
    void fetchPage();
    void request(const QUrl &url, bool binary, int redirects = 0);
    void fetched(const QByteArray &body);
    void install();
};
} // namespace t64
