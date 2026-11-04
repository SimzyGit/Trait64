// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlQuery>
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
        auto *handle = *static_cast<sqlite3 **>(native.data());
        require(handle != nullptr, "Connexion SQLite fermée.");
        // Le pilote Qt peut embarquer sa propre copie de SQLite, distincte de celle que
        // nous lions : c'est le cas des binaires officiels de Qt sur macOS et Windows.
        // Poser notre gestionnaire de progression sur sa connexion serait alors confier
        // l'objet d'une bibliothèque aux fonctions d'une autre, ce qui plante. Faute de
        // moyen portable d'identifier la bibliothèque d'en face, la version sert de
        // garde-fou : au moindre doute on renonce à l'annulation plutôt que de risquer
        // un arrêt brutal, et la recherche se déroule simplement sans être annulable.
        if (!sameLibrary(db))
            return;
        handle_ = handle;
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

    // Faux quand l'annulation n'a pas pu être posée : la recherche aboutira, mais elle
    // ira jusqu'au bout. Les tests s'en servent pour se déclarer non concernés plutôt
    // que d'échouer sur une fonction indisponible par construction.
    bool active() const { return handle_ != nullptr; }

    // Vrai lorsque la connexion Qt et notre bibliothèque SQLite annoncent la même
    // version. Deux bibliothèques peuvent partager un numéro sans être le même binaire,
    // mais une différence de numéro est la preuve qu'elles diffèrent.
    static bool sameLibrary(const QSqlDatabase &db) {
        QSqlQuery version(db);
        if (!version.exec("SELECT sqlite_version()") || !version.next())
            return false;
        return version.value(0).toString() == QString::fromLatin1(sqlite3_libversion());
    }

  private:
    static void require(bool ok, const QString &error) {
        if (!ok)
            throw std::runtime_error(error.toStdString());
    }
    sqlite3 *handle_ = nullptr;
};
} // namespace t64
