// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include "move_notation.h"
#include <QtConcurrent>

void Controller::initLibrarySearch() {
    searchEnabled_ = true;
    connect(&searchWorker_, &QFutureWatcher<SearchResult>::finished, this, [this] {
        // Do not replace a future before its finished signal is handled. A completed
        // future may still have a queued signal while a newer query arrives.
        searchActive_ = false;
        const auto result = searchWorker_.result();
        if (result.generation == searchGeneration_ && !cancelSearch_->load()) {
            games_ = result.rows;
            libraryHasNext_ = games_.size() > libraryPageSize;
            if (libraryHasNext_)
                games_.removeLast();
            librarySearching_ = false;
            librarySearchStatus_ = result.error.isEmpty()
                                       // Ce compteur est le seul texte de la page
                                       // restait en français quand l'interface est en
                                       // anglais : il passe par la traduction.
                                       ? tr("Page %1 · %2 partie(s)%3")
                                             .arg(libraryPage())
                                             .arg(games_.size())
                                             .arg(libraryHasNext_ ? tr(" · suite disponible") : QString())
                                       : tr("Recherche impossible.");
            emit libraryChanged();
            emit searchChanged();
            // L'analyse affiche le rang de la partie ouverte dans cette liste :
            // une nouvelle recherche le déplace.
            refresh();
            if (!result.error.isEmpty())
                emit message(result.error, true);
        }
        if (searchPending_ && searchEnabled_)
            startLibrarySearch();
    });
}

void Controller::requestLibrarySearch(bool resetPage) {
    if (!searchEnabled_)
        return;
    if (resetPage) {
        searchCursor_.clear();
        searchCursors_.clear();
    }
    ++searchGeneration_;
    if (cancelSearch_)
        cancelSearch_->store(true);
    searchPending_ = true;
    librarySearching_ = true;
    libraryHasNext_ = false;
    librarySearchStatus_ = "Recherche…";
    emit searchChanged();
    if (!searchActive_)
        startLibrarySearch();
}

void Controller::startLibrarySearch() {
    const auto path = db_->path();
    const auto query = query_, result = resultFilter_;
    const auto position = positionFilter_ ? QString::fromStdString(game_.nodes[node_].fen) : QString();
    const bool favorite = favoritesOnly_, trash = trashFilter_;
    auto filters = advancedFilters_;
    filters.remove("offset");
    filters["cursor"] = searchCursor_;
    filters["limit"] = libraryPageSize + 1;
    const auto generation = searchGeneration_;
    cancelSearch_ = std::make_shared<std::atomic_bool>(false);
    const auto canceled = cancelSearch_;
    searchPending_ = false;
    searchActive_ = true;
    searchWorker_.setFuture(QtConcurrent::run([=] {
        SearchResult found;
        found.generation = generation;
        try {
            if (!canceled->load()) {
                t64::Database database(path);
                found.rows = database.search(query, result, favorite, position, trash, filters, canceled);
            }
        } catch (const std::exception &e) {
            found.error = QString::fromUtf8(e.what());
        }
        return found;
    }));
}

void Controller::cancelLibrarySearch() {
    ++searchGeneration_;
    if (cancelSearch_)
        cancelSearch_->store(true);
    searchPending_ = false;
    librarySearching_ = false;
    libraryHasNext_ = false;
    searchCursor_.clear();
    searchCursors_.clear();
    games_.clear();
    librarySearchStatus_ = "Recherche annulée.";
    emit libraryChanged();
    emit searchChanged();
}

void Controller::nextLibraryPage() {
    if (librarySearching_ || !libraryHasNext_ || games_.isEmpty())
        return;
    searchCursors_ << searchCursor_;
    searchCursor_ = games_.last().toMap();
    requestLibrarySearch(false);
}

void Controller::previousLibraryPage() {
    if (librarySearching_ || searchCursors_.isEmpty())
        return;
    searchCursor_ = searchCursors_.takeLast();
    requestLibrarySearch(false);
}

QVariantMap Controller::tournamentTable(qint64 gameId) {
    try {
        return db_->tournament(gameId);
    } catch (const std::exception &e) {
        fail(e);
        return {};
    }
}

void Controller::findNameVariants(const QString &field) {
    if (nameVariants_.value("running").toBool())
        return;
    nameVariants_ = {{"field", field}, {"running", true}, {"groups", QVariantList()}};
    emit nameVariantsChanged();
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher] {
        watcher->deleteLater();
        nameVariants_ = watcher->result();
        emit nameVariantsChanged();
    });
    // Sur sa propre connexion : regrouper les noms d'un million de parties prend du temps.
    watcher->setFuture(QtConcurrent::run([path = db_->path(), field] {
        QVariantMap done{{"field", field}, {"running", false}};
        try {
            done["groups"] = t64::Database(path).nameVariants(field);
        } catch (const std::exception &e) {
            done["groups"] = QVariantList();
            done["error"] = QString::fromUtf8(e.what());
        }
        return done;
    }));
}

void Controller::unifyNames(const QString &field, const QStringList &spellings, const QString &kept) {
    try {
        qint64 changed = 0;
        for (const auto &name : spellings)
            changed += db_->renameName(field, name, kept);
        emit message(tr("%1 partie(s) corrigée(s) : « %2 ».").arg(changed).arg(kept.trimmed()));
        refreshLibrary();
        findNameVariants(field);
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::saveSearch(const QString &name, const QVariantMap &search) {
    const auto title = name.simplified().left(80);
    if (title.isEmpty())
        return;
    auto list = savedSearches();
    auto entry = search;
    entry["name"] = title;
    entry["filters"] = [filters = search.value("filters").toMap()]() mutable {
        filters.remove("cursor"); // une page n'est pas une recherche
        return filters;
    }();
    bool replaced = false;
    for (auto &item : list)
        if (item.toMap()["name"].toString() == title) {
            item = entry;
            replaced = true;
        }
    if (!replaced)
        list << entry;
    settings_.setValue("savedSearches", list);
    emit savedSearchesChanged();
    emit message(tr("Recherche « %1 » enregistrée.").arg(title));
}

void Controller::removeSearch(int index) {
    auto list = savedSearches();
    if (index < 0 || index >= list.size())
        return;
    list.removeAt(index);
    settings_.setValue("savedSearches", list);
    emit savedSearchesChanged();
}

void Controller::findTournaments(const QString &text) {
    tournamentList_["text"] = text;
    if (tournamentList_.value("running").toBool())
        return; // la réponse en cours sera suivie d'une nouvelle, voir plus bas
    tournamentList_["running"] = true;
    emit tournamentListChanged();
    auto *watcher = new QFutureWatcher<QVariantList>(this);
    connect(watcher, &QFutureWatcher<QVariantList>::finished, this, [this, watcher, text] {
        watcher->deleteLater();
        tournamentList_["running"] = false;
        tournamentList_["rows"] = watcher->result();
        emit tournamentListChanged();
        // On a tapé pendant le calcul : on refait avec le dernier texte.
        if (tournamentList_["text"].toString() != text)
            findTournaments(tournamentList_["text"].toString());
    });
    watcher->setFuture(QtConcurrent::run([path = db_->path(), text] {
        try {
            return t64::Database(path).tournaments(text);
        } catch (const std::exception &) {
            return QVariantList();
        }
    }));
}

namespace {
// Les lignes groupées par la recherche (année, résultat, tranche d'Elo, nombre) mises en
// trois séries à dessiner. Plus de quarante ans d'écart : les années vont par décennies.
QVariantMap summarize(const QVariantList &rows) {
    QMap<int, qint64> years, elo;
    qint64 total = 0, white = 0, draws = 0, black = 0, unfinished = 0, unknownElo = 0;
    double eloSum = 0;
    for (const auto &row : rows) {
        const auto r = row.toMap();
        const qint64 n = r["count"].toLongLong();
        total += n;
        if (const int year = r["year"].toInt(); year > 0)
            years[year] += n;
        const auto result = r["result"].toString();
        (result == "1-0" ? white : result == "0-1" ? black : result == "1/2-1/2" ? draws : unfinished) += n;
        if (const int band = r["elo"].toInt(); band > 0) {
            elo[band] += n;
            eloSum += (band + 50.) * double(n);
        } else {
            unknownElo += n;
        }
    }
    const bool byDecade = !years.isEmpty() && years.lastKey() - years.firstKey() > 40;
    if (byDecade) {
        QMap<int, qint64> decades;
        for (auto it = years.cbegin(); it != years.cend(); ++it)
            decades[it.key() / 10 * 10] += it.value();
        years = decades;
    }
    QVariantList yearList, eloList;
    for (auto it = years.cbegin(); it != years.cend(); ++it)
        yearList << QVariantMap{
            {"label", byDecade ? QString::number(it.key()) + "–" + QString::number(it.key() + 9)
                               : QString::number(it.key())},
            {"count", it.value()}};
    for (auto it = elo.cbegin(); it != elo.cend(); ++it)
        eloList << QVariantMap{{"label", QString::number(it.key()) + "–" + QString::number(it.key() + 99)},
                               {"count", it.value()}};
    const qint64 rated = total - unknownElo;
    return {{"total", total},
            {"years", yearList},
            {"elo", eloList},
            {"unknownElo", unknownElo},
            {"averageElo", rated ? qRound(eloSum / double(rated)) : 0},
            {"results",
             QVariantMap{{"white", white}, {"draws", draws}, {"black", black}, {"unfinished", unfinished}}}};
}
} // namespace

void Controller::summarizeLibrary() {
    const auto generation = ++summaryGeneration_;
    librarySummary_ = {{"running", true}};
    emit librarySummaryChanged();
    auto filters = advancedFilters_;
    filters.remove("offset");
    filters.remove("cursor");
    filters["summary"] = true;
    const auto position = positionFilter_ ? QString::fromStdString(game_.nodes[node_].fen) : QString();
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, generation] {
        watcher->deleteLater();
        if (generation != summaryGeneration_)
            return;
        librarySummary_ = watcher->result();
        emit librarySummaryChanged();
    });
    watcher->setFuture(QtConcurrent::run([path = db_->path(), query = query_, result = resultFilter_,
                                          favorite = favoritesOnly_, trash = trashFilter_, position,
                                          filters] {
        try {
            return summarize(t64::Database(path).search(query, result, favorite, position, trash, filters));
        } catch (const std::exception &e) {
            return QVariantMap{{"error", QString::fromUtf8(e.what())}};
        }
    }));
}

void Controller::explorePlans() {
    const auto generation = ++plansGeneration_;
    const auto fen = game_.nodes[node_].fen;
    plans_ = {{"running", true}, {"fen", QString::fromStdString(fen)}};
    emit plansChanged();
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, generation] {
        watcher->deleteLater();
        if (generation != plansGeneration_)
            return;
        plans_ = watcher->result();
        emit plansChanged();
    });
    const int notation = frenchNotation();
    watcher->setFuture(QtConcurrent::run([path = db_->path(), fen, notation] {
        try {
            auto found = t64::Database(path).plans(fen);
            // Les coups dans la notation choisie : « Fd3 » en français.
            for (const auto *side : {"white", "black"}) {
                auto rows = found[side].toList();
                for (auto &row : rows) {
                    auto r = row.toMap();
                    r["text"] = t64::displaySan(r["move"].toString().toStdString(), notation);
                    row = r;
                }
                found[side] = rows;
            }
            found["fen"] = QString::fromStdString(fen);
            return found;
        } catch (const std::exception &e) {
            return QVariantMap{{"error", QString::fromUtf8(e.what())}};
        }
    }));
}
