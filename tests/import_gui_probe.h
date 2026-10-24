// SPDX-License-Identifier: AGPL-3.0-or-later
// Optional real-corpus qualification; never part of the normal fixture suite.
#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

int importGuiProbe(QGuiApplication &app, const QString &folder) {
    const auto args = app.arguments();
    const int inputIndex = args.indexOf("--benchmark-import") + 1;
    check(inputIndex > 0 && inputIndex < args.size(), "Missing benchmark PGN");
    const auto input = QFileInfo(args[inputIndex]).canonicalFilePath();
    check(!input.isEmpty() && QFileInfo(input).isFile(), "Benchmark PGN must exist");
    const int limitIndex = args.indexOf("--benchmark-games");
    bool valid = true;
    const qint64 limit = limitIndex < 0 ? 10000 : args.value(limitIndex + 1).toLongLong(&valid);
    check(valid && limit > 0 && limit % 250 == 0, "Benchmark limit must be a positive multiple of 250");
    QElapsedTimer elapsed;
    elapsed.start();
    QStringList failures;
    Controller controller(folder);
    controller.setPreference("importWorkers", 4);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("controller", &controller);
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    QObject::connect(&controller, &Controller::message, &app,
                     [&](const QString &text, bool error) {
                         if (error)
                             failures.append(text);
                     });
    engine.load(QUrl("qrc:/ui/Main.qml"));
    check(!engine.rootObjects().empty(), "Benchmark QML loads");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    check(window, "Benchmark window exists");
    window->resize(1440, 940);
    window->setProperty("page", 2);
    QTest::qWait(100);
    const double startupMs = elapsed.nsecsElapsed() / 1e6;
    std::vector<double> intervals;
    QTimer heartbeat, navigation;
    heartbeat.setTimerType(Qt::PreciseTimer);
    heartbeat.setInterval(10);
    QElapsedTimer tick;
    tick.start();
    QObject::connect(&heartbeat, &QTimer::timeout, &app, [&] {
        intervals.push_back(tick.nsecsElapsed() / 1e6);
        tick.restart();
    });
    int navigations = 0;
    bool navigationSucceeded = true;
    navigation.setInterval(1000);
    QObject::connect(&navigation, &QTimer::timeout, &app, [&] {
        const int page = ++navigations % 2 ? 0 : 2;
        window->setProperty("page", page);
        navigationSucceeded = navigationSucceeded && window->property("page").toInt() == page;
    });
    bool requestedPause = false, timedOut = false;
    elapsed.restart();
    heartbeat.start();
    navigation.start();
    controller.importResumable(input);
    while (controller.busy()) {
        QTest::qWait(10);
        if (!requestedPause && (controller.importProgress()["committed"].toLongLong() >= limit ||
                                elapsed.elapsed() > 1800000)) {
            timedOut = elapsed.elapsed() > 1800000;
            requestedPause = true;
            controller.cancelTask();
        }
    }
    const double importMs = elapsed.nsecsElapsed() / 1e6;
    heartbeat.stop();
    navigation.stop();
    // Include any final synchronous refresh even if it delayed the last tick.
    intervals.push_back(tick.nsecsElapsed() / 1e6);
    check(!timedOut, "GUI benchmark timed out");
    check(failures.isEmpty(), qPrintable(failures.join("; ")));
    check(!controller.importJobs().empty(), "GUI benchmark created an import job");
    const auto job = controller.importJobs().first().toMap();
    check(job["status"] == "paused" || job["status"] == "completed", "GUI import succeeded");
    check(!intervals.empty() && navigations > 0, "UI processed timer and navigation events during import");
    check(navigationSucceeded, "Page navigation remained usable during import");
    check(warnings.empty(), "No QML warnings during real-corpus import");
    std::sort(intervals.begin(), intervals.end());
    const auto stats = controller.stats();
    QJsonObject report{
        {"mode", "real Qt/QML window, offscreen software rendering"},
        {"input", input}, {"requested_games", limit},
        {"processed_games", job["processed"].toLongLong()},
        {"stored_games", stats["games"].toLongLong()},
        {"status", job["status"].toString()}, {"workers", 4},
        {"startup_ms", startupMs}, {"import_ms", importMs},
        {"timer_interval_ms", 10}, {"timer_samples", qint64(intervals.size())},
        {"timer_median_ms", intervals[intervals.size() / 2]},
        {"timer_p95_ms", intervals[(intervals.size() * 95 + 99) / 100 - 1]},
        {"timer_max_ms", intervals.back()}, {"page_navigations", navigations},
        {"qml_warnings", warnings.size()}, {"qt", QT_VERSION_STR}};
    std::cout << QJsonDocument(report).toJson().constData();
    return 0;
}
