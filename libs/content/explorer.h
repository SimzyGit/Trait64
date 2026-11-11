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
    static int pieces(const QString &fen);

  signals:
    void ready(const QVariantMap &report);
    void failed(const QString &message);

  private:
    QNetworkAccessManager *network_;
    QPointer<QNetworkReply> reply_;
};
} // namespace t64
