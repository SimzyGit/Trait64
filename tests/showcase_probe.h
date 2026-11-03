// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
// `desktop_tests --showcase <folder>`: README screenshots from a realistic session
// (a commented game, several library games, the engine running when T64_STOCKFISH is set).

int showcaseProbe(const QString &directory, const QString &output) {
    QSettings().setValue("language", "fr");
    QSettings().setValue("lightTheme", true);
    Controller c(directory + "/showcase");
    if (!qEnvironmentVariable("T64_STOCKFISH").isEmpty()) {
        c.setPreference("enginePath", qEnvironmentVariable("T64_STOCKFISH"));
        c.setPreference("multiPV", 3);
    }
    c.importText(
        "[Event \"Open de Lyon\"]\n[Date \"2025.07.14\"]\n[White \"Moreau, Lucas\"]\n[Black \"Lefèvre, Camille\"]\n"
        "[WhiteElo \"2417\"]\n[BlackElo \"2384\"]\n[Result \"1-0\"]\n[ECO \"B90\"]\n\n"
        "1. e4 c5 2. Nf3 d6 3. d4 cxd4 4. Nxd4 Nf6 5. Nc3 a6 6. Be3 e5 (6... e6 7. f3 b5 8. Qd2 Nbd7 9. g4) 7. Nb3 Be6 "
        "8. f3 {Le plan f3, Dd2 et grand roque : les Blancs visent g4-g5 sur l'aile roi.} Be7 9. Qd2 O-O 10. O-O-O Nbd7 "
        "11. g4 b5 12. g5 Nh5 13. Nd5 Bxd5 14. exd5 f5 15. gxf6 Bxf6 16. Bh3 b4 17. Kb1 a5 1-0\n\n"
        "[Event \"Championnat régional\"]\n[White \"Durand, Inès\"]\n[Black \"Petit, Hugo\"]\n[WhiteElo \"2290\"]\n"
        "[BlackElo \"2311\"]\n[Result \"1/2-1/2\"]\n[Date \"2025.05.02\"]\n\n1. e4 c5 2. Nf3 d6 3. d4 cxd4 4. Nxd4 Nf6 5. Nc3 a6 "
        "6. Be3 e5 7. Nb3 Be6 8. Be2 Be7 9. O-O O-O 1/2-1/2\n\n"
        "[Event \"Ligue nationale\"]\n[White \"Garnier, Paul\"]\n[Black \"Roux, Emma\"]\n[WhiteElo \"2455\"]\n"
        "[BlackElo \"2402\"]\n[Result \"0-1\"]\n[Date \"2024.11.23\"]\n\n1. e4 c5 2. Nf3 d6 3. d4 cxd4 4. Nxd4 Nf6 5. Nc3 a6 "
        "6. Be3 e5 7. Nb3 Be6 8. f3 h5 9. Qd2 Nbd7 0-1\n\n"
        "[Event \"Open international\"]\n[White \"Blanc, Arthur\"]\n[Black \"Chevalier, Zoé\"]\n[WhiteElo \"2330\"]\n"
        "[BlackElo \"2356\"]\n[Result \"1/2-1/2\"]\n[Date \"2024.04.19\"]\n\n1. d4 d5 2. c4 e6 3. Nc3 Nf6 4. Bg5 Be7 1/2-1/2");
    QElapsedTimer wait;
    wait.start();
    while ((c.busy() || c.librarySearching()) && wait.elapsed() < 15000) QTest::qWait(20);
    qint64 id = 0;
    for (const auto &value : c.games())
        if (value.toMap()["white"] == "Moreau, Lucas") id = value.toMap()["id"].toLongLong();
    check(id > 0, "Showcase game imported");
    c.loadGame(id);
    // 7... Be6: main-line node 14.
    for (int ply = 0; ply < 14; ++ply) c.step(1);
    const int node = c.state()["node"].toInt();
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("controller", &c);
    engine.load(QUrl("qrc:/ui/Main.qml"));
    check(!engine.rootObjects().isEmpty(), "Showcase QML loads");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    window->resize(1440, 900);
    auto grab = [&](const QString &name) {
        QTest::qWait(300);
        auto image = window->contentItem()->grabToImage();
        QSignalSpy ready(image.data(), &QQuickItemGrabResult::ready);
        check(ready.wait(5000) && image->saveToFile(output + "/" + name + ".png"), "Showcase screenshot saved");
    };
    window->setProperty("page", 1);
    grab("library");
    window->setProperty("page", 2);
    c.navigate(node);
    if (!qEnvironmentVariable("T64_STOCKFISH").isEmpty()) {
        c.toggleAnalysis();
        QTest::qWait(4000);
    }
    grab("analysis");
    if (c.state()["analysis"].toBool()) c.toggleAnalysis();
    std::cout << "Showcase screenshots written to " << output.toStdString() << "\n";
    return 0;
}
