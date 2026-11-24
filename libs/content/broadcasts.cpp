// SPDX-License-Identifier: AGPL-3.0-or-later
#include "broadcasts.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>

namespace t64 {
namespace {
const QString site("https://lichess.org");

// Un identifiant ou un « slug » de Lichess : on ne met dans une adresse que ce qu'il
// est permis d'y trouver.
bool plain(const QString &text) {
    static const QRegularExpression allowed("^[A-Za-z0-9_-]{1,120}$");
    return allowed.match(text).hasMatch();
}

QVariantMap entry(const QJsonObject &item) {
    const auto tour = item["tour"].toObject(), round = item["round"].toObject();
    return {{"tour", tour["name"].toString()},
            {"tourSlug", tour["slug"].toString()},
            {"tier", tour["tier"].toInt()},
            {"round", round["name"].toString()},
            {"roundSlug", round["slug"].toString()},
            {"roundId", round["id"].toString()},
            {"ongoing", round["ongoing"].toBool()},
            {"startsAt", round["startsAt"].toVariant()},
            {"finished", round["finished"].toBool() || round.contains("finishedAt")}};
}

QVariantMap player(const QJsonObject &p) {
    return {{"name", p["name"].toString()},
            {"title", p["title"].toString()},
            {"rating", p["rating"].toInt()},
            {"fed", p["fed"].toString()},
            // Centièmes de seconde chez Lichess : on garde des secondes.
            {"clock", p.contains("clock") ? QVariant(p["clock"].toInt() / 100) : QVariant()}};
}
} // namespace

Broadcasts::Broadcasts(QObject *parent, QNetworkAccessManager *network)
    : QObject(parent), network_(network ? network : new QNetworkAccessManager(this)) {}

QVariantList Broadcasts::parseTop(const QByteArray &json) {
    const auto root = QJsonDocument::fromJson(json).object();
    QVariantList found;
    for (const auto &item : root["active"].toArray())
        found << entry(item.toObject());
    for (const auto &item : root["past"].toObject()["currentPageResults"].toArray())
        found << entry(item.toObject());
    QVariantList usable;
    for (const auto &item : found)
        if (plain(item.toMap()["roundId"].toString()) && plain(item.toMap()["tourSlug"].toString()) &&
            plain(item.toMap()["roundSlug"].toString()))
            usable << item;
    return usable;
}

QVariantMap Broadcasts::parseRound(const QByteArray &json) {
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return {{"error", "Réponse de Lichess illisible."}};
    const auto root = document.object();
    QVariantList games;
    for (const auto &value : root["games"].toArray()) {
        const auto game = value.toObject();
        const auto players = game["players"].toArray();
        auto status = game["status"].toString("*");
        if (status == "½-½")
            status = "1/2-1/2";
        games << QVariantMap{{"id", game["id"].toString()},
                             {"name", game["name"].toString()},
                             {"fen", game["fen"].toString()},
                             {"lastMove", game["lastMove"].toString()},
                             {"status", status},
                             {"white", players.size() > 0 ? player(players[0].toObject()) : QVariantMap()},
                             {"black", players.size() > 1 ? player(players[1].toObject()) : QVariantMap()}};
    }
    return {{"tour", root["tour"].toObject()["name"].toString()},
            {"round", root["round"].toObject()["name"].toString()},
            {"roundId", root["round"].toObject()["id"].toString()},
            {"ongoing", root["round"].toObject()["ongoing"].toBool()},
            {"games", games}};
}

void Broadcasts::get(const QString &url, const QByteArray &accept,
                     const std::function<void(const QByteArray &)> &done) {
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(20000);
    request.setRawHeader("Accept", accept);
    request.setRawHeader("User-Agent", "Trait64-broadcasts/1.0");
    auto *reply = network_->get(request);
    replies_ << reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, done] {
        replies_.removeAll(reply);
        reply->deleteLater();
        if (reply->error() == QNetworkReply::OperationCanceledError)
            return;
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 429) {
            emit failed("Lichess demande de ralentir. Le suivi reprendra dans un instant.");
            return;
        }
        if (reply->error() != QNetworkReply::NoError || status != 200) {
            emit failed("Retransmissions Lichess injoignables : " + reply->errorString());
            return;
        }
        done(reply->readAll());
    });
}

void Broadcasts::list() {
    get(site + "/api/broadcast/top?page=1", "application/json",
        [this](const QByteArray &body) { emit listed(parseTop(body)); });
}

void Broadcasts::round(const QString &tour, const QString &roundSlug, const QString &roundId) {
    if (!plain(tour) || !plain(roundSlug) || !plain(roundId)) {
        emit failed("Ronde inconnue.");
        return;
    }
    get(site + "/api/broadcast/" + tour + '/' + roundSlug + '/' + roundId, "application/json",
        [this](const QByteArray &body) {
            const auto round = parseRound(body);
            if (round.contains("error"))
                emit failed(round["error"].toString());
            else
                emit roundReady(round);
        });
}

void Broadcasts::pgn(const QString &roundId) {
    if (!plain(roundId)) {
        emit failed("Ronde inconnue.");
        return;
    }
    get(site + "/api/broadcast/round/" + roundId + ".pgn?clocks=true", "application/x-chess-pgn",
        [this, roundId](const QByteArray &body) { emit pgnReady(roundId, body); });
}

void Broadcasts::search(const QString &query, int page) {
    QUrl url(site + "/api/broadcast/search");
    QUrlQuery q;
    q.addQueryItem("q", query.left(80));
    q.addQueryItem("page", QString::number(std::clamp(page, 1, 20)));
    url.setQuery(q);
    get(url.toString(QUrl::FullyEncoded), "application/json", [this, query, page](const QByteArray &body) {
        bool more = false;
        const auto tours = parseSearch(body, &more);
        emit searched(query, page, tours, more);
    });
}

void Broadcasts::tourPgn(const QString &tourId) {
    if (!plain(tourId)) {
        emit failed("Tournoi inconnu.");
        return;
    }
    get(site + "/api/broadcast/" + tourId + ".pgn", "application/x-chess-pgn",
        [this, tourId](const QByteArray &body) { emit tourPgnReady(tourId, body); });
}

QVariantList Broadcasts::parseSearch(const QByteArray &json, bool *more) {
    const auto root = QJsonDocument::fromJson(json).object();
    if (more)
        *more = root["nextPage"].isDouble();
    QVariantList tours;
    for (const auto &value : root["currentPageResults"].toArray()) {
        const auto item = value.toObject();
        // Un résultat est un tournoi avec sa dernière ronde, ou le tournoi seul.
        const auto tour = item.contains("tour") ? item["tour"].toObject() : item;
        const auto round = item["round"].toObject();
        const auto info = tour["info"].toObject();
        const auto id = tour["id"].toString();
        if (!plain(id))
            continue;
        tours << QVariantMap{{"id", id},
                             {"name", tour["name"].toString()},
                             {"slug", tour["slug"].toString()},
                             {"location", info["location"].toString()},
                             {"website", info["website"].toString()},
                             {"finished", round["finished"].toBool() || round.contains("finishedAt") ||
                                              tour.contains("finishedAt")}};
    }
    return tours;
}

bool Broadcasts::isFrench(const QVariantMap &tour) {
    const auto website = tour["website"].toString();
    if (website.contains("echecs.asso.fr", Qt::CaseInsensitive) || website.contains("ffechecs", Qt::CaseInsensitive))
        return true;
    static const QRegularExpression name(
        R"(\b(FFE|Championnats? de France|Coupe de France|Top ?1[26]|Top Jeunes|Nationale ?[1-4])\b)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression place(R"((,\s*|\b)(France|FRA)\s*$)", QRegularExpression::CaseInsensitiveOption);
    return name.match(tour["name"].toString()).hasMatch() || place.match(tour["location"].toString()).hasMatch();
}

void Broadcasts::cancel() {
    for (const auto &reply : replies_)
        if (reply)
            reply->abort();
    replies_.clear();
}
} // namespace t64
