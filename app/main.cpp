// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("Trait64");
    QCoreApplication::setOrganizationName("Trait64");
    QCoreApplication::setApplicationVersion(T64_VERSION);
    // Les deux sessions de bureau nomment l'icône autrement : Wayland ne la trouve que
    // par le nom du fichier .desktop et ignore setWindowIcon, X11 fait l'inverse. Sans
    // ces deux lignes, la fenêtre porte l'icône générique quoi qu'on installe.
    QGuiApplication::setDesktopFileName("trait64");
    QGuiApplication::setWindowIcon(QIcon(":/assets/trait64.svg"));
    QQuickStyle::setStyle("Basic");
    const bool smoke = app.arguments().contains("--smoke-test");
    QTemporaryDir temporary;
    if (smoke) {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    }
    QString data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (smoke)
        data = temporary.path();
    QDir().mkpath(data);
    QLockFile lock(data + "/trait64.lock");
    if (!lock.tryLock(app.arguments().contains("--wait-for-unlock") ? 10000 : 0)) {
        qCritical() << "Trait64 est déjà ouvert pour ce dossier de données.";
        return 2;
    }
    try {
        Controller controller(data);
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("controller", &controller);
        bool qmlWarnings = false;
        QObject::connect(&engine, &QQmlEngine::warnings, &app,
                         [&](const QList<QQmlError> &) { qmlWarnings = true; });
        engine.load(QUrl("qrc:/ui/Main.qml"));
        if (engine.rootObjects().isEmpty())
            return 3;
        if (!smoke)
            controller.startBackgroundServices();
        if (smoke) {
            QString error;
            if (!controller.smokeTest(&error)) {
                qCritical() << error;
                return 4;
            }
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            const QString screenshot = qEnvironmentVariable("T64_SCREENSHOT");
            if (!screenshot.isEmpty() && qEnvironmentVariableIsSet("T64_SCREENSHOT_WIDTH"))
                window->resize(qEnvironmentVariableIntValue("T64_SCREENSHOT_WIDTH"),
                               qEnvironmentVariableIntValue("T64_SCREENSHOT_HEIGHT"));
            // Traverse every page so that QML delegates and lazy models are exercised.
            for (int i = 0; i < 7; ++i)
                QTimer::singleShot(150 + i * 170, &app, [window, i] { window->setProperty("page", i); });
            QTimer::singleShot(1500, &app, [window, &controller] {
                const int page = qEnvironmentVariableIsSet("T64_SCREENSHOT_PAGE")
                                     ? qEnvironmentVariableIntValue("T64_SCREENSHOT_PAGE")
                                     : 2;
                window->setProperty("page", qBound(0, page, 6));
                // « dark » manquait : on ne pouvait capturer que le thème clair, donc
                // vérifier la moitié de ce que l'on voulait voir.
                const auto theme = qEnvironmentVariable("T64_SCREENSHOT_THEME");
                if (theme == "light" || theme == "dark")
                    controller.setPreference("lightTheme", theme == "light");
                // Un nom finissant par « Menu » ouvre ce menu et surligne sa première
                // entrée : l'état survolé ne se voyait sur aucune capture, et c'est
                // justement là que le fond du menu peignait le texte en noir sur noir.
                const auto preview = qEnvironmentVariable("T64_PREVIEW");
                const bool menu = preview.endsWith("Menu");
                if (!preview.isEmpty())
                    if (auto *opened = window->findChild<QObject *>(menu ? preview : preview + "Dialog")) {
                        QMetaObject::invokeMethod(opened, "open");
                        if (menu)
                            opened->setProperty("currentIndex", 0);
                    }
            });
            QTimer::singleShot(2000, &app, [&, window, screenshot] {
                if (screenshot.isEmpty()) {
                    qInfo() << "Desktop smoke test completed";
                    app.exit(qmlWarnings ? 5 : 0);
                    return;
                }
                auto capture = window->contentItem()->grabToImage();
                if (!capture) {
                    qCritical() << "Unable to capture window";
                    app.exit(6);
                    return;
                }
                QObject::connect(capture.data(), &QQuickItemGrabResult::ready, &app,
                                 [&, capture, screenshot] {
                                     const bool saved = capture->saveToFile(screenshot);
                                     qInfo() << "Desktop capture saved:" << saved;
                                     app.exit(qmlWarnings ? 5 : saved ? 0 : 6);
                                 });
            });
        }
        return app.exec();
    } catch (const std::exception &e) {
        qCritical() << "Trait64:" << e.what();
        return 1;
    }
}
