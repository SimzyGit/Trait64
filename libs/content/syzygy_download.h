// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QFile>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

namespace t64 {
namespace syzygy {
// Les tables d'un nombre de pièces donné, nommées comme Fathom les cherche (« KQvKR »).
// 3 : 5 tables, 4 : 30, 5 : 110, 6 : 365. Chacune a deux fichiers, .rtbw et .rtbz.
QStringList tableNames(int pieces);
// Chaque fichier Syzygy porte à sa fin la somme CityHash de son contenu : un fichier
// tronqué ou abîmé se voit sans liste de contrôle extérieure.
bool intact(const QString &path);
// Taille annoncée d'un paquet, pour la comparer à la place libre.
qint64 packBytes(const QString &pack);
} // namespace syzygy

// Télécharge un paquet de tables (« 3-4-5 » ou « 6 ») dans un dossier, fichier par
// fichier : chacun est vérifié avant de prendre son nom, un fichier déjà là n'est pas
// repris, et un téléchargement interrompu repart où il s'était arrêté.
class SyzygyDownload : public QObject {
    Q_OBJECT
  public:
    explicit SyzygyDownload(QObject *parent = nullptr, QNetworkAccessManager *network = nullptr);
    void start(const QString &folder, const QString &pack);
    void cancel();
    bool busy() const { return !reply_.isNull() || checking_; }
    // Les adresses essayées pour un fichier, du miroir préféré au dernier recours.
    static QStringList candidates(const QString &pack, const QString &file);

  signals:
    void progress(int done, int total, qint64 received);
    void finished(const QString &error);

  private:
    void next();
    void fetch();
    void fail(const QString &error);

    QNetworkAccessManager *network_;
    QPointer<QNetworkReply> reply_;
    QString folder_, pack_;
    QStringList files_;
    int index_ = 0, mirror_ = 0, tried_ = 0;
    qint64 received_ = 0;
    std::unique_ptr<QFile> part_;
    bool checking_ = false;
};
} // namespace t64
