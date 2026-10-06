// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
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
    QCoreApplication::setApplicationVersion("0.1.0");
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
    if (!lock.tryLock(0)) {
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
        if (smoke) {
            QString error;
            if (!controller.smokeTest(&error)) {
                qCritical() << error;
                return 4;
            }
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            const QString screenshot = qEnvironmentVariable("T64_SCREENSHOT");
            // Traverse every page so that QML delegates and lazy models are exercised.
            for (int i = 0; i < 6; ++i)
                QTimer::singleShot(150 + i * 170, &app, [window, i] { window->setProperty("page", i); });
            QTimer::singleShot(1400, &app, [window] { window->setProperty("page", 2); });
            QTimer::singleShot(1800, &app, [&, window, screenshot] {
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
