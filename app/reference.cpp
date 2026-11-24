// SPDX-License-Identifier: AGPL-3.0-or-later
// La base de référence : les rondes terminées des diffusions Lichess, rapatriées à la
// demande ou au démarrage et ajoutées sans doublon ; les fichiers de la base élite de
// Lichess ou de TWIC, téléchargés par l'utilisateur, importés de même.
#include "controller.h"
#include "unpack.h"
#include <QDir>
#include <QSaveFile>

namespace {
// Les rondes déjà importées, gardées dans les réglages : au-delà, les plus anciennes
// tombent, une ronde déjà présente étant de toute façon écartée comme doublon.
constexpr int keptRounds = 3000;
// Une mise à jour ne rapatrie pas plus de rondes d'un coup : la liste de Lichess en
// propose une page, et chaque PGN est une requête.
constexpr int roundsPerRun = 60;
// La base FFE : les recherches posées à Lichess, et pas plus de tournois par passage ; le
// passage suivant reprend où celui-ci s'est arrêté.
const QStringList ffeQueries{"FFE", "Championnat de France", "Top 12", "Top Jeunes", "Coupe de France",
                             "Nationale 1", "France"};
constexpr int ffePages = 5;
constexpr int ffeToursPerRun = 40;
} // namespace

QVariantMap Controller::reference() const {
    return {{"running", referenceRunning_},
            {"status", referenceStatus_},
            {"last", settings_.value("reference/last", 0)},
            {"rounds", settings_.value("reference/rounds").toStringList().size()},
            {"atStartup", settings_.value("reference/atStartup", false)},
            {"ffeRunning", ffeRunning_},
            {"ffeStatus", ffeStatus_},
            {"ffeLast", settings_.value("ffe/last", 0)},
            {"ffeTours", settings_.value("ffe/tours").toStringList().size()}};
}

void Controller::updateReference() {
    if (referenceRunning_)
        return;
    if (!referenceFeed_) {
        referenceFeed_ = new t64::Broadcasts(this);
        connect(referenceFeed_, &t64::Broadcasts::listed, this, [this](const QVariantList &rounds) {
            const auto done = settings_.value("reference/rounds").toStringList();
            referenceQueue_.clear();
            for (const auto &row : rounds) {
                const auto round = row.toMap();
                const auto id = round["roundId"].toString();
                if (round["finished"].toBool() && !done.contains(id) && referenceQueue_.size() < roundsPerRun)
                    referenceQueue_ << id;
            }
            referencePgn_.clear();
            referenceDone_.clear();
            if (referenceQueue_.isEmpty()) {
                referenceRunning_ = false;
                referenceStatus_ = tr("Déjà à jour : aucune nouvelle ronde terminée.");
                settings_.setValue("reference/last", QDateTime::currentMSecsSinceEpoch());
                emit referenceChanged();
                return;
            }
            nextReferenceRound();
        });
        connect(referenceFeed_, &t64::Broadcasts::pgnReady, this,
                [this](const QString &roundId, const QByteArray &pgn) {
                    if (!pgn.trimmed().isEmpty()) {
                        referencePgn_ += pgn.trimmed() + "\n\n";
                        referenceDone_ << roundId;
                    }
                    nextReferenceRound();
                });
        connect(referenceFeed_, &t64::Broadcasts::failed, this, [this](const QString &error) {
            // Une ronde illisible n'arrête pas les autres ; la liste illisible, si.
            if (!referenceQueue_.isEmpty() || !referencePgn_.isEmpty()) {
                nextReferenceRound();
                return;
            }
            referenceRunning_ = false;
            referenceStatus_ = tr("Mise à jour impossible : %1").arg(error);
            emit referenceChanged();
        });
    }
    referenceRunning_ = true;
    referenceStatus_ = tr("Liste des diffusions…");
    emit referenceChanged();
    referenceFeed_->list();
}

void Controller::nextReferenceRound() {
    if (!referenceQueue_.isEmpty()) {
        const auto id = referenceQueue_.takeFirst();
        referenceStatus_ = tr("Diffusions : %1 ronde(s) lue(s), %2 restante(s)…")
                               .arg(referenceDone_.size())
                               .arg(referenceQueue_.size() + 1);
        emit referenceChanged();
        referenceFeed_->pgn(id);
        return;
    }
    finishReference();
}

void Controller::finishReference() {
    if (referencePgn_.isEmpty()) {
        referenceRunning_ = false;
        referenceStatus_ = tr("Aucune partie dans les rondes lues.");
        emit referenceChanged();
        return;
    }
    QDir().mkpath(dataPath_ + "/downloads");
    const auto file = dataPath_ + "/downloads/reference-broadcasts.pgn";
    QSaveFile out(file);
    if (!out.open(QIODevice::WriteOnly) || out.write(referencePgn_) < 0 || !out.commit()) {
        referenceRunning_ = false;
        referenceStatus_ = tr("Écriture impossible : %1").arg(file);
        emit referenceChanged();
        return;
    }
    referencePgn_.clear();
    const auto rounds = referenceDone_;
    // Les rondes ne sont notées qu'une fois l'import fini : interrompu, il reprendra.
    auto *done = new QMetaObject::Connection;
    *done = connect(this, &Controller::busyChanged, this, [this, done, rounds, file] {
        if (busy())
            return;
        disconnect(*done);
        delete done;
        auto known = settings_.value("reference/rounds").toStringList() + rounds;
        while (known.size() > keptRounds)
            known.removeFirst();
        settings_.setValue("reference/rounds", known);
        settings_.setValue("reference/last", QDateTime::currentMSecsSinceEpoch());
        QFile::remove(file);
        referenceRunning_ = false;
        referenceStatus_ = tr("%1 ronde(s) de diffusions ajoutée(s).").arg(rounds.size());
        emit referenceChanged();
    });
    referenceStatus_ = tr("Import de %1 ronde(s)…").arg(rounds.size());
    emit referenceChanged();
    importSourceFile(file, {{"id", "reference:broadcasts"},
                            {"title", tr("Référence — diffusions Lichess")},
                            {"source", "https://lichess.org/broadcast"},
                            {"license", "CC0"}});
}

void Controller::importReferenceFile(const QString &url, const QString &kind) {
    const bool elite = kind == "elite";
    QString file = localPath(url);
    // Les bases mensuelles arrivent souvent en zip : le PGN qu'il contient est extrait à côté.
    if (file.endsWith(".zip", Qt::CaseInsensitive)) {
        QDir().mkpath(dataPath_ + "/downloads");
        const auto pgn = dataPath_ + "/downloads/reference-" + kind + ".pgn";
        if (const auto error = t64::extractPgnFromZip(file, pgn); !error.isEmpty()) {
            emit message(tr(error.toUtf8().constData()), true);
            return;
        }
        file = pgn;
    }
    importSourceFile(file,
                     {{"id", elite ? "reference:elite" : "reference:twic"},
                      {"title", elite ? tr("Référence — base élite Lichess") : tr("Référence — TWIC")},
                      {"source", elite ? "https://database.nikonoel.fr" : "https://theweekinchess.com"}});
    settings_.setValue("reference/last", QDateTime::currentMSecsSinceEpoch());
    emit referenceChanged();
}

void Controller::updateFfe() {
    if (ffeRunning_)
        return;
    if (!ffeFeed_) {
        ffeFeed_ = new t64::Broadcasts(this);
        connect(ffeFeed_, &t64::Broadcasts::searched, this,
                [this](const QString &query, int page, const QVariantList &tours, bool more) {
                    for (const auto &row : tours) {
                        const auto tour = row.toMap();
                        if (t64::Broadcasts::isFrench(tour))
                            ffeFound_[tour["id"].toString()] = tour;
                    }
                    if (more && page < ffePages)
                        ffeSearches_.prepend({query, page + 1});
                    nextFfeStep();
                });
        connect(ffeFeed_, &t64::Broadcasts::tourPgnReady, this, [this](const QString &id, const QByteArray &pgn) {
            if (!pgn.trimmed().isEmpty())
                ffePgn_ += pgn.trimmed() + "\n\n";
            ffeDone_ << id;
            nextFfeStep();
        });
        connect(ffeFeed_, &t64::Broadcasts::failed, this, [this](const QString &error) {
            // Une recherche ou un tournoi illisible n'arrête pas les autres, sauf si rien
            // n'a encore répondu : le réseau manque.
            if (ffeFound_.isEmpty() && ffeDone_.isEmpty() && ffeQueue_.isEmpty()) {
                finishFfe(tr("Téléchargement impossible : %1").arg(error));
                return;
            }
            nextFfeStep();
        });
    }
    ffeRunning_ = true;
    ffeFound_.clear();
    ffeQueue_.clear();
    ffeDone_.clear();
    ffeFinished_.clear();
    ffePgn_.clear();
    ffeSearches_.clear();
    for (const auto &query : ffeQueries)
        ffeSearches_.append({query, 1});
    ffeStatus_ = tr("Recherche des tournois français…");
    emit referenceChanged();
    nextFfeStep();
}

void Controller::nextFfeStep() {
    if (!ffeSearches_.isEmpty()) {
        const auto [query, page] = ffeSearches_.takeFirst();
        ffeFeed_->search(query, page);
        return;
    }
    // Les recherches finies : les tournois à lire, sauf ceux déjà rapatriés une fois terminés.
    if (ffeQueue_.isEmpty() && ffeDone_.isEmpty() && !ffeFound_.isEmpty()) {
        const auto known = settings_.value("ffe/tours").toStringList();
        for (auto it = ffeFound_.cbegin(); it != ffeFound_.cend() && ffeQueue_.size() < ffeToursPerRun; ++it)
            if (!known.contains(it.key())) {
                ffeQueue_ << it.key();
                if (it.value().toMap()["finished"].toBool())
                    ffeFinished_ << it.key();
            }
        ffeFound_.clear();
        if (ffeQueue_.isEmpty()) {
            finishFfe(tr("Déjà à jour : aucun nouveau tournoi français."));
            return;
        }
    }
    if (!ffeQueue_.isEmpty()) {
        const auto id = ffeQueue_.takeFirst();
        ffeStatus_ = tr("Tournois français : %1 lu(s), %2 restant(s)…").arg(ffeDone_.size()).arg(ffeQueue_.size() + 1);
        emit referenceChanged();
        ffeFeed_->tourPgn(id);
        return;
    }
    if (ffePgn_.isEmpty()) {
        finishFfe(ffeDone_.isEmpty() ? tr("Aucun tournoi français trouvé dans les diffusions de Lichess.")
                                     : tr("Aucune partie dans les tournois lus."));
        return;
    }
    QDir().mkpath(dataPath_ + "/downloads");
    const auto file = dataPath_ + "/downloads/ffe.pgn";
    QSaveFile out(file);
    if (!out.open(QIODevice::WriteOnly) || out.write(ffePgn_) < 0 || !out.commit()) {
        finishFfe(tr("Écriture impossible : %1").arg(file));
        return;
    }
    ffePgn_.clear();
    // Seuls les tournois terminés sont notés : un tournoi en cours sera relu, ses parties
    // déjà présentes écartées comme doublons. Noté une fois l'import fini.
    const auto finished = ffeFinished_;
    const int tours = ffeDone_.size();
    auto *done = new QMetaObject::Connection;
    *done = connect(this, &Controller::busyChanged, this, [this, done, finished, tours, file] {
        if (busy())
            return;
        disconnect(*done);
        delete done;
        auto known = settings_.value("ffe/tours").toStringList() + finished;
        known.removeDuplicates();
        settings_.setValue("ffe/tours", known);
        QFile::remove(file);
        finishFfe(tr("%1 tournoi(s) français ajouté(s) à la base FFE.").arg(tours));
    });
    ffeStatus_ = tr("Import de %1 tournoi(s)…").arg(tours);
    emit referenceChanged();
    importSourceFile(file, {{"id", "reference:ffe"},
                            {"title", tr("FFE — tournois français")},
                            {"source", "https://lichess.org/broadcast"},
                            {"license", "CC0"}});
}

void Controller::finishFfe(const QString &status) {
    ffeRunning_ = false;
    ffeStatus_ = status;
    settings_.setValue("ffe/last", QDateTime::currentMSecsSinceEpoch());
    emit referenceChanged();
}
