// SPDX-License-Identifier: AGPL-3.0-or-later
#include "lichess_login.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>

namespace t64 {
namespace {
constexpr auto clientId = "trait64";
QString randomText(int bytes) {
    QByteArray raw(bytes, Qt::Uninitialized);
    QRandomGenerator::system()->generate(reinterpret_cast<quint32 *>(raw.data()),
                                         reinterpret_cast<quint32 *>(raw.data() + bytes));
    return QString::fromLatin1(raw.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}
} // namespace

LichessLogin::LichessLogin(QObject *parent, QNetworkAccessManager *network)
    : QObject(parent), network_(network ? network : new QNetworkAccessManager(this)) {
    timeout_.setSingleShot(true);
    // Cinq minutes pour autoriser dans le navigateur ; au-delà, le serveur local se ferme.
    timeout_.setInterval(5 * 60 * 1000);
    connect(&timeout_, &QTimer::timeout, this,
            [this] { fail(tr("Connexion à Lichess abandonnée : délai dépassé.")); });
}

QString LichessLogin::challenge(const QString &verifier) {
    return QString::fromLatin1(QCryptographicHash::hash(verifier.toLatin1(), QCryptographicHash::Sha256)
                                   .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

bool LichessLogin::start() {
    stop();
    server_ = new QTcpServer(this);
    // Sur la boucle locale seulement : rien d'autre que ce navigateur ne doit pouvoir
    // répondre à la place de Lichess.
    if (!server_->listen(QHostAddress::LocalHost)) {
        fail(tr("Impossible d’ouvrir le port local de connexion à Lichess."));
        return false;
    }
    connect(server_, &QTcpServer::newConnection, this, &LichessLogin::receive);
    verifier_ = randomText(48);
    state_ = randomText(16);
    redirect_ = QString("http://127.0.0.1:%1/").arg(server_->serverPort());
    authorize_ = QUrl(base_ + "/oauth");
    authorize_.setQuery(QUrlQuery{{"response_type", "code"},
                                  {"client_id", clientId},
                                  {"redirect_uri", redirect_},
                                  {"code_challenge_method", "S256"},
                                  {"code_challenge", challenge(verifier_)},
                                  {"state", state_}});
    timeout_.start();
    return true;
}

void LichessLogin::cancel() {
    stop();
}

void LichessLogin::stop() {
    timeout_.stop();
    if (server_) {
        server_->close();
        server_->deleteLater();
        server_.clear();
    }
}

void LichessLogin::fail(const QString &message) {
    stop();
    emit failed(message);
}

void LichessLogin::receive() {
    if (!server_)
        return;
    auto *socket = server_->nextPendingConnection();
    if (!socket)
        return;
    // Détaché du serveur : fermé dès le code reçu, il emportait le socket avec lui avant
    // que la page de confirmation parte, et le navigateur affichait une erreur.
    socket->setParent(this);
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
        // Seule la ligne de requête compte : « GET /?code=…&state=… HTTP/1.1 ».
        if (!socket->canReadLine() && socket->bytesAvailable() < 8192)
            return;
        const auto line = QString::fromLatin1(socket->readLine(8192)).trimmed();
        const auto parts = line.split(' ');
        const QUrl url(parts.size() >= 2 ? "http://127.0.0.1" + parts[1] : QString());
        const QUrlQuery query(url);
        const bool ours =
            parts.value(0) == "GET" && url.path() == "/" && query.queryItemValue("state") == state_;
        const auto code = query.queryItemValue("code");
        const auto page = ours && !code.isEmpty()
                              ? tr("Trait64 est connecté à Lichess. Vous pouvez fermer cet onglet.")
                              : tr("Connexion refusée ou interrompue. Revenez dans Trait64 pour réessayer.");
        const auto body = ("<!doctype html><meta charset=utf-8><title>Trait64</title>"
                           "<body style=\"font:16px sans-serif;margin:3em\"><p>" +
                           page.toHtmlEscaped() + "</p>")
                              .toUtf8();
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n"
                      "Content-Length: " +
                      QByteArray::number(body.size()) + "\r\n\r\n" + body);
        socket->disconnectFromHost();
        // Une requête étrangère (favicon, autre onglet) ne met pas fin à l'attente.
        if (!ours)
            return;
        if (code.isEmpty()) {
            fail(tr("Connexion à Lichess refusée."));
            return;
        }
        stop();
        exchange(code);
    });
}

void LichessLogin::exchange(const QString &code) {
    QNetworkRequest request(QUrl(base_ + "/api/token"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setTransferTimeout(30000);
    QUrlQuery form{{"grant_type", "authorization_code"},
                   {"code", code},
                   {"code_verifier", verifier_},
                   {"redirect_uri", redirect_},
                   {"client_id", clientId}};
    auto *reply = network_->post(request, form.query(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const auto token = QJsonDocument::fromJson(reply->readAll()).object()["access_token"].toString();
        if (reply->error() != QNetworkReply::NoError || token.isEmpty())
            return fail(tr("Lichess n’a pas délivré de jeton : ") + reply->errorString());
        identify(token);
    });
}

void LichessLogin::identify(const QString &token) {
    QNetworkRequest request(QUrl(base_ + "/api/account"));
    request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    request.setTransferTimeout(30000);
    auto *reply = network_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, token] {
        reply->deleteLater();
        const auto user = QJsonDocument::fromJson(reply->readAll()).object()["username"].toString();
        if (reply->error() != QNetworkReply::NoError || user.isEmpty())
            return fail(tr("Jeton reçu, mais Lichess ne dit pas à quel compte il appartient."));
        emit connected(user, token);
    });
}
} // namespace t64
