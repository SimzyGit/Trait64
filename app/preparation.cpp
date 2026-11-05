// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QtConcurrent>

void Controller::initPreparation() {
    connect(&preparationWorker_, &QFutureWatcher<PreparationResult>::finished, this, [this] {
        preparationActive_ = false;
        const auto result = preparationWorker_.result();
        if (result.generation == preparationGeneration_ && !cancelPreparation_->load()) {
            preparation_["running"] = false;
            preparation_["rows"] = result.rows;
            preparation_["fen"] = result.fen;
            preparation_["error"] = result.error;
            preparation_["status"] = result.error.isEmpty()
                                         ? tr("%1 suite(s) observée(s) dans les parties sélectionnées.")
                                               .arg(result.rows.size())
                                         : tr("Statistiques indisponibles.");
            emit preparationChanged();
        }
        if (preparationPending_)
            startPreparation();
    });
}

void Controller::preparePosition(const QVariantMap &filters) {
    ++preparationGeneration_;
    if (cancelPreparation_)
        cancelPreparation_->store(true);
    preparationFen_ = QString::fromStdString(game_.nodes[node_].fen);
    preparationFilters_ = filters;
    preparationPending_ = true;
    preparation_ = {{"running", true},
                    {"status", "Recherche des suites…"},
                    {"rows", QVariantList()},
                    {"fen", preparationFen_},
                    {"filters", filters}};
    emit preparationChanged();
    if (!preparationActive_)
        startPreparation();
}
void Controller::startPreparation() {
    const auto path = db_->path(), fen = preparationFen_;
    const auto filters = preparationFilters_;
    const auto generation = preparationGeneration_;
    cancelPreparation_ = std::make_shared<std::atomic_bool>(false);
    const auto canceled = cancelPreparation_;
    preparationPending_ = false;
    preparationActive_ = true;
    preparationWorker_.setFuture(QtConcurrent::run([=] {
        PreparationResult result;
        result.fen = fen;
        result.generation = generation;
        try {
            t64::Database db(path);
            result.rows = db.positionStatistics(fen, filters, canceled);
        } catch (const std::exception &e) {
            result.error = QString::fromUtf8(e.what());
        }
        return result;
    }));
}
void Controller::cancelPreparation() {
    ++preparationGeneration_;
    if (cancelPreparation_)
        cancelPreparation_->store(true);
    preparationPending_ = false;
    preparation_ = {{"running", false}, {"status", "Préparation annulée."}, {"rows", QVariantList()}};
    emit preparationChanged();
}
void Controller::playPreparedMove(const QString &uci) {
    try {
        if (trainingActive() || busy_ || preparation_["running"].toBool() ||
            t64::positionKey(game_.nodes[node_].fen) !=
                t64::positionKey(preparation_["fen"].toString().toStdString()))
            throw std::runtime_error(
                "Recalculez les statistiques depuis la position actuelle hors entraînement.");
        bool found = false;
        for (const auto &row : preparation_["rows"].toList())
            found |= row.toMap()["uci"] == uci;
        if (!found)
            throw std::runtime_error("Cette suite n’appartient pas aux résultats affichés.");
        move(uci);
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::exportPreparation(const QString &url) {
    try {
        if (preparation_["running"].toBool() || preparation_["rows"].toList().empty())
            throw std::runtime_error("Calculez des statistiques avant leur export.");
        const auto path = localPath(url);
        db_->validateOutputPath(path);
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            throw std::runtime_error(file.errorString().toStdString());
        QByteArray bytes("\xEF\xBB\xBF");
        bytes += "Filtres;FEN;Coup;Parties;Victoires blancs;Nulles;Victoires noirs;Elo moyen;Parties avec "
                 "Elo;Dernière année connue;Partie exemple\n";
        const QStringList keys{"san",        "count", "white",    "draw",   "black",
                               "averageElo", "rated", "lastYear", "example"};
        auto quote = [](QString value) { return '"' + value.replace('"', "\"\"") + '"'; };
        for (const auto &row : preparation_["rows"].toList()) {
            QStringList fields{quote(QString::fromUtf8(
                                   QJsonDocument(QJsonObject::fromVariantMap(preparation_["filters"].toMap()))
                                       .toJson(QJsonDocument::Compact))),
                               quote(preparation_["fen"].toString())};
            for (const auto &key : keys)
                fields << quote(row.toMap()[key].toString());
            bytes += fields.join(';').toUtf8() + '\n';
        }
        if (file.write(bytes) != bytes.size() || !file.commit())
            throw std::runtime_error(file.errorString().toStdString());
        emit message(tr("Statistiques exportées en CSV."));
    } catch (const std::exception &e) {
        fail(e);
    }
}
