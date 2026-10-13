// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
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
                                       ? QString("Page %1 · %2 partie(s)%3")
                                             .arg(libraryPage())
                                             .arg(games_.size())
                                             .arg(libraryHasNext_ ? " · suite disponible" : "")
                                       : "Recherche impossible.";
            emit libraryChanged();
            emit searchChanged();
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
