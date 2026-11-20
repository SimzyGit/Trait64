// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>

class QNetworkAccessManager;
#include <QTcpServer>

namespace t64 {
// Connexion à Lichess comme dans En Croissant : Lichess s'ouvre dans le navigateur, on y
// autorise Trait64, et Lichess renvoie vers un petit serveur local qui reçoit le code.
// Le code s'échange contre un jeton (OAuth2 avec PKCE, sans secret), puis le jeton dit à
// quel compte il appartient. Aucune autorisation n'est demandée : le jeton suffit à
// l'explorateur d'ouvertures et accélère le téléchargement des parties du compte.
class LichessLogin : public QObject {
    Q_OBJECT
  public:
    explicit LichessLogin(QObject *parent = nullptr, QNetworkAccessManager *network = nullptr);
    // Lance la connexion ; authorizeUrl() dit l'adresse à ouvrir dans le navigateur.
    bool start();
    void cancel();
    bool running() const { return server_ != nullptr; }
    QUrl authorizeUrl() const { return authorize_; }
    // Pour les essais : l'adresse de Lichess, remplacée par un serveur local.
    void setBase(const QString &base) { base_ = base; }
    // Le défi PKCE (S256) d'un vérificateur, en base64url sans remplissage.
    static QString challenge(const QString &verifier);

  signals:
    void connected(const QString &user, const QString &token);
    void failed(const QString &message);

  private:
    void receive();
    void exchange(const QString &code);
    void identify(const QString &token);
    void stop();
    void fail(const QString &message);
    QNetworkAccessManager *network_;
    QPointer<QTcpServer> server_;
    QString base_ = QStringLiteral("https://lichess.org"), verifier_, state_, redirect_;
    QUrl authorize_;
    QTimer timeout_;
};
} // namespace t64
