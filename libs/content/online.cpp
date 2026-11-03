// SPDX-License-Identifier: AGPL-3.0-or-later
#include "online.h"
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrlQuery>

namespace t64 {
namespace {
constexpr qint64 maxBytes = 256 * 1024 * 1024;
const QByteArray userAgent = "Trait64 (https://github.com/SimzyGit/Trait64)";
} // namespace

OnlineGames::OnlineGames(QObject *parent, QNetworkAccessManager *network)
    : QObject(parent), network_(network ? network : new QNetworkAccessManager(this)) {}

bool OnlineGames::validUser(const QString &user) {
    static const QRegularExpression pattern("^[A-Za-z0-9_-]{2,30}$");
    return pattern.match(user).hasMatch();
}

QByteArray OnlineGames::lastGames(const QByteArray &pgn, int max, int *count) {
    QList<qsizetype> starts;
    for (qsizetype at = 0; (at = pgn.indexOf("[Event ", at)) >= 0; at += 7)
        if (at == 0 || pgn[at - 1] == '\n') starts << at;
    if (count) *count = int(std::min<qsizetype>(starts.size(), max));
    if (starts.size() <= max) return pgn;
    return pgn.mid(starts[starts.size() - max]);
}

void OnlineGames::fetch(const QString &site, const QString &user, int max, const QString &destination,
                        qint64 sinceMs) {
    if (busy()) return fail(tr("Un téléchargement est déjà en cours."));
    if (!validUser(user)) return fail(tr("Pseudo invalide : lettres, chiffres, - et _ uniquement."));
    user_ = user;
    max_ = std::clamp(max, 1, 5000);
    destination_ = destination;
    since_ = sinceMs;
    collected_.clear();
    archives_.clear();
    if (site == "lichess") {
        QUrl url("https://lichess.org/api/games/user/" + user);
        QUrlQuery query{{"max", QString::number(max_)}, {"clocks", "true"}, {"evals", "true"}};
        if (sinceMs > 0) query.addQueryItem("since", QString::number(sinceMs));
        url.setQuery(query);
        get(url, "application/x-chess-pgn", [this](const QByteArray &body) { finish(body); });
    } else if (site == "chesscom") {
        // Chess.com publishes one archive per month; read the newest ones until `max` games are collected.
        get(QUrl("https://api.chess.com/pub/player/" + user.toLower() + "/games/archives"), "application/json",
            [this](const QByteArray &body) {
                const auto since = QDateTime::fromMSecsSinceEpoch(since_).toUTC().date();
                for (const auto &value : QJsonDocument::fromJson(body).object()["archives"].toArray()) {
                    const auto url = value.toString();
                    // ".../games/2026/10": skip months that ended before the last sync.
                    const int year = url.section('/', -2, -2).toInt(), month = url.section('/', -1).toInt();
                    if (since_ <= 0 || year > since.year() || (year == since.year() && month >= since.month()))
                        archives_ << url;
                }
                if (archives_.isEmpty()) {
                    if (since_ > 0) return emit upToDate();
                    return fail(tr("Aucune partie publique pour ce joueur."));
                }
                nextArchive();
            });
    } else
        fail(tr("Site inconnu."));
}

void OnlineGames::nextArchive() {
    int count = 0;
    lastGames(collected_, max_, &count);
    if (archives_.isEmpty() || count >= max_) return finish(collected_);
    const QUrl url(archives_.takeLast() + "/pgn");
    if (url.scheme() != "https" || url.host() != "api.chess.com") return fail(tr("Réponse de Chess.com inattendue."));
    get(url, "application/x-chess-pgn", [this](const QByteArray &body) {
        collected_.prepend(body.trimmed() + "\n\n");
        nextArchive();
    });
}

void OnlineGames::get(const QUrl &url, const char *accept, std::function<void(const QByteArray &)> done) {
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", userAgent);
    request.setRawHeader("Accept", accept);
    request.setTransferTimeout(60000);
    reply_ = network_->get(request);
    connect(reply_, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64) {
        if (received > maxBytes && reply_) reply_->abort();
    });
    connect(reply_, &QNetworkReply::finished, this, [this, reply = reply_.data(), done] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto body = reply->readAll();
        const bool canceled = reply->error() == QNetworkReply::OperationCanceledError;
        reply_.clear();
        if (canceled) return fail(tr("Téléchargement annulé."));
        if (status == 404) return fail(tr("Joueur introuvable : ") + user_);
        if (status == 429) return fail(tr("Trop de requêtes : réessayez dans une minute."));
        if (reply->error() != QNetworkReply::NoError || status != 200)
            return fail(tr("Téléchargement impossible : ") + reply->errorString());
        done(body);
    });
}

void OnlineGames::finish(const QByteArray &pgn) {
    int count = 0;
    const auto games = lastGames(pgn.trimmed(), max_, &count);
    if (count == 0) {
        if (since_ > 0) return emit upToDate();
        return fail(tr("Aucune partie publique pour ce joueur."));
    }
    QSaveFile file(destination_);
    if (!file.open(QIODevice::WriteOnly) || file.write(games + "\n") < 0 || !file.commit())
        return fail(tr("Écriture impossible : ") + destination_);
    emit finished(destination_, count);
}

void OnlineGames::fail(const QString &message) {
    archives_.clear();
    collected_.clear();
    emit failed(message);
}

void OnlineGames::cancel() {
    archives_.clear();
    if (reply_) reply_->abort();
}
} // namespace t64
