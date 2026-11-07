// SPDX-License-Identifier: AGPL-3.0-or-later
#include "updater.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSettings>
#include <QSignalSpy>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <iostream>
int checks = 0;
void check(bool value, const char *message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F f) {
    bool caught = false;
    try {
        f();
    } catch (const std::exception &) {
        caught = true;
    }
    check(caught, "Unsafe operation accepted");
}
struct Response {
    int status = 200;
    QByteArray body;
    QUrl redirect;
};
class Reply : public QNetworkReply {
  public:
    QByteArray bytes;
    qint64 offset = 0;
    Reply(const QNetworkRequest &request, Response response, QObject *parent)
        : QNetworkReply(parent), bytes(response.body) {
        setRequest(request);
        setUrl(request.url());
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response.status);
        if (!response.redirect.isEmpty())
            setAttribute(QNetworkRequest::RedirectionTargetAttribute, response.redirect);
        QTimer::singleShot(0, this, [this] {
            if (isFinished())
                return;
            emit readyRead();
            if (isFinished())
                return;
            setFinished(true);
            emit finished();
        });
    }
    void abort() override {
        if (isFinished())
            return;
        setError(OperationCanceledError, "Canceled");
        setFinished(true);
        emit finished();
    }
    qint64 bytesAvailable() const override { return bytes.size() - offset + QNetworkReply::bytesAvailable(); }

  protected:
    qint64 readData(char *data, qint64 max) override {
        auto n = std::min(max, bytes.size() - offset);
        if (!n)
            return -1;
        memcpy(data, bytes.constData() + offset, n);
        offset += n;
        return n;
    }
};
class Network : public QNetworkAccessManager {
  public:
    QList<Response> responses;
    QList<QNetworkRequest> requests;

  protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &r, QIODevice *) override {
        requests << r;
        if (responses.empty())
            return new Reply(r, {500, {}, {}}, this);
        return new Reply(r, responses.takeFirst(), this);
    }
};
// Le nom de la pièce jointe doit porter le suffixe de la machine qui joue le test :
// l'AppImage en dur ne disait rien à Windows, qui ne trouvait donc aucune mise à jour.
QJsonObject release(QString version, bool prerelease, QString hash = QString(64, 'a'),
                    QString suffix = "-linux-x86_64.AppImage") {
    return {{"tag_name", version},
            {"prerelease", prerelease},
            {"draft", false},
            {"published_at", "2026-10-10T00:00:00Z"},
            {"body", "Release notes"},
            {"assets", QJsonArray{QJsonObject{{"id", 42},
                                              {"name", "Trait64-" + version + suffix},
                                              {"state", "uploaded"},
                                              {"size", 100},
                                              {"digest", "sha256:" + hash}}}}};
}
void wait(t64::Updater &u) {
    for (int i = 0; i < 100 && u.state()["busy"].toBool(); ++i)
        QTest::qWait(10);
    check(!u.state()["busy"].toBool(), "Updater timed out");
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    app.setOrganizationName("Trait64Tests");
    app.setApplicationName("Updates");
    app.setApplicationVersion("1.0.0");
    QTemporaryDir dir;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    qunsetenv("T64_GITHUB_TOKEN");
    qunsetenv("APPIMAGE");
    try {
        check(t64::Version::parse("v1.2.10")->compare(*t64::Version::parse("1.2.9")) > 0,
              "Numeric semantic comparison");
        check(t64::Version::parse("1.2.0-beta.10")->compare(*t64::Version::parse("1.2.0-beta.2")) > 0,
              "Numeric prerelease comparison");
        check(t64::Version::parse("1.2.0")->compare(*t64::Version::parse("1.2.0-rc.1")) > 0,
              "Stable follows prerelease");
        for (auto text : {"1.2", "1.02.0", "1.2.0-beta.01", "1.2.0-dev..3", "1.2.0+bad..metadata", "garbage"})
            check(!t64::Version::parse(text), "Invalid version rejected");
        QJsonArray releases{release("v1.1.0", false), release("v1.2.0-beta.1", true),
                            release("v1.3.0-dev.3", true)};
        const QString suffix = "-linux-x86_64.AppImage";
        check(t64::selectRelease(releases, "1.0.0", "stable", suffix).version == "v1.1.0",
              "Stable excludes all prereleases");
        check(t64::selectRelease(releases, "1.0.0", "beta", suffix).version == "v1.2.0-beta.1",
              "Beta excludes development");
        check(t64::selectRelease(releases, "1.0.0", "development", suffix).version == "v1.3.0-dev.3",
              "Development chooses highest SemVer");
        check(!t64::selectRelease(releases, "2.0.0", "stable", suffix).valid(), "Never downgrade");
        check(!t64::selectRelease(releases, "1.0.0", "stable", suffix, {"v1.1.0"}).valid(),
              "Ignored release excluded");
        auto draft = release("v9.0.0", false);
        draft["draft"] = true;
        check(!t64::selectRelease({draft}, "1.0.0", "stable", suffix).valid(), "Draft not offered");
        check(!t64::selectRelease({release("v9.0.0", false, "")}, "1.0.0", "stable", suffix).valid(),
              "Missing digest not offered");
        check(!t64::trustedUpdateUrl(QUrl("https://api.github.com.evil.test/file")),
              "Deceptive host refused");
        check(!t64::trustedUpdateUrl(QUrl("http://github.com/file")), "TLS downgrade refused");
        check(!t64::trustedUpdateUrl(QUrl("https://user:pass@github.com/file")), "URL credentials refused");
        check(t64::trustedUpdateUrl(QUrl("https://release-assets.githubusercontent.com/file")),
              "GitHub asset redirect permitted");
        QByteArray binary(100, 'x');
        binary.replace(0, 4,
                       QByteArray("\x7f"
                                  "ELF",
                                  4));
        binary.replace(8, 3, QByteArray("AI\x02", 3));
        const auto hash =
            QString::fromLatin1(QCryptographicHash::hash(binary, QCryptographicHash::Sha256).toHex());
        QFile source(dir.path() + "/new.AppImage");
        source.open(QIODevice::WriteOnly);
        source.write(binary);
        source.close();
        QFile old(dir.path() + "/Trait64.AppImage");
        old.open(QIODevice::WriteOnly);
        old.write("old executable");
        old.close();
        rejects([&] { t64::replaceAppImage(source.fileName(), old.fileName(), QString(64, 'a'), 100); });
        old.open(QIODevice::ReadOnly);
        check(old.readAll() == "old executable", "Bad update preserves executable");
        old.close();
        t64::replaceAppImage(source.fileName(), old.fileName(), hash, 100);
        check(t64::verifyFile(old.fileName(), hash, 100), "Atomic replacement verified");
        QFile previous(old.fileName() + ".previous");
        check(previous.open(QIODevice::ReadOnly) && previous.readAll() == "old executable",
              "Previous executable preserved");
        {
            // Quel paquet pour quelle machine. Aucune de ces combinaisons n'est joignable
            // depuis une seule plateforme : c'est pourquoi la décision est une fonction.
            auto p = [](const char *os, const char *arch, bool appImage, bool systemPath,
                        const char *debian = "") {
                return t64::packagingFor(os, arch, appImage, systemPath, debian);
            };
            check(p("windows", "x86_64", false, false).suffix == "-windows-x86_64.zip" &&
                      p("windows", "x86_64", false, false).kind == "manual",
                  "Windows is offered its archive, to install by hand");
            check(p("macos", "arm64", false, false).suffix == "-macos-arm64.dmg",
                  "An Apple Silicon Mac is offered its disk image");
            check(p("macos", "x86_64", false, false).suffix.isEmpty(),
                  "No package is published for an Intel Mac, and none is claimed");
            check(p("linux", "x86_64", true, false).suffix == "-linux-x86_64.AppImage" &&
                      p("linux", "x86_64", true, false).kind == "AppImage",
                  "A running AppImage updates itself");
            check(p("linux", "x86_64", false, true, "12.9").suffix == "_debian12_amd64.deb",
                  "A system install on Debian 12 takes the Debian 12 package");
            check(p("linux", "x86_64", false, true, "13.1\n").suffix == "_debian13_amd64.deb",
                  "The release file is read past its minor number and its newline");
            check(p("linux", "x86_64", false, true, "trixie/sid").suffix.isEmpty(),
                  "A system install on another distribution is offered nothing rather than a foreign deb");
            check(p("linux", "aarch64", true, false).suffix.isEmpty(),
                  "No Linux package is published for arm64");
            check(p("linux", "x86_64", false, false).kind == "AppImage",
                  "An executable outside /usr/bin is treated as an AppImage install");
        }
        {
            // Toutes les versions publiées sont des préversions : avec « stable » par
            // défaut, aucune mise à jour n'était jamais proposée à personne.
            QSettings settings;
            settings.remove("updates/channel");
            const auto restore = QCoreApplication::applicationVersion();
            Network quiet;
            QCoreApplication::setApplicationVersion("0.3.0-dev.139");
            check(t64::Updater(dir.path(), nullptr, &quiet).state()["channel"] == "development",
                  "A development build looks for development updates by default");
            QCoreApplication::setApplicationVersion("1.2.0-beta.1");
            check(t64::Updater(dir.path(), nullptr, &quiet).state()["channel"] == "beta",
                  "A beta build looks for beta updates by default");
            QCoreApplication::setApplicationVersion("1.2.0");
            check(t64::Updater(dir.path(), nullptr, &quiet).state()["channel"] == "stable",
                  "A stable build stays on stable");
            // Le réglage choisi garde la main : on ne ramène personne sur un canal.
            settings.setValue("updates/channel", "development");
            check(t64::Updater(dir.path(), nullptr, &quiet).state()["channel"] == "development",
                  "A chosen channel outranks the default");
            settings.remove("updates/channel");
            QCoreApplication::setApplicationVersion(restore);
        }
        Network network;
        t64::Updater updater(dir.path(), nullptr, &network);
        updater.setSessionToken("test-only-token");
        QSignalSpy offers(&updater, &t64::Updater::offered);
        // La mise à jour automatique ne vise que les paquets publiés pour Linux x86_64.
        // Ailleurs — un Mac à puce Apple, par exemple — il n'y a rien à installer, et
        // c'est le refus qui est le bon comportement, pas l'offre d'une mise à jour.
        const bool packaged = !updater.state()["packageSuffix"].toString().isEmpty();
        if (!packaged) {
            updater.check();
            wait(updater);
            check(offers.isEmpty() && !updater.state()["available"].toBool() &&
                      updater.state()["status"] == "error",
                  "Without a package for this machine the updater refuses instead of offering one");
            std::cout << "Skipping update-delivery checks: no published package for "
                      << QSysInfo::currentCpuArchitecture().toStdString() << '\n';
        } else {
            const auto suffix = updater.state()["packageSuffix"].toString();
            // Windows et macOS téléchargent et vérifient, puis s'arrêtent : y déployer le
            // fichier relève du système. Seul Linux installe lui-même, et lui seul passe
            // par la sauvegarde.
            const bool installs = updater.state()["kind"].toString() != "manual";
            network.responses << Response{
                200, QJsonDocument(QJsonArray{release("v1.1.0", false, hash, suffix)}).toJson(), {}};
            updater.check();
            wait(updater);
            check(offers.size() == 1 && updater.state()["available"].toBool(),
                  "Real asynchronous check offers release");
            check(network.requests[0].rawHeader("Authorization") == "Bearer test-only-token",
                  "Private metadata authenticated");
            bool backed = false;
            updater.beforeInstall = [&]() -> QString {
                backed = true;
                throw std::runtime_error("Backup intentionally unavailable");
            };
            // Les refus d'abord : une fois le fichier téléchargé et remis, l'installateur
            // se tait, et plus rien ne s'éprouverait là où Trait64 n'installe pas lui-même.
            network.responses << Response{200, QByteArray(100, 'z'), {}};
            updater.downloadAndInstall();
            wait(updater);
            check(!backed && updater.state()["message"].toString().contains("SHA-256"),
                  "Tampering blocked before backup/installation");
            network.responses << Response{302, {}, QUrl("https://evil.test/file")};
            updater.downloadAndInstall();
            wait(updater);
            check(!backed && updater.state()["status"] == "error", "Untrusted redirect blocked");
            network.responses << Response{404, "{}", {}};
            updater.check();
            wait(updater);
            check(updater.state()["status"] == "error", "Private 404 is not reported as up to date");
            network.responses << Response{200, "not json", {}};
            updater.check();
            wait(updater);
            check(updater.state()["status"] == "error", "Malformed metadata rejected");
            network.responses << Response{200, "[]", {}};
            updater.check();
            updater.cancel();
            QTest::qWait(20);
            check(updater.state()["status"] == "idle", "Cancellation ignores stale callbacks");
            // La remise du fichier en dernier : là où Trait64 n'installe pas lui-même,
            // elle laisse l'installateur en « installé », qui refuse tout ce qui suit.
            network.responses << Response{
                200, QJsonDocument(QJsonArray{release("v1.1.0", false, hash, suffix)}).toJson(), {}};
            updater.check();
            wait(updater);
            network.responses << Response{302, {}, QUrl("https://release-assets.githubusercontent.com/test")}
                              << Response{200, binary, {}};
            updater.downloadAndInstall();
            wait(updater);
            check(installs ? (backed && updater.state()["status"] == "error")
                           : (!backed && updater.state()["status"] == "installed"),
                  installs ? "Backup failure blocks installation"
                           : "Without self-installation the file is downloaded, verified and handed over");
            check(network.requests.last().rawHeader("Authorization").isEmpty(),
                  "Token never forwarded to asset host");
        }
        QSettings persisted;
        check(!persisted.allKeys().join(' ').contains("token", Qt::CaseInsensitive),
              "Session token never persisted");
        check(t64::debianVersion("v0.3.0-beta.2") == "0.3.0~beta.2", "Debian prerelease ordering");
        // La connexion par GitHub CLI se teste avec un faux « gh » : un script shell, et
        // un PATH séparé par « : ». Les deux sont propres aux systèmes POSIX, donc ce
        // scénario ne dit rien de Windows et n'y est pas joué.
        // Ce scénario présuppose que la mise à jour ait quelque chose à installer : sans
        // paquet pour la machine, check() renonce avant même de chercher à s'authentifier,
        // et le faux « gh » ne démarre jamais. Le faux en question est par ailleurs un
        // script shell lancé via un PATH séparé par « : », propres aux systèmes POSIX.
#ifdef Q_OS_WIN
        std::cout << "Skipping GitHub CLI shutdown checks: the fixture is a POSIX shell script\n";
#else
        if (!packaged) {
            std::cout << "Skipping GitHub CLI shutdown checks: the updater has no package to install\n";
        } else {
            QTemporaryDir tools;
            QFile fake(tools.path() + "/gh");
            check(fake.open(QIODevice::WriteOnly), "Create isolated authentication fixture");
            fake.write("#!/bin/sh\nprintf ready > \"$T64_FAKE_AUTH_READY\"\nexec /bin/sleep 30\n");
            fake.close();
            check(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner),
                  "Make authentication fixture executable");
            const auto oldPath = qgetenv("PATH");
            const auto marker = tools.path() + "/ready";
            qputenv("PATH", tools.path().toUtf8() + ':' + oldPath);
            qputenv("T64_FAKE_AUTH_READY", marker.toUtf8());
            auto closing = std::make_unique<t64::Updater>(dir.path());
            closing->useGithubCli(true);
            bool destroying = false, lateSignal = false;
            QObject::connect(closing.get(), &t64::Updater::changed, &app, [&] {
                if (destroying)
                    lateSignal = true;
            });
            closing->check();
            for (int i = 0; i < 100 && !QFile::exists(marker); ++i)
                QTest::qWait(10);
            check(QFile::exists(marker), "Authentication process is running before shutdown");
            destroying = true;
            closing.reset();
            qputenv("PATH", oldPath);
            qunsetenv("T64_FAKE_AUTH_READY");
            check(!lateSignal, "Shutdown cannot call slots on partially destroyed updater members");
        }
#endif
        std::cout << checks << " update checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
