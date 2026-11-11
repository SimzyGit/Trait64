// SPDX-License-Identifier: AGPL-3.0-or-later
#include "explorer.h"
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>

namespace t64 {
namespace {
// Les vitesses et les tranches de classement que l'explorateur accepte. Une valeur
// inconnue n'est pas transmise : mieux vaut une réponse sans filtre qu'un refus du
// serveur sur un mot mal orthographié.
const QStringList &speeds() {
    static const QStringList all{"ultraBullet", "bullet", "blitz", "rapid", "classical", "correspondence"};
    return all;
}
const QStringList &ratings() {
    static const QStringList all{"0", "1000", "1200", "1400", "1600", "1800", "2000", "2200", "2500"};
    return all;
}
QString keep(const QVariant &value, const QStringList &allowed) {
    QStringList chosen;
    for (const auto &one : value.toStringList())
        if (allowed.contains(one) && !chosen.contains(one))
            chosen << one;
    return chosen.join(',');
}
// « 1-0 », « 0-1 » ou « 1/2-1/2 » : la notation des parties, pas celle de l'API.
QString resultOf(const QJsonObject &game) {
    const auto winner = game["winner"].toString();
    return winner == "white" ? "1-0" : winner == "black" ? "0-1" : "1/2-1/2";
}
QVariantMap gameOf(const QJsonObject &game) {
    const auto white = game["white"].toObject(), black = game["black"].toObject();
    auto date = QString::number(game["year"].toInt());
    if (const auto month = game["month"]; month.isDouble())
        date += '.' + QString::number(month.toInt()).rightJustified(2, '0');
    else if (const auto text = game["month"].toString(); !text.isEmpty())
        date = text; // Lichess note parfois le mois « 2019-07 », année comprise.
    return {{"id", game["id"].toString()},
            {"white", white["name"].toString()},
            {"black", black["name"].toString()},
            {"whiteElo", white["rating"].toInt()},
            {"blackElo", black["rating"].toInt()},
            {"result", resultOf(game)},
            {"date", date}};
}
} // namespace

OpeningExplorer::OpeningExplorer(QObject *parent, QNetworkAccessManager *network)
    : QObject(parent), network_(network ? network : new QNetworkAccessManager(this)) {}

bool OpeningExplorer::knownSource(const QString &source) {
    return source == "lichess" || source == "masters" || source == "tablebase";
}

int OpeningExplorer::pieces(const QString &fen) {
    int count = 0;
    for (const auto c : fen.section(' ', 0, 0))
        count += c.isLetter() ? 1 : 0;
    return count;
}

QVariantMap OpeningExplorer::parseTablebase(const QByteArray &body) {
    QJsonParseError error{};
    const auto document = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return {{"error", "Réponse illisible des tables de finales Lichess."}};
    const auto root = document.object();
    const auto verdict = root["category"].toString();
    // Le verdict d'un coup est donné pour le camp qui jouera ensuite : « loss » après
    // mon coup veut dire que je gagne. On le retourne — sauf si la réponse elle-même
    // dit le contraire : les coups arrivent du meilleur au pire, donc dans une position
    // gagnante le premier doit être gagnant d'une façon ou de l'autre.
    const auto moves = root["moves"].toArray();
    const auto first = moves.isEmpty() ? QString() : moves.first().toObject()["category"].toString();
    const bool flip = !(verdict == "win" && first == "win") && !(verdict == "loss" && first == "loss");
    const auto mine = [flip](const QString &category) {
        if (!flip)
            return category;
        static const QHash<QString, QString> mirror{{"win", "loss"},
                                                    {"loss", "win"},
                                                    {"cursed-win", "blessed-loss"},
                                                    {"blessed-loss", "cursed-win"},
                                                    {"maybe-win", "maybe-loss"},
                                                    {"maybe-loss", "maybe-win"}};
        return mirror.value(category, category);
    };
    const auto number = [](const QJsonValue &value) {
        return value.isDouble() ? QVariant(qAbs(value.toInt())) : QVariant();
    };
    QVariantList rows;
    for (const auto &value : moves) {
        const auto move = value.toObject();
        rows << QVariantMap{{"san", move["san"].toString()},
                            {"uci", move["uci"].toString()},
                            {"verdict", mine(move["category"].toString())},
                            {"dtz", number(move["dtz"])},
                            {"dtm", number(move["dtm"])}};
    }
    return {{"tablebase", true},          {"verdict", verdict}, {"dtz", number(root["dtz"])},
            {"dtm", number(root["dtm"])}, {"moves", rows},      {"total", rows.size()}};
}

QUrl OpeningExplorer::buildUrl(const QString &source, const QString &fen, const QVariantMap &filters) {
    if (!knownSource(source) || fen.isEmpty())
        return {};
    if (source == "tablebase") {
        // Sept pièces au plus : au-delà, les tables n'existent pas et la question ne
        // part pas.
        if (pieces(fen) > 7)
            return {};
        QUrl tables("https://tablebase.lichess.ovh/standard");
        tables.setQuery(QUrlQuery{{"fen", fen}});
        return tables;
    }
    QUrl url("https://explorer.lichess.ovh/" + source);
    QUrlQuery query;
    query.addQueryItem("fen", fen);
    query.addQueryItem("moves", "12");
    query.addQueryItem("topGames", source == "masters" ? "8" : "4");
    if (source == "lichess") {
        query.addQueryItem("recentGames", "0");
        if (const auto chosen = keep(filters["speeds"], speeds()); !chosen.isEmpty())
            query.addQueryItem("speeds", chosen);
        if (const auto chosen = keep(filters["ratings"], ratings()); !chosen.isEmpty())
            query.addQueryItem("ratings", chosen);
    }
    // « 2019-01 » pour le site, « 1952 » pour l'échiquier : l'API ne compte pas le temps
    // de la même façon selon la base, et un format pour l'autre fait une erreur 400.
    static const QRegularExpression month("^[0-9]{4}-[0-9]{2}$"), year("^[0-9]{4}$");
    const auto &shape = source == "masters" ? year : month;
    for (const auto *key : {"since", "until"})
        if (const auto value = filters[key].toString(); !value.isEmpty() && shape.match(value).hasMatch())
            query.addQueryItem(key, value);
    url.setQuery(query);
    return url;
}

QVariantMap OpeningExplorer::parse(const QByteArray &body) {
    QJsonParseError error{};
    const auto document = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return {{"error", "Réponse illisible de l’explorateur Lichess."}};
    const auto root = document.object();
    const auto white = root["white"].toInteger(), draws = root["draws"].toInteger(),
               black = root["black"].toInteger();
    QVariantMap report{
        {"total", white + draws + black}, {"whiteWins", white}, {"draws", draws}, {"blackWins", black}};
    const auto opening = root["opening"].toObject();
    if (const auto name = opening["name"].toString(); !name.isEmpty())
        report["name"] = opening["eco"].toString().isEmpty() ? name : opening["eco"].toString() + ' ' + name;
    QVariantList moves;
    for (const auto &value : root["moves"].toArray()) {
        const auto move = value.toObject();
        const auto w = move["white"].toInteger(), d = move["draws"].toInteger(),
                   b = move["black"].toInteger();
        moves << QVariantMap{{"san", move["san"].toString()},
                             {"uci", move["uci"].toString()},
                             {"count", w + d + b},
                             {"white", w},
                             {"draw", d},
                             {"black", b},
                             // Absent sur l'échiquier, où le classement moyen n'a pas
                             // le même sens qu'entre deux comptes du site.
                             {"rating", move["averageRating"].toInt()}};
    }
    report["moves"] = moves;
    QVariantList games;
    for (const auto &value : root["topGames"].toArray())
        games << gameOf(value.toObject());
    report["games"] = games;
    return report;
}

void OpeningExplorer::query(const QString &source, const QString &fen, const QVariantMap &filters) {
    cancel();
    const auto url = buildUrl(source, fen, filters);
    if (url.isEmpty()) {
        emit failed(source == "tablebase" && pieces(fen) > 7
                        ? "Les tables de finales couvrent les positions de sept pièces au plus."
                        : "Base en ligne inconnue.");
        return;
    }
    const bool tables = source == "tablebase";
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(20000);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "Trait64-explorer/1.0");
    reply_ = network_->get(request);
    connect(reply_, &QNetworkReply::finished, this, [this, tables] {
        if (!reply_)
            return;
        std::unique_ptr<QNetworkReply> reply(reply_.data());
        reply_.clear();
        reply->deleteLater();
        if (reply->error() == QNetworkReply::OperationCanceledError)
            return;
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // 429 : l'explorateur limite les appels, et le dire vaut mieux qu'un échec muet.
        if (status == 429) {
            emit failed("L’explorateur Lichess demande de ralentir. Réessayez dans un instant.");
            return;
        }
        if (reply->error() != QNetworkReply::NoError || status != 200) {
            emit failed("Explorateur Lichess injoignable : " + reply->errorString());
            return;
        }
        auto report = tables ? parseTablebase(reply->readAll()) : parse(reply->readAll());
        if (report.contains("error")) {
            emit failed(report["error"].toString());
            return;
        }
        emit ready(report);
    });
}

void OpeningExplorer::cancel() {
    if (auto *reply = reply_.data()) {
        reply_.clear();
        reply->abort();
        reply->deleteLater();
    }
}
} // namespace t64
