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
} // namespace

QVariantMap Controller::reference() const {
    return {{"running", referenceRunning_},
            {"status", referenceStatus_},
            {"last", settings_.value("reference/last", 0)},
            {"rounds", settings_.value("reference/rounds").toStringList().size()},
            {"atStartup", settings_.value("reference/atStartup", false)}};
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
