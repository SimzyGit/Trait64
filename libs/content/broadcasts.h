// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QObject>
#include <QPointer>
#include <QVariantMap>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace t64 {
// Les retransmissions de Lichess : la liste des tournois diffusés, les échiquiers d'une
// ronde (joueurs, position, pendules, résultat) et le PGN de la ronde. API publique,
// sans compte ; rien n'est demandé tant que l'on n'ouvre pas le panneau.
class Broadcasts : public QObject {
    Q_OBJECT
  public:
    explicit Broadcasts(QObject *parent = nullptr, QNetworkAccessManager *network = nullptr);
    // Les tournois en cours d'abord, puis les derniers terminés.
    void list();
    // Les échiquiers d'une ronde ; `tour` et `round` sont les « slugs » de l'adresse.
    void round(const QString &tour, const QString &roundSlug, const QString &roundId);
    // Tout le PGN d'une ronde, horloges comprises.
    void pgn(const QString &roundId);
    // La recherche de Lichess dans les tournois diffusés, page par page.
    void search(const QString &query, int page);
    // Toutes les parties de toutes les rondes d'un tournoi.
    void tourPgn(const QString &tourId);
    void cancel();

    // [{tour, round, roundSlug, tourSlug, roundId, ongoing, startsAt, finished, tier}]
    static QVariantList parseTop(const QByteArray &json);
    // {tour, round, games: [{id, name, fen, lastMove, status, white:{name,title,rating,fed,clock},
    //  black:{…}}]} ; « error » si la réponse ne se lit pas.
    static QVariantMap parseRound(const QByteArray &json);

    // [{id, name, slug, location, website, finished}] ; « more » dit s'il reste une page.
    static QVariantList parseSearch(const QByteArray &json, bool *more = nullptr);
    // Un tournoi de la Fédération française : son site, son lieu ou son nom le disent.
    static bool isFrench(const QVariantMap &tour);

  signals:
    void listed(const QVariantList &broadcasts);
    void roundReady(const QVariantMap &round);
    void pgnReady(const QString &roundId, const QByteArray &pgn);
    void searched(const QString &query, int page, const QVariantList &tours, bool more);
    void tourPgnReady(const QString &tourId, const QByteArray &pgn);
    void failed(const QString &error);

  private:
    void get(const QString &url, const QByteArray &accept,
             const std::function<void(const QByteArray &)> &done);
    QNetworkAccessManager *network_;
    QList<QPointer<QNetworkReply>> replies_;
};
} // namespace t64
