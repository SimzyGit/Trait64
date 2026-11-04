// SPDX-License-Identifier: AGPL-3.0-or-later
// Profils de joueur, à la manière d'En Croissant : un même utilisateur rassemble sous un
// seul nom ses comptes Lichess et Chess.com et son profil FIDE. Les comptes continuent
// d'être enregistrés comme avant et portent seulement le nom de leur profil : la
// synchronisation existante n'en sait rien et n'a pas à changer.
#include "controller.h"
#include <QDateTime>
#include <algorithm>

namespace {
void require(bool ok, const QString &error) {
    if (!ok)
        throw std::runtime_error(error.toStdString());
}
// Un nom de profil sert de clé pour rattacher les comptes : il doit rester lisible et
// stable, donc ni vide, ni fait d'espaces, ni interminable.
QString cleanName(const QString &name) {
    return name.simplified().left(60);
}
} // namespace

QVariantList Controller::profiles() const {
    const auto saved = settings_.value("profiles").toList();
    const auto accounts = settings_.value("onlineAccounts").toList();
    QVariantList result;
    for (const auto &value : saved) {
        auto profile = value.toMap();
        // Les comptes rattachés sont recomposés à la lecture : une seule source de
        // vérité, donc aucun risque de voir les deux listes se contredire.
        QVariantList linked;
        for (int i = 0; i < accounts.size(); ++i) {
            const auto account = accounts[i].toMap();
            if (account["profile"].toString() == profile["name"].toString())
                linked << QVariantMap{{"index", i}, {"site", account["site"]}, {"user", account["user"]}};
        }
        profile["accounts"] = linked;
        result << profile;
    }
    return result;
}

void Controller::addProfile(const QString &name) {
    try {
        const auto wanted = cleanName(name);
        require(!wanted.isEmpty(), "Donnez un nom à ce profil.");
        auto list = settings_.value("profiles").toList();
        require(list.size() < 32, "32 profils au maximum.");
        for (const auto &value : list)
            require(value.toMap()["name"].toString().compare(wanted, Qt::CaseInsensitive) != 0,
                    "Ce profil existe déjà.");
        list << QVariantMap{{"name", wanted}};
        settings_.setValue("profiles", list);
        emit profilesChanged();
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::renameProfile(int index, const QString &name) {
    try {
        const auto wanted = cleanName(name);
        require(!wanted.isEmpty(), "Donnez un nom à ce profil.");
        auto list = settings_.value("profiles").toList();
        require(index >= 0 && index < list.size(), "Profil introuvable.");
        const auto previous = list[index].toMap()["name"].toString();
        if (previous == wanted)
            return;
        for (int i = 0; i < list.size(); ++i)
            require(i == index || list[i].toMap()["name"].toString().compare(wanted, Qt::CaseInsensitive) != 0,
                    "Ce profil existe déjà.");
        auto profile = list[index].toMap();
        profile["name"] = wanted;
        list[index] = profile;
        settings_.setValue("profiles", list);
        // Les comptes désignent leur profil par son nom : les suivre évite qu'un
        // renommage les détache en silence.
        auto accounts = settings_.value("onlineAccounts").toList();
        bool moved = false;
        for (int i = 0; i < accounts.size(); ++i) {
            auto account = accounts[i].toMap();
            if (account["profile"].toString() != previous)
                continue;
            account["profile"] = wanted;
            accounts[i] = account;
            moved = true;
        }
        if (moved)
            settings_.setValue("onlineAccounts", accounts);
        emit profilesChanged();
        emit accountsChanged();
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::removeProfile(int index) {
    try {
        auto list = settings_.value("profiles").toList();
        require(index >= 0 && index < list.size(), "Profil introuvable.");
        const auto name = list[index].toMap()["name"].toString();
        list.removeAt(index);
        settings_.setValue("profiles", list);
        // Retirer un profil ne retire ni les comptes ni les parties : ils redeviennent
        // simplement non rattachés, et restent synchronisables comme avant.
        auto accounts = settings_.value("onlineAccounts").toList();
        bool detached = false;
        for (int i = 0; i < accounts.size(); ++i) {
            auto account = accounts[i].toMap();
            if (account["profile"].toString() != name)
                continue;
            account.remove("profile");
            accounts[i] = account;
            detached = true;
        }
        if (detached)
            settings_.setValue("onlineAccounts", accounts);
        emit profilesChanged();
        emit accountsChanged();
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::setAccountProfile(int accountIndex, const QString &profileName) {
    try {
        auto accounts = settings_.value("onlineAccounts").toList();
        require(accountIndex >= 0 && accountIndex < accounts.size(), "Compte introuvable.");
        const auto wanted = cleanName(profileName);
        if (!wanted.isEmpty()) {
            bool known = false;
            for (const auto &value : settings_.value("profiles").toList())
                known |= value.toMap()["name"].toString() == wanted;
            require(known, "Profil introuvable.");
        }
        auto account = accounts[accountIndex].toMap();
        if (wanted.isEmpty())
            account.remove("profile");
        else
            account["profile"] = wanted;
        accounts[accountIndex] = account;
        settings_.setValue("onlineAccounts", accounts);
        emit profilesChanged();
        emit accountsChanged();
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::setFideDetails(int index, const QString &fideId, const QString &fideName, int standard,
                                int rapid, int blitz) {
    try {
        auto list = settings_.value("profiles").toList();
        require(index >= 0 && index < list.size(), "Profil introuvable.");
        const auto id = fideId.trimmed();
        // Un identifiant FIDE est un nombre ; on le refuse plutôt que d'enregistrer une
        // saisie qui ne désignera jamais personne.
        require(id.isEmpty() || (id.size() <= 12 && std::all_of(id.cbegin(), id.cend(), [](QChar c) {
                    return c.isDigit();
                })), "L’identifiant FIDE ne contient que des chiffres.");
        auto rating = [](int value) {
            // 0 se lit « pas de classement dans cette cadence », ce qui est courant.
            return value <= 0 ? QVariant() : QVariant(std::clamp(value, 1000, 3000));
        };
        auto profile = list[index].toMap();
        profile["fideId"] = id;
        profile["fideName"] = fideName.simplified().left(100);
        profile["fideStandard"] = rating(standard);
        profile["fideRapid"] = rating(rapid);
        profile["fideBlitz"] = rating(blitz);
        profile["fideUpdated"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        list[index] = profile;
        settings_.setValue("profiles", list);
        emit profilesChanged();
    } catch (const std::exception &e) {
        fail(e);
    }
}
