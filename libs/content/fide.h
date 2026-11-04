// SPDX-License-Identifier: AGPL-3.0-or-later
// Classements FIDE, lus dans les listes mensuelles que la fédération publie elle-même.
// On ne lit pas la fiche web d'un joueur : c'est du HTML de présentation, qui change au
// gré des refontes du site. Les listes sont un format publié, stable depuis des années,
// et le téléchargement reste la seule façon officielle d'obtenir ces chiffres.
#pragma once
#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <functional>
#include <memory>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

namespace t64 {

struct FidePlayer {
    QString id, name, country, title;
    int rating = 0, games = 0, birthYear = 0;
    bool found = false;
};

class FideList {
  public:
    // "standard", "rapid" ou "blitz". Renvoie une adresse vide pour toute autre cadence.
    static QUrl listUrl(const QString &speed);
    static bool validId(const QString &fideId);

    // Lit l'archive téléchargée et en extrait le seul joueur demandé. La liste standard
    // dépasse le million de joueurs : elle est parcourue par morceaux et rien n'est
    // conservé en dehors de la fiche recherchée.
    static FidePlayer findInArchive(const QString &zipPath, const QString &fideId);

    // Même recherche, alimentée morceau par morceau. `next` renvoie le bloc suivant, et
    // un bloc vide marque la fin. Séparé du téléchargement et de l'archive pour être
    // vérifiable sur un extrait, sans réseau ni fichier.
    static FidePlayer findInChunks(const std::function<QByteArray()> &next, const QString &fideId);
};

// Télécharge les listes demandées, l'une après l'autre, et en extrait un seul joueur.
// Chaque liste pèse quelques dizaines de mégaoctets : elle est écrite au fur et à mesure
// dans un fichier de travail, puis effacée. Rien n'est conservé hors la fiche trouvée.
class FideSync : public QObject {
    Q_OBJECT
  public:
    explicit FideSync(QObject *parent = nullptr, QNetworkAccessManager *network = nullptr);
    ~FideSync() override;
    bool busy() const;
    // `speeds` est un sous-ensemble de {"standard", "rapid", "blitz"} : ne demander que
    // les cadences utiles évite de télécharger trois fois pour un seul chiffre.
    void fetch(const QString &fideId, const QStringList &speeds, const QString &workDirectory);
    void cancel();

  signals:
    // `index` et `count` situent la cadence en cours parmi celles demandées.
    void progress(const QString &speed, int index, int count, qint64 received, qint64 total);
    // {found, name, country, title, standard, rapid, blitz} ; une cadence où le joueur
    // n'est pas classé reste absente, ce qui se distingue d'un classement nul.
    void finished(const QVariantMap &player);
    void failed(const QString &message);

  private:
    void startNext();
    void finishList(const QString &path);
    void fail(const QString &message);
    void cleanup();
    QNetworkAccessManager *network_;
    QPointer<QNetworkReply> reply_;
    std::unique_ptr<QFile> output_;
    QString id_, directory_, current_;
    QStringList pending_;
    QVariantMap found_;
    int done_ = 0, count_ = 0;
};

} // namespace t64
