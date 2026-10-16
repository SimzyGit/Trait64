// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QVariant>
#include <atomic>
#include <memory>
#include <sqlite3.h>
#include <stdexcept>

namespace t64 {
// The Qt SQL connection and its progress handler stay on the querying thread.
// Only the shared atomic flag is written from the GUI thread.
class QueryCancellation {
  public:
    QueryCancellation(const QSqlDatabase &db, const std::shared_ptr<std::atomic_bool> &canceled) {
        if (!canceled)
            return;
        require(!canceled->load(), "Recherche annulée.");
        auto native = db.driver()->handle();
        require(native.isValid() && QByteArray(native.typeName()) == "sqlite3*",
                "Le pilote SQLite ne permet pas d’annuler la recherche.");
        handle_ = *static_cast<sqlite3 **>(native.data());
        require(handle_ != nullptr, "Connexion SQLite fermée.");
        sqlite3_progress_handler(
            handle_, 1000, [](void *flag) { return static_cast<std::atomic_bool *>(flag)->load() ? 1 : 0; },
            canceled.get());
    }
    ~QueryCancellation() {
        if (handle_)
            sqlite3_progress_handler(handle_, 0, nullptr, nullptr);
    }
    QueryCancellation(const QueryCancellation &) = delete;
    QueryCancellation &operator=(const QueryCancellation &) = delete;

  private:
    static void require(bool ok, const QString &error) {
        if (!ok)
            throw std::runtime_error(error.toStdString());
    }
    sqlite3 *handle_ = nullptr;
};
} // namespace t64
