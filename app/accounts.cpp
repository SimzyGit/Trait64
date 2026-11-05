// SPDX-License-Identifier: AGPL-3.0-or-later
// Saved Lichess / Chess.com accounts, kept in sync with the library like En Croissant does:
// each sync only asks for the games played since the previous one.
#include "controller.h"
#include <QDateTime>
#include <QTimer>

QVariantList Controller::onlineAccounts() const {
    QVariantList accounts;
    for (const auto &value : settings_.value("onlineAccounts").toList()) {
        auto account = value.toMap();
        const auto key = "onlineSync/" + account["site"].toString() + "/" + account["user"].toString().toLower();
        account["lastSync"] = settings_.value(key, 0).toLongLong();
        account["syncing"] = online_ && onlineSyncKey_ == key && (online_->busy() || busy_);
        accounts << account;
    }
    return accounts;
}

void Controller::addOnlineAccount(const QString &site, const QString &user) {
    const auto name = user.trimmed();
    if (!QStringList{"lichess", "chesscom"}.contains(site) || !t64::OnlineGames::validUser(name)) {
        emit message(tr("Pseudo invalide : lettres, chiffres, - et _ uniquement."), true);
        return;
    }
    auto list = settings_.value("onlineAccounts").toList();
    for (const auto &value : list)
        if (value.toMap()["site"] == site && value.toMap()["user"].toString().compare(name, Qt::CaseInsensitive) == 0) {
            emit message(tr("Ce compte est déjà enregistré."));
            return;
        }
    list << QVariantMap{{"site", site}, {"user", name}, {"games", 0}};
    settings_.setValue("onlineAccounts", list);
    emit accountsChanged();
    syncOnlineAccount(int(list.size()) - 1);
}

void Controller::removeOnlineAccount(int index) {
    auto list = settings_.value("onlineAccounts").toList();
    if (index < 0 || index >= list.size()) return;
    list.removeAt(index);
    settings_.setValue("onlineAccounts", list);
    // Games already imported stay in the library, filed under their source.
    syncQueue_.clear();
    emit accountsChanged();
}

void Controller::syncOnlineAccount(int index) {
    syncQueue_.removeAll(index);
    syncQueue_.prepend(index);
    if (!busy()) nextAccountSync();
    emit accountsChanged();
}

void Controller::syncAllAccounts() {
    const int count = int(settings_.value("onlineAccounts").toList().size());
    for (int i = 0; i < count; ++i)
        if (!syncQueue_.contains(i)) syncQueue_ << i;
    if (!busy()) nextAccountSync();
    emit accountsChanged();
}

void Controller::nextAccountSync() {
    const auto list = settings_.value("onlineAccounts").toList();
    while (!syncQueue_.isEmpty()) {
        const int index = syncQueue_.takeFirst();
        if (index < 0 || index >= list.size()) continue;
        const auto account = list[index].toMap();
        syncingAccount_ = index;
        // A first sync fetches the latest games (the API's limit); later ones only the new ones.
        importOnline(account["site"].toString(), account["user"].toString(), 5000);
        emit accountsChanged();
        return;
    }
    syncingAccount_ = -1;
    emit accountsChanged();
}

void Controller::accountSynced(int added) {
    if (syncingAccount_ < 0) return;
    auto list = settings_.value("onlineAccounts").toList();
    if (syncingAccount_ < list.size()) {
        auto account = list[syncingAccount_].toMap();
        account["games"] = account["games"].toInt() + added;
        list[syncingAccount_] = account;
        settings_.setValue("onlineAccounts", list);
    }
    syncingAccount_ = -1;
    emit accountsChanged();
    // Let the library refresh before the next account starts.
    QTimer::singleShot(0, this, [this] { if (!busy()) nextAccountSync(); });
}
