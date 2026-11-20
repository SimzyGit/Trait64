// SPDX-License-Identifier: AGPL-3.0-or-later
// Saved Lichess / Chess.com accounts, kept in sync with the library like En Croissant does:
// each sync only asks for the games played since the previous one.
#include "controller.h"
#include "sounds.h"
#include <QDateTime>
#include <QDesktopServices>
#include <QRegularExpression>
#include <QTimer>

QVariantList Controller::onlineAccounts() const {
    QVariantList accounts;
    for (const auto &value : settings_.value("onlineAccounts").toList()) {
        auto account = value.toMap();
        const auto key = "onlineSync/" + account["site"].toString() + "/" + account["user"].toString().toLower();
        account["lastSync"] = settings_.value(key, 0).toLongLong();
        account["syncing"] = online_ && onlineSyncKey_ == key && (online_->busy() || busy_);
        // Le jeton ne repart jamais vers l'interface : seulement le fait qu'il y en a un.
        account["connected"] = !account.take("token").toString().isEmpty();
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

void Controller::setLichessToken(const QString &token) {
    const auto clean = token.trimmed();
    // Un jeton Lichess : lettres, chiffres et soulignés (lip_…). Autre chose serait une
    // erreur de copier-coller, et partirait tel quel dans un en-tête HTTP.
    static const QRegularExpression shape("^[A-Za-z0-9_]{8,200}$");
    if (!clean.isEmpty() && !shape.match(clean).hasMatch()) {
        emit message(tr("Ce jeton Lichess ne semble pas valide : copiez-le tel que Lichess l’affiche."));
        return;
    }
    if (clean.isEmpty())
        settings_.remove("lichessToken");
    else
        settings_.setValue("lichessToken", clean);
    emit accountsChanged();
    emit message(clean.isEmpty() ? tr("Jeton Lichess retiré.")
                                 : tr("Jeton Lichess enregistré : le livre Lichess est accessible."));
}

bool Controller::hasLichessToken() const {
    return !lichessToken().isEmpty();
}

QString Controller::accountToken(const QString &user) const {
    for (const auto &value : settings_.value("onlineAccounts").toList()) {
        const auto account = value.toMap();
        if (account["site"] == "lichess" &&
            account["user"].toString().compare(user, Qt::CaseInsensitive) == 0)
            return account["token"].toString();
    }
    return {};
}

QString Controller::lichessToken() const {
    if (const auto own = settings_.value("lichessToken").toString(); !own.isEmpty())
        return own;
    for (const auto &value : settings_.value("onlineAccounts").toList())
        if (const auto token = value.toMap()["token"].toString(); !token.isEmpty())
            return token;
    return {};
}

void Controller::setAccountToken(int index, const QString &token) {
    auto list = settings_.value("onlineAccounts").toList();
    if (index < 0 || index >= list.size() || list[index].toMap()["site"] != "lichess") {
        emit message(tr("Choisissez un compte Lichess enregistré."), true);
        return;
    }
    const auto clean = token.trimmed();
    static const QRegularExpression shape("^[A-Za-z0-9_]{8,200}$");
    if (!clean.isEmpty() && !shape.match(clean).hasMatch()) {
        emit message(tr("Ce jeton Lichess ne semble pas valide : copiez-le tel que Lichess l’affiche."),
                     true);
        return;
    }
    auto account = list[index].toMap();
    if (clean.isEmpty())
        account.remove("token");
    else
        account["token"] = clean;
    list[index] = account;
    settings_.setValue("onlineAccounts", list);
    emit accountsChanged();
    emit message(clean.isEmpty() ? tr("Compte %1 déconnecté de Lichess.").arg(account["user"].toString())
                                 : tr("Jeton lié au compte %1.").arg(account["user"].toString()));
}

void Controller::connectLichess() {
    if (!lichessLogin_->start())
        return;
    QDesktopServices::openUrl(lichessLogin_->authorizeUrl());
    emit accountsChanged();
    emit message(tr("Lichess s’ouvre dans votre navigateur : autorisez Trait64, puis revenez ici."));
}

void Controller::cancelLichessLogin() {
    lichessLogin_->cancel();
    emit accountsChanged();
}

void Controller::previewSound(const QString &theme) {
    const int volume = settings_.value("soundVolume", 80).toInt();
    const t64::BoardSound sequence[] = {t64::BoardSound::Move, t64::BoardSound::Capture,
                                        t64::BoardSound::Check};
    for (int i = 0; i < 3; ++i)
        QTimer::singleShot(i * 350, this,
                           [theme, volume, sound = sequence[i]] { t64::playSound(sound, theme, volume); });
}
