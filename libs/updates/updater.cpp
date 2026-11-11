// SPDX-License-Identifier: AGPL-3.0-or-later
#include "updater.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
namespace t64 {
static const QString repo = "SimzyGit/Trait64";
static const QString api = "https://api.github.com/repos/" + repo;
Updater::Updater(const QString &path, QObject *parent, QNetworkAccessManager *network)
    : QObject(parent), network_(network ? network : new QNetworkAccessManager(this)), dataPath_(path) {
    token_ = qEnvironmentVariable("T64_GITHUB_TOKEN");
    const auto appImage = qEnvironmentVariable("APPIMAGE");
    const auto executable = QCoreApplication::applicationFilePath();
    target_ = !appImage.isEmpty()                ? QFileInfo(appImage).absoluteFilePath()
              : executable == "/usr/bin/trait64" ? executable
                                                 : QDir::homePath() + "/.local/bin/trait64";
    QString debianRelease;
    QFile debian("/etc/debian_version");
    if (debian.open(QIODevice::ReadOnly))
        debianRelease = QString::fromLatin1(debian.readAll());
    const QString os =
#if defined(Q_OS_WIN)
        "windows";
#elif defined(Q_OS_MACOS)
        "macos";
#else
        "linux";
#endif
    const auto packaging = packagingFor(os, QSysInfo::currentCpuArchitecture(), !appImage.isEmpty(),
                                        executable == "/usr/bin/trait64", debianRelease);
    kind_ = packaging.kind;
    suffix_ = packaging.suffix;
    if (kind_ == "manual")
        target_.clear();
    connect(&auth_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        if (status_ != "authenticating")
            return;
        if (code != 0) {
            fail("Connexion GitHub CLI indisponible. Connectez gh à github.com ou renseignez un jeton de "
                 "session.");
            return;
        }
        token_ = QString::fromUtf8(auth_.readAllStandardOutput()).trimmed();
        if (token_.isEmpty()) {
            fail("GitHub CLI n’a pas fourni de jeton.");
            return;
        }
        status_ = "checking";
        emit changed();
        fetchPage();
    });
    connect(&auth_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (status_ == "authenticating")
            fail(
                "GitHub CLI est introuvable. Installez gh et connectez-le, ou utilisez un jeton de session.");
    });
    connect(&installer_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus exit) {
        if (status_ != "installing")
            return;
        if (code != 0 || exit != QProcess::NormalExit) {
            fail("Installation refusée ou interrompue. La sauvegarde reste disponible : " + backup_);
            return;
        }
        status_ = "installed";
        message_ = "Mise à jour installée. Redémarrez Trait64 pour l’utiliser.";
        emit changed();
    });
    connect(&installer_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (status_ == "installing")
            fail("Impossible de lancer l’installateur système (pkexec / apt-get).");
    });
}
Updater::~Updater() {
    ++epoch_;
    // QProcess destruction can emit signals after our QString members have been destroyed.
    disconnect(&auth_, nullptr, this, nullptr);
    disconnect(&installer_, nullptr, this, nullptr);
    if (reply_) {
        disconnect(reply_, nullptr, this, nullptr);
        reply_->abort();
        reply_->deleteLater();
    }
    if (auth_.state() != QProcess::NotRunning) {
        auth_.kill();
        auth_.waitForFinished(1000);
    }
}
QString Updater::channel() const {
    // Par défaut, le canal de la version installée. « stable » pour tout le monde ne
    // proposait jamais rien : toutes les versions publiées sont des préversions, et
    // selectRelease les écarte hors du canal correspondant. Qui a installé une
    // préversion en attend les suites ; qui tourne sur une version stable n'y est pas
    // ramené, puisque le réglage choisi par l'utilisateur garde la main.
    const auto installed = Version::parse(QCoreApplication::applicationVersion());
    const QString natural = !installed || installed->pre.isEmpty() ? QStringLiteral("stable")
                            : installed->pre.first() == "dev"      ? QStringLiteral("development")
                                                                   : QStringLiteral("beta");
    return settings_.value("updates/channel", natural).toString();
}
bool Updater::busy() const {
    return QStringList{"checking", "authenticating", "downloading", "installing"}.contains(status_);
}
QVariantMap Updater::state() const {
    return {{"status", status_},
            {"message", message_},
            {"busy", busy()},
            {"version", candidate_.version},
            {"current", QCoreApplication::applicationVersion()},
            {"notes", candidate_.notes},
            {"available", candidate_.valid()},
            {"progress", candidate_.size ? double(received_) / candidate_.size : 0.},
            {"channel", channel()},
            {"automatic", settings_.value("updates/automatic", true)},
            {"githubCli", settings_.value("updates/githubCli", false)},
            {"authenticated", !token_.isEmpty()},
            {"lastChecked", settings_.value("updates/lastChecked")},
            {"kind", kind_},
            // Vide là où aucun paquet n'est publié pour la machine. Trait64 n'installe
            // lui-même que sous Linux x86_64 ; ailleurs il télécharge et vérifie.
            {"packageSuffix", suffix_},
            {"target", target_},
            {"backup", backup_}};
}
void Updater::setAutomatic(bool v) {
    settings_.setValue("updates/automatic", v);
    emit changed();
}
void Updater::useGithubCli(bool v) {
    if (busy())
        return;
    settings_.setValue("updates/githubCli", v);
    if (!v)
        token_ = qEnvironmentVariable("T64_GITHUB_TOKEN");
    emit changed();
}
void Updater::setSessionToken(const QString &v) {
    if (busy())
        return;
    if (v.contains('\n') || v.contains('\r'))
        return;
    token_ = v.trimmed();
    emit changed();
}
void Updater::setChannel(const QString &c) {
    if (busy() || !QStringList{"stable", "beta", "development"}.contains(c))
        return;
    settings_.setValue("updates/channel", c);
    candidate_ = {};
    status_ = "idle";
    message_.clear();
    emit changed();
}
void Updater::startAutomaticCheck(int firstDelay, int interval) {
    if (settings_.value("updates/automatic", true).toBool())
        QTimer::singleShot(firstDelay, this, [this] { check(false); });
    // Une fois au démarrage ne suffit pas : un logiciel d'échecs reste ouvert des jours,
    // et une version publiée entre-temps n'était proposée qu'au lancement suivant. Le
    // réglage est relu à chaque passage — le couper prend effet sans redémarrer.
    auto *again = new QTimer(this);
    again->setInterval(interval);
    connect(again, &QTimer::timeout, this, [this] {
        // Une version déjà proposée et laissée de côté n'est pas redemandée : le
        // « plus tard » de l'utilisateur vaut jusqu'à son terme.
        if (settings_.value("updates/automatic", true).toBool() && status_ != "available")
            check(false);
    });
    again->start();
}
void Updater::fail(const QString &msg) {
    status_ = "error";
    message_ = msg;
    download_.reset();
    emit changed();
}
void Updater::check(bool manual) {
    if (busy() || status_ == "installed" || auth_.state() != QProcess::NotRunning)
        return;
    if (!manual && settings_.value("updates/snooze").toDateTime() > QDateTime::currentDateTimeUtc())
        return;
    candidate_ = {};
    releases_ = {};
    page_ = 1;
    manual_ = manual;
    ++epoch_;
    message_.clear();
    if (suffix_.isEmpty()) {
        fail("Pas de paquet compatible avec cette distribution ou cette architecture.");
        return;
    }
    if (token_.isEmpty() && settings_.value("updates/githubCli", false).toBool()) {
        status_ = "authenticating";
        auth_.start("gh", {"auth", "token", "--hostname", "github.com"});
        const auto e = epoch_;
        QTimer::singleShot(10000, this, [this, e] {
            if (e == epoch_ && status_ == "authenticating") {
                auth_.kill();
                fail("Délai de connexion GitHub dépassé.");
            }
        });
    } else {
        status_ = "checking";
        fetchPage();
    }
    emit changed();
}
void Updater::fetchPage() {
    request(QUrl(api + "/releases?per_page=100&page=" + QString::number(page_)), false);
}
void Updater::request(const QUrl &url, bool binary, int redirects) {
    if (!trustedUpdateUrl(url) || redirects > 8) {
        fail("Redirection de mise à jour non autorisée.");
        return;
    }
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setTransferTimeout(binary ? 120000 : 15000);
    req.setRawHeader("User-Agent", "Trait64-Updater");
    req.setRawHeader("Accept", binary ? "application/octet-stream" : "application/vnd.github+json");
    if (url.host() == "api.github.com") {
        req.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
        if (!token_.isEmpty())
            req.setRawHeader("Authorization", "Bearer " + token_.toUtf8());
    }
    auto *reply = network_->get(req);
    reply_ = reply;
    const auto e = epoch_;
    auto buffer = std::make_shared<QByteArray>();
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, buffer, binary, e] {
        if (e != epoch_)
            return;
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code != 200) {
            reply->readAll();
            return;
        }
        while (reply->bytesAvailable() > 0) {
            const auto bytes = reply->read(65536);
            if (binary) {
                received_ += bytes.size();
                if (!download_ || received_ > candidate_.size || download_->write(bytes) != bytes.size()) {
                    reply->abort();
                    fail("Téléchargement incomplet ou trop volumineux.");
                    return;
                }
            } else {
                buffer->append(bytes);
                if (buffer->size() > 4 * 1024 * 1024) {
                    reply->abort();
                    fail("Réponse GitHub trop volumineuse.");
                    return;
                }
            }
        }
        if (binary)
            emit changed();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, buffer, binary, redirects, e] {
        reply->deleteLater();
        if (e != epoch_ || status_ == "error")
            return;
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code >= 300 && code < 400) {
            const auto target =
                reply->url().resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
            request(target, binary, redirects + 1);
            return;
        }
        if (code == 401 || code == 404) {
            fail("Dépôt privé : activez votre connexion GitHub CLI ou fournissez un jeton ayant accès en "
                 "lecture au dépôt Trait64.");
            return;
        }
        if (code == 403 || code == 429) {
            fail("GitHub a refusé la requête ou sa limite est atteinte. Réessayez plus tard et vérifiez "
                 "votre accès au dépôt.");
            return;
        }
        if (code != 200 || reply->error() != QNetworkReply::NoError) {
            fail("Vérification/téléchargement impossible. Vérifiez la connexion et réessayez ; le logiciel "
                 "reste utilisable hors ligne.");
            return;
        }
        if (!binary) {
            fetched(*buffer);
            return;
        }
        if (!download_ || received_ != candidate_.size || !download_->commit()) {
            fail("Fichier incomplet : installation annulée.");
            return;
        }
        download_.reset();
        if (!verifyFile(downloadPath_, candidate_.sha256, candidate_.size)) {
            QFile::remove(downloadPath_);
            fail("Empreinte SHA-256 incorrecte : installation annulée.");
            return;
        }
        install();
    });
}
void Updater::fetched(const QByteArray &body) {
    QJsonParseError error;
    const auto json = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !json.isArray()) {
        fail("Réponse de versions GitHub invalide.");
        return;
    }
    const auto rows = json.array();
    for (auto r : rows)
        releases_.append(r);
    if (rows.size() == 100) {
        if (++page_ > 10) {
            fail("Trop de versions à parcourir. Vérification interrompue sans installation.");
            return;
        }
        fetchPage();
        return;
    }
    candidate_ = selectRelease(releases_, QCoreApplication::applicationVersion(), channel(), suffix_,
                               manual_ ? QStringList()
                                       : settings_.value("updates/ignored/" + channel()).toStringList());
    settings_.setValue("updates/lastChecked", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    status_ = candidate_.valid() ? "available" : "current";
    message_ = candidate_.valid() ? "Une nouvelle version est disponible."
                                  : "Aucune version plus récente compatible sur ce canal.";
    emit changed();
    if (candidate_.valid())
        emit offered();
}
void Updater::later() {
    if (busy())
        return;
    settings_.setValue("updates/snooze", QDateTime::currentDateTimeUtc().addSecs(24 * 3600));
    emit changed();
}
void Updater::ignore() {
    if (busy() || !candidate_.valid())
        return;
    auto ignored = settings_.value("updates/ignored/" + channel()).toStringList();
    ignored.append(candidate_.version);
    while (ignored.size() > 100)
        ignored.removeFirst();
    settings_.setValue("updates/ignored/" + channel(), ignored);
    candidate_ = {};
    status_ = "idle";
    message_ = "Version ignorée sur ce canal.";
    emit changed();
}
void Updater::cancel() {
    if (status_ == "installing" || status_ == "installed")
        return;
    ++epoch_;
    if (reply_)
        reply_->abort();
    auth_.kill();
    download_.reset();
    status_ = "idle";
    message_ = "Opération annulée.";
    emit changed();
}
void Updater::downloadAndInstall() {
    if (busy() || !candidate_.valid() || status_ == "installed")
        return;
    QDir dir(dataPath_ + "/updates");
    if (!dir.mkpath(".")) {
        fail("Dossier de téléchargement inaccessible.");
        return;
    }
    QFile::setPermissions(dir.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    downloadPath_ = dir.filePath(candidate_.name);
    download_ = std::make_unique<QSaveFile>(downloadPath_);
    download_->setDirectWriteFallback(false);
    if (!download_->open(QIODevice::WriteOnly)) {
        fail("Impossible de créer le fichier de mise à jour.");
        return;
    }
    received_ = 0;
    status_ = "downloading";
    message_ = "Téléchargement et vérification SHA-256…";
    ++epoch_;
    emit changed();
    request(QUrl(api + "/releases/assets/" + QString::number(candidate_.assetId)), true);
}
void Updater::install() {
    try {
        if (kind_ == "manual") {
            // Le fichier est téléchargé et vérifié ; le déployer relève du système, pas
            // de nous. On dit où il est plutôt que de laisser l'utilisateur le chercher.
            status_ = "installed";
            message_ = "Fichier téléchargé et vérifié : " + QDir::toNativeSeparators(downloadPath_) +
                       ". Installez-le vous-même, Trait64 ne le fait pas sur ce système.";
            emit changed();
            return;
        }
        if (!beforeInstall)
            throw std::runtime_error("La sauvegarde avant installation n’est pas configurée.");
        backup_ = beforeInstall();
        if (backup_.isEmpty())
            throw std::runtime_error("Sauvegarde non confirmée, installation annulée.");
        if (kind_ == "AppImage") {
            QDir().mkpath(QFileInfo(target_).absolutePath());
            replaceAppImage(downloadPath_, target_, candidate_.sha256, candidate_.size);
            status_ = "installed";
            message_ = "AppImage installée ; l’ancien exécutable est conservé avec le suffixe .previous.";
            emit changed();
            return;
        }
        QProcess inspect;
        inspect.start("/usr/bin/dpkg-deb", {"--field", downloadPath_, "Package", "Version", "Architecture"});
        if (!inspect.waitForFinished(5000) || inspect.exitCode() != 0)
            throw std::runtime_error("Paquet Debian illisible.");
        const auto fields = inspect.readAllStandardOutput();
        if (!fields.contains("Package: trait64\n") || !fields.contains("Architecture: amd64\n") ||
            !fields.contains("Version: " + debianVersion(candidate_.version).toUtf8() + "\n"))
            throw std::runtime_error("Identité du paquet Debian incorrecte.");
        status_ = "installing";
        message_ = "Autorisez l’installation dans la fenêtre système. Les données ont été sauvegardées.";
        emit changed();
        installer_.start("/usr/bin/pkexec",
                         {"/usr/bin/apt-get", "--assume-yes", "--no-remove", "install", downloadPath_});
    } catch (const std::exception &e) {
        fail(QString::fromUtf8(e.what()));
    }
}
void Updater::restart() {
    if (status_ != "installed")
        return;
    QProcess next;
    next.setProgram(target_);
    next.setArguments({"--wait-for-unlock"});
    auto env = QProcessEnvironment::systemEnvironment();
    for (const auto &key : QStringList{"APPIMAGE", "APPDIR", "LD_LIBRARY_PATH", "QT_PLUGIN_PATH",
                                       "QML2_IMPORT_PATH", "QML_IMPORT_PATH"})
        env.remove(key);
    next.setProcessEnvironment(env);
    if (next.startDetached())
        QCoreApplication::quit();
    else
        fail("Redémarrage impossible ; fermez puis relancez Trait64.");
}
} // namespace t64
