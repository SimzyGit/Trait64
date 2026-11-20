// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;

namespace t64 {
// L'explorateur d'ouvertures de Lichess, interrogé en ligne. Deux bases y répondent : les
// parties classées du site, qui se comptent en milliards, et celles des maîtres sur
// l'échiquier. Rien n'est téléchargé : on demande une position, on reçoit ce qui s'y joue.
// API publique, sans compte ni jeton.
class OpeningExplorer : public QObject {
    Q_OBJECT
  public:
    explicit OpeningExplorer(QObject *parent = nullptr, QNetworkAccessManager *network = nullptr);
    // source : « lichess » pour les parties classées du site, « masters » pour l'échiquier.
    // filters : speeds et ratings (listes), since et until (mois « 2019-01 » pour lichess,
    // années « 1952 » pour masters).
    void query(const QString &source, const QString &fen, const QVariantMap &filters = {});
    void cancel();
    // Le dossier des tables Syzygy de l'ordinateur : quand elles couvrent la position, les
    // finales s'y lisent sans réseau. Vide : toujours Lichess.
    void setLocalTables(const QString &folder) { localTables_ = folder; }
    // Jeton personnel Lichess : l'explorateur refuse les requêtes anonymes depuis 2025.
    // Il n'est envoyé qu'à explorer.lichess.ovh, jamais aux autres services.
    void setToken(const QString &token) { token_ = token.trimmed(); }
    bool busy() const { return !reply_.isNull(); }
    static bool knownSource(const QString &source);
    // Séparées de la requête pour s'éprouver sans réseau.
    static QUrl buildUrl(const QString &source, const QString &fen, const QVariantMap &filters);
    // Rend le même plan que le dossier de la bibliothèque : l'interface n'a qu'une
    // lecture à connaître, quelle que soit la base interrogée.
    static QVariantMap parse(const QByteArray &body);
    // Les tables de finales de Lichess (sept pièces au plus) : le verdict de la position
    // et celui de chaque coup, vus du camp au trait.
    static QVariantMap parseTablebase(const QByteArray &body);
    // L'analyse dans le nuage de Lichess : les lignes que ses joueurs ont déjà fait
    // calculer pour cette position (trois au plus), évaluées du point de vue des Blancs.
    static QVariantMap parseCloud(const QByteArray &body, const QString &fen);
    static int pieces(const QString &fen);

  signals:
    void ready(const QVariantMap &report);
    void failed(const QString &message);

  private:
    QNetworkAccessManager *network_;
    QPointer<QNetworkReply> reply_;
    QString localTables_, token_;
};
} // namespace t64
