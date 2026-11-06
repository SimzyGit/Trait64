// SPDX-License-Identifier: AGPL-3.0-or-later
// Profils de joueur, à la manière d'En Croissant : un même utilisateur rassemble sous un
// seul nom ses comptes Lichess et Chess.com et son profil FIDE. Les comptes continuent
// d'être enregistrés comme avant et portent seulement le nom de leur profil : la
// synchronisation existante n'en sait rien et n'a pas à changer.
#include "controller.h"
#include <QDateTime>
#include <QDir>
#include <QImageReader>
#include <QUuid>
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
            require(i == index ||
                        list[i].toMap()["name"].toString().compare(wanted, Qt::CaseInsensitive) != 0,
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

void Controller::setProfilePhoto(int index, const QString &url) {
    try {
        auto list = settings_.value("profiles").toList();
        require(index >= 0 && index < list.size(), "Profil introuvable.");
        auto profile = list[index].toMap();
        const auto previous = profile["photo"].toString();
        if (url.trimmed().isEmpty()) {
            profile.remove("photo");
        } else {
            const auto source = localPath(url);
            QImageReader reader(source);
            // On lit les dimensions avant de décoder : une image de 20 000 pixels de côté
            // ne doit pas être dépliée en mémoire pour finir en vignette.
            require(reader.canRead(), "Image illisible.");
            const auto size = reader.size();
            require(size.isValid() && size.width() <= 8000 && size.height() <= 8000,
                    "Image trop grande (8000 pixels de côté au maximum).");
            reader.setScaledSize(size.scaled(256, 256, Qt::KeepAspectRatio));
            const auto image = reader.read();
            require(!image.isNull(), "Image illisible.");
            const auto folder = dataPath_ + "/avatars";
            require(QDir().mkpath(folder), "Dossier des portraits impossible à créer.");
            // Un nom neuf à chaque fois : QML garde l'ancienne image en cache tant que
            // l'adresse ne change pas, et le portrait ne se serait pas rafraîchi.
            const auto destination =
                folder + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".png";
            require(image.save(destination, "PNG"), "Enregistrement du portrait impossible.");
            profile["photo"] = destination;
        }
        // L'ancienne copie ne sert plus personne : la garder ferait grossir le dossier
        // de données à chaque changement de portrait.
        if (!previous.isEmpty() && previous.startsWith(dataPath_ + "/avatars/"))
            QFile::remove(previous);
        list[index] = profile;
        settings_.setValue("profiles", list);
        emit profilesChanged();
        emit message(url.trimmed().isEmpty() ? tr("Portrait retiré.") : tr("Portrait enregistré."));
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::setFideDetails(int index, const QString &fideId, const QString &fideName, int standard,
                                int rapid, int blitz, const QString &title, const QString &federation) {
    try {
        auto list = settings_.value("profiles").toList();
        require(index >= 0 && index < list.size(), "Profil introuvable.");
        const auto id = fideId.trimmed();
        // Un identifiant FIDE est un nombre ; on le refuse plutôt que d'enregistrer une
        // saisie qui ne désignera jamais personne.
        require(id.isEmpty() || (id.size() <= 12 &&
                                 std::all_of(id.cbegin(), id.cend(), [](QChar c) { return c.isDigit(); })),
                "L’identifiant FIDE ne contient que des chiffres.");
        auto rating = [](int value) {
            // 0 se lit « pas de classement dans cette cadence », ce qui est courant.
            return value <= 0 ? QVariant() : QVariant(std::clamp(value, 1000, 3000));
        };
        auto profile = list[index].toMap();
        profile["fideId"] = id;
        profile["fideName"] = fideName.simplified().left(100);
        // Saisissables à la main : télécharger la liste mensuelle pour le seul titre
        // coûterait des dizaines de mégaoctets. Une lecture de la liste les écrasera.
        profile["fideTitle"] = title.simplified().left(6).toUpper();
        profile["fideCountry"] = federation.simplified().left(3).toUpper();
        profile["fideStandard"] = rating(standard);
        profile["fideRapid"] = rating(rapid);
        profile["fideBlitz"] = rating(blitz);
        // Un chiffre saisi à la main est tenu pour voulu : il perd la marque de doute
        // qu'un téléchargement précédent aurait pu lui laisser.
        for (const auto &key : QStringList{"fideStandardStale", "fideRapidStale", "fideBlitzStale"})
            profile[key] = false;
        profile["fideUpdated"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        list[index] = profile;
        settings_.setValue("profiles", list);
        emit profilesChanged();
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::fetchFideRatings(int index, const QStringList &speeds) {
    try {
        auto list = settings_.value("profiles").toList();
        require(index >= 0 && index < list.size(), "Profil introuvable.");
        const auto id = list[index].toMap()["fideId"].toString();
        require(t64::FideList::validId(id), "Renseignez d’abord l’identifiant FIDE de ce profil.");
        require(!fide_ || !fide_->busy(), "Un téléchargement FIDE est déjà en cours.");
        if (!fide_) {
            fide_ = new t64::FideSync(this);
            connect(fide_, &t64::FideSync::progress, this,
                    [this](const QString &speed, int position, int count, qint64 received, qint64 total) {
                        fideSync_ = {{"busy", true},       {"speed", speed},       {"index", position},
                                     {"count", count},     {"received", received}, {"total", total},
                                     {"status", QString()}};
                        emit fideSyncChanged();
                    });
            connect(fide_, &t64::FideSync::failed, this, [this](const QString &text) {
                fideSync_ = {{"busy", false}, {"status", text}};
                emit fideSyncChanged();
                emit message(text, true);
            });
            connect(fide_, &t64::FideSync::finished, this, [this](const QVariantMap &player) {
                fideSync_ = {{"busy", false}, {"status", QString()}};
                emit fideSyncChanged();
                applyFideRatings(fideProfile_, fideSpeeds_, player);
            });
        }
        fideProfile_ = index;
        // Retenu pour distinguer, au retour, une cadence cherchée sans succès — donc
        // douteuse — d'une cadence simplement pas demandée, qui reste intacte.
        fideSpeeds_ = speeds;
        // Le dossier de données accueille les listes le temps de les lire ; elles sont
        // effacées aussitôt, car une liste complète ne sert à rien hors de cette lecture.
        QDir().mkpath(dataPath_ + "/downloads");
        fideSync_ = {{"busy", true}, {"status", QString()}};
        emit fideSyncChanged();
        fide_->fetch(id, speeds, dataPath_ + "/downloads");
    } catch (const std::exception &e) {
        fail(e);
    }
}

void Controller::cancelFideRatings() {
    if (fide_)
        fide_->cancel();
}

void Controller::applyFideRatings(int index, const QStringList &requested, const QVariantMap &player) {
    try {
        auto list = settings_.value("profiles").toList();
        require(index >= 0 && index < list.size(), "Profil introuvable.");
        if (!player["found"].toBool()) {
            emit message(tr("Ce numéro FIDE ne figure dans aucune des listes téléchargées."), true);
            return;
        }
        auto profile = list[index].toMap();
        // Le nom publié par la fédération remplace une saisie approximative, mais on ne
        // vide pas un champ que la liste ne fournit pas.
        for (const auto &key : QStringList{"name", "country", "title"})
            if (!player[key].toString().isEmpty())
                profile["fide" + key.mid(0, 1).toUpper() + key.mid(1)] = player[key];
        for (const auto &speed : QStringList{"standard", "rapid", "blitz"}) {
            const auto key = "fide" + speed.mid(0, 1).toUpper() + speed.mid(1);
            if (player.contains(speed)) {
                profile[key] = player[speed];
                profile[key + "Stale"] = false;
                continue;
            }
            // Cadence cherchée sans y trouver le joueur : le chiffre connu est conservé
            // mais marqué douteux, car il n'est plus confirmé par la liste du mois. Une
            // cadence qui n'a pas été demandée, elle, n'apprend rien et reste intacte.
            if (requested.contains(speed) && profile.value(key).isValid())
                profile[key + "Stale"] = true;
        }
        profile["fideUpdated"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        list[index] = profile;
        settings_.setValue("profiles", list);
        emit profilesChanged();
        emit message(tr("Classements FIDE à jour pour %1.").arg(profile["name"].toString()));
    } catch (const std::exception &e) {
        fail(e);
    }
}

QStringList Controller::profileAccountNames(int profileIndex) const {
    const auto all = profiles();
    if (profileIndex < 0 || profileIndex >= all.size())
        return {};
    // Les parties sont rangées sous le pseudo du site : un profil mesure donc ce que ses
    // comptes rattachés ont joué, et rien d'autre.
    QStringList names;
    for (const auto &value : all[profileIndex].toMap()["accounts"].toList())
        names << value.toMap()["user"].toString();
    names.removeAll(QString());
    names.removeDuplicates();
    return names;
}
QVariantMap Controller::skillDashboard(int profileIndex) const {
    try {
        const auto all = profiles();
        if (profileIndex < 0 || profileIndex >= all.size())
            // Un code plutôt qu'une phrase : ces trois cas se produisent sur une
            // installation neuve, et l'interface doit pouvoir les dire dans sa langue.
            return {{"ready", false}, {"reason", "noProfile"}};
        const auto profile = all[profileIndex].toMap();
        const auto names = profileAccountNames(profileIndex);
        if (names.isEmpty())
            return {{"ready", false}, {"reason", "noAccount"}, {"name", profile["name"]}};
        const auto summary = db_->reviewSummary(names);
        if (summary["games"].toInt() == 0)
            return {{"ready", false}, {"reason", "noReview"}, {"name", profile["name"]}, {"accounts", names}};
        auto history = db_->reviewHistory(names, 13);
        // Comparaison à soi-même : le mois le plus récent contre la moyenne, pondérée par
        // les coups, des mois antérieurs. Sans passé, il n'y a rien à comparer, et un
        // écart inventé vaudrait moins que pas d'écart du tout.
        QVariant trend;
        if (history.size() >= 2) {
            const auto latest = history.first().toMap();
            double weighted = 0;
            qint64 moves = 0;
            for (int i = 1; i < history.size(); ++i) {
                const auto row = history[i].toMap();
                if (row["accuracy"].isValid()) {
                    weighted += row["accuracy"].toDouble() * row["moves"].toDouble();
                    moves += row["moves"].toLongLong();
                }
            }
            if (moves > 0 && latest["accuracy"].isValid())
                trend = qRound(10 * (latest["accuracy"].toDouble() - weighted / double(moves))) / 10.;
        }
        auto result = summary;
        result["ready"] = true;
        result["name"] = profile["name"];
        result["accounts"] = names;
        result["history"] = history;
        result["trend"] = trend;
        // Les fautes se lisent par cent coups : un total brut ne dit rien tant qu'on ne
        // sait pas sur combien de coups il porte.
        const auto moves = summary["moves"].toLongLong();
        auto rate = [moves](qint64 count) {
            return moves > 0 ? QVariant(qRound(1000.0 * double(count) / double(moves)) / 10.) : QVariant();
        };
        result["blunderRate"] = rate(summary["blunders"].toLongLong());
        result["mistakeRate"] = rate(summary["mistakes"].toLongLong());
        result["inaccuracyRate"] = rate(summary["inaccuracies"].toLongLong());
        // La tactique ne vient pas des parties mais des exercices résolus : c'est la
        // seule mesure directe d'une compétence, les parties n'en donnant que le reflet.
        result["tactics"] = db_->puzzleStats();
        return result;
    } catch (const std::exception &e) {
        return {{"ready", false}, {"reason", QString::fromUtf8(e.what())}};
    }
}

QVariantMap Controller::repertoireGap(qint64 repertoireId, int profileIndex) const {
    try {
        const auto all = profiles();
        if (profileIndex < 0 || profileIndex >= all.size())
            return {{"ready", false}, {"reason", "noProfile"}};
        auto names = profileAccountNames(profileIndex);
        // Le nom du profil compte aussi : les parties jouées sur l'échiquier portent
        // celui-là, pas un pseudo de site.
        const auto own = all[profileIndex].toMap()["name"].toString().trimmed();
        if (!own.isEmpty() && !names.contains(own, Qt::CaseInsensitive))
            names << own;
        if (names.isEmpty())
            return {{"ready", false}, {"reason", "noAccount"}};
        auto gap = db_->repertoireGap(repertoireId, names);
        gap["ready"] = true;
        return gap;
    } catch (const std::exception &e) {
        return {{"ready", false}, {"reason", QString::fromUtf8(e.what())}};
    }
}
