// SPDX-License-Identifier: AGPL-3.0-or-later
// Le parcours des études conduit avec de vrais clics : choisir un dossier, déplier,
// lire un fichier, parcourir ses parties dans l'aperçu. Les tests du moteur disaient
// que tout marchait ; seul un clic dans la vraie fenêtre dit ce que l'utilisateur voit.
#pragma once

namespace explorerProbe {
inline QQuickItem *named(QQuickWindow *window, const QString &name) {
    return visualItem(window->contentItem(), name);
}
// Le premier descendant visible dont le texte contient `text`.
inline QQuickItem *withText(QQuickItem *root, const QString &text) {
    if (!root->isVisible())
        return nullptr;
    if (root->property("text").toString().contains(text) && root->width() > 0)
        return root;
    for (auto *child : root->childItems())
        if (auto *found = withText(child, text))
            return found;
    return nullptr;
}
inline void click(QQuickWindow *window, QQuickItem *item, int count = 1) {
    check(item && item->isVisible(), "Clicked item exists and is visible");
    const auto at = item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
    for (int i = 0; i < count; ++i) {
        if (i)
            QTest::mouseDClick(window, Qt::LeftButton, Qt::NoModifier, at);
        else
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, at);
    }
    QTest::qWait(count > 1 ? 150 : 450); // un simple clic attend l'écart du double
}
// QTest::mouseMove envoie un déplacement sans bouton enfoncé : ce n'est pas un glisser.
inline void drag(QQuickWindow *window, const QPoint &to) {
    QMouseEvent move(QEvent::MouseMove, QPointF(to), QPointF(window->mapToGlobal(to)), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &move);
    QTest::qWait(15);
}
// keyClicks n'existe que pour les widgets : on frappe lettre à lettre.
inline void type(QQuickWindow *window, const QString &text) {
    for (const auto c : text)
        QTest::keyClick(window, c.toLatin1());
}
inline void shot(QQuickWindow *window, const QString &name) {
    const auto dir = qEnvironmentVariable("T64_PROBE_SHOTS");
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);
    auto grab = window->contentItem()->grabToImage();
    QSignalSpy ready(grab.data(), &QQuickItemGrabResult::ready);
    if (ready.wait(3000))
        grab->saveToFile(dir + "/" + name + ".png");
}
// Les pièces dessinées par un échiquier : ses cases dont le modèle porte une pièce.
inline int pieces(QQuickItem *board) {
    int count = 0;
    // « shown » est ce que l'échiquier affiche vraiment, case par case.
    for (const auto &code : board->property("shown").toList())
        count += code.toString().isEmpty() ? 0 : 1;
    return count;
}
} // namespace explorerProbe

int explorerProbeMain(const QString &home) {
    using namespace explorerProbe;
    QTemporaryDir files;
    const auto folder = files.path() + "/etudes";
    check(QDir().mkpath(folder + "/Ouvertures"), "Create the study folder");
    const auto write = [](const QString &path, const QByteArray &pgn) {
        QFile file(path);
        check(file.open(QIODevice::WriteOnly) && file.write(pgn) == pgn.size(), "Write a study file");
    };
    write(folder + "/Ouvertures/Italienne.pgn",
          "[Event \"A\"]\n[White \"Anna\"]\n[Black \"Boris\"]\n[Result \"1-0\"]\n\n1. e4 e5 2. Nf3 Nc6 3. "
          "Bc4 Bc5 1-0\n\n"
          "[Event \"B\"]\n[White \"Clara\"]\n[Black \"David\"]\n[Result \"0-1\"]\n\n1. d4 d5 2. c4 e6 0-1\n");
    write(folder + "/Parties.pgn",
          "[Event \"C\"]\n[White \"Eve\"]\n[Black \"Fred\"]\n[Result \"*\"]\n\n1. c4 e5 *\n");

    Controller controller(home);
    controller.setPreference("language", "fr");
    controller.setPreference("studyFolder", folder);
    QStringList errors;
    QObject::connect(&controller, &Controller::message, &controller, [&](const QString &text, bool error) {
        if (error)
            errors << text;
    });
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("controller", &controller);
    QStringList warnings;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError> &list) {
        for (const auto &one : list)
            warnings << one.toString();
    });
    engine.load(QUrl("qrc:/ui/Main.qml"));
    check(!engine.rootObjects().empty(), "QML loads");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    check(window, "Window exists");
    window->resize(1440, 900);
    window->show();
    const auto idle = [&] {
        QElapsedTimer timer;
        timer.start();
        while ((controller.busy() || controller.librarySearching()) && timer.elapsed() < 30000)
            QTest::qWait(30);
        QTest::qWait(250);
    };

    // 1. La page Études s'ouvre sur l'explorateur quand la base est vide.
    window->setProperty("page", 8);
    QTest::qWait(400);
    shot(window, "1-etudes");
    auto *tree = named(window, "explorerEntries");
    check(tree && tree->isVisible(), "With no study yet, the page opens on the explorer");
    check(tree->property("count").toInt() == 2, "The tree lists the folder and the file at the root");

    // 2. Un clic déplie le dossier.
    click(window, withText(tree, "Ouvertures"));
    shot(window, "2-deplie");
    check(tree->property("count").toInt() == 3 && withText(tree, "Italienne.pgn"),
          "Clicking a folder unfolds it in place");

    // 3. Un clic choisit le fichier, « Lire ce fichier » l'importe et montre son recueil.
    click(window, withText(tree, "Italienne.pgn"));
    auto *open = named(window, "explorerOpen");
    check(open && open->isVisible() && open->property("text").toString() == "Lire ce fichier",
          "Selecting an unread file offers to read it");
    click(window, open);
    idle();
    shot(window, "3-lu");
    auto *games = named(window, "studyGames");
    check(games && games->isVisible() && games->property("count").toInt() == 2,
          "Reading a file shows its collection and its two games, without another click");

    // 4. Un clic sur une partie la montre dans l'aperçu : échiquier, pièces, coups.
    click(window, withText(games, "Anna"));
    shot(window, "4-apercu");
    auto *preview = named(window, "studyPreview");
    check(preview && preview->isVisible(), "The preview pane is there beside the games");
    auto *board = visualItem(preview, "previewBoard");
    check(board && board->isVisible() && board->width() > 100 && pieces(board) == 32,
          "Clicking a game shows its starting position on the preview board");
    click(window, visualItem(preview, "previewForward"));
    check(preview->property("step").toInt() == 1 && pieces(board) == 32,
          "The forward button steps through the game on the preview board");

    // 5. Double clic : la partie s'ouvre dans Analyse, avec sa position.
    click(window, withText(games, "Clara"), 2);
    idle();
    shot(window, "5-analyse");
    check(window->property("page").toInt() == 2 && controller.state()["title"].toString().contains("Clara"),
          "Double-clicking a game opens it in Analysis");
    auto *main = named(window, "chessboard");
    check(main && main->isVisible() && pieces(main) == 32, "The analysis board shows the position");

    // 6. La bibliothèque : un clic sur une ligne montre la partie dans l'aperçu.
    window->setProperty("page", 1);
    idle();
    auto *results = named(window, "libraryResults");
    check(results && results->property("count").toInt() >= 2, "The library lists the imported games");
    click(window, withText(results, "Anna"));
    shot(window, "6-bibliotheque");
    auto *libraryPreview = named(window, "libraryPreview");
    auto *libraryBoard = libraryPreview ? visualItem(libraryPreview, "previewBoard") : nullptr;
    check(libraryBoard && libraryBoard->isVisible() && pieces(libraryBoard) == 32,
          "Clicking a library row shows its position on the preview board");

    // 7. Retour aux études : le fichier lu est marqué, et un nouveau fichier se crée.
    window->setProperty("page", 8);
    QTest::qWait(300);
    shot(window, "7-retour");
    tree = named(window, "explorerEntries");
    check(withText(tree, "2 partie(s)"), "Back on the explorer, the file that was read says so");

    // 8. Créer un fichier par le dialogue : le nom tapé, Entrée, et il apparaît choisi.
    click(window, withText(tree, "Ouvertures")); // replie, et choisit le dossier comme cible
    click(window, withText(tree, "Ouvertures")); // redéplie
    click(window, named(window, "explorerNewFile"));
    type(window, "Sicilienne");
    QTest::keyClick(window, Qt::Key_Return);
    QTest::qWait(300);
    shot(window, "8-cree");
    check(QFileInfo(folder + "/Ouvertures/Sicilienne.pgn").size() > 40 && withText(tree, "Sicilienne.pgn"),
          "Typing a name and pressing Enter creates the file in the chosen folder and lists it");

    // 9. Le renommer depuis sa fiche.
    click(window, withText(tree, "Sicilienne.pgn"));
    click(window, withText(window->contentItem(), "Renommer"));
    QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
    type(window, "Najdorf");
    QTest::keyClick(window, Qt::Key_Return);
    QTest::qWait(300);
    shot(window, "9-renomme");
    check(QFileInfo::exists(folder + "/Ouvertures/Najdorf.pgn") &&
              !QFileInfo::exists(folder + "/Ouvertures/Sicilienne.pgn") && withText(tree, "Najdorf.pgn"),
          "Renaming from the card renames the file on disk and in the tree");

    // 10. La corbeille demande confirmation ; « Annuler » ne touche à rien.
    click(window, named(window, "explorerTrash"));
    shot(window, "10-corbeille");
    auto *cancel = withText(window->contentItem(), "Annuler");
    if (!cancel)
        cancel = withText(window->contentItem(), "Cancel");
    click(window, cancel);
    check(QFileInfo::exists(folder + "/Ouvertures/Najdorf.pgn"),
          "Cancelling the trash dialog deletes nothing");

    // 11. La recherche filtre l'arbre.
    click(window, named(window, "explorerSearch"));
    type(window, "parties");
    QTest::qWait(300);
    shot(window, "11-recherche");
    check(tree->property("count").toInt() == 1 && withText(tree, "Parties.pgn"),
          "Typing in the search field narrows the tree to what matches");
    if (!errors.isEmpty())
        qWarning() << "messages d'erreur" << errors;
    if (!warnings.isEmpty())
        qWarning() << "avertissements QML" << warnings;
    check(errors.isEmpty(), "The whole walk raises no error message");
    check(warnings.isEmpty(), "The whole walk raises no QML warning");
    std::cout << checks << " explorer walk checks passed\n";
    return 0;
}

// L'échiquier à vrais gestes : glisser une pièce, cliquer deux cases, et ce que coûte
// l'affichage d'un coup.
int boardProbeMain(const QString &home) {
    using namespace explorerProbe;
    Controller controller(home);
    controller.setPreference("language", "fr");
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("controller", &controller);
    QStringList warnings;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError> &list) {
        for (const auto &one : list)
            warnings << one.toString();
    });
    engine.load(QUrl("qrc:/ui/Main.qml"));
    check(!engine.rootObjects().empty(), "QML loads");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    window->resize(1440, 900);
    window->show();
    window->setProperty("page", 2);
    QTest::qWait(500);
    auto *board = named(window, "chessboard");
    check(board && board->isVisible() && board->width() > 200, "The analysis board is on screen");
    const auto centre = [&](const QString &square) {
        const int file = square[0].unicode() - 'a', rank = square[1].unicode() - '1';
        const qreal cell = board->width() / 8;
        const bool flipped = controller.state()["flipped"].toBool();
        const qreal x = (flipped ? 7 - file : file) * cell + cell / 2,
                    y = (flipped ? rank : 7 - rank) * cell + cell / 2;
        return board->mapToScene(QPointF(x, y)).toPoint();
    };
    const auto fen = [&] { return controller.state()["fen"].toString().section(' ', 0, 0); };

    // 1. Glisser : appuyer sur e2, traîner, lâcher sur e4.
    const auto from = centre("e2"), to = centre("e4");
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from);
    for (int i = 1; i <= 8; ++i)
        drag(window, from + (to - from) * i / 8);
    shot(window, "b1-glisse");
    check(board->property("draggedFrom").toString() == "e2", "Dragging lifts the piece off its square");
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, to);
    const auto sliding = [&] { return !board->property("hidden").toMap().isEmpty(); };
    check(!sliding(), "A piece dropped by hand is already there: it does not slide again");
    QTest::qWait(400);
    shot(window, "b2-pose");
    check(fen() == "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR", "Dropping it on e4 plays e2-e4");
    check(board->property("draggedFrom").toString().isEmpty(), "The piece is put down");

    // 2. Lâcher sur une case interdite : rien ne bouge, et la pièce revient.
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, centre("g8"));
    drag(window, centre("g6"));
    drag(window, centre("g5"));
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, centre("g5"));
    QTest::qWait(300);
    check(fen() == "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR" &&
              board->property("draggedFrom").toString().isEmpty(),
          "An illegal drop changes nothing and puts the piece back");

    // 3. Deux clics : la case de départ, puis celle d'arrivée.
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, centre("e7"));
    QTest::qWait(150);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, centre("e5"));
    check(sliding(), "A clicked move slides the piece to its square");
    QTest::qWait(400);
    check(!sliding(), "The slide ends and the piece rests on its square");
    check(fen() == "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR", "Two clicks play a move as well");

    // 3 bis. Variantes au clavier, lecture automatique, flèches du moteur.
    controller.navigate(0);
    controller.move("d2d4");
    const int alternative = controller.state()["node"].toInt();
    controller.stepVariation(-1);
    check(fen() == "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR", "Up goes to the previous variation");
    controller.stepVariation(-1);
    controller.stepVariation(1);
    check(controller.state()["node"].toInt() == alternative, "Down comes back, and neither runs off the end");
    controller.navigate(0);
    QTest::qWait(250);
    auto *playback = window->findChild<QObject *>("autoplayTimer");
    check(playback && playback->setProperty("interval", 60), "The playback timer is there");
    // Sans gestionnaire de fenêtres la fenêtre n'est jamais active, et les raccourcis
    // restent muets : on passe par le bouton, qui fait la même chose qu'Espace.
    click(window, named(window, "autoplay"));
    QTest::qWait(700);
    check(fen() == "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR" &&
              !playback->property("running").toBool(),
          "Playback walks to the end of the line, then stops");
    board->setProperty("hintMoves", "g1f3 e7e5");
    QTest::qWait(250);
    shot(window, "b3-fleches");
    board->setProperty("hintMoves", "");

    // 4. Ce que coûte un coup à l'écran : cent allers-retours dans la partie.
    controller.move("g1f3");
    controller.move("b8c6");
    controller.move("f1b5");
    QTest::qWait(300);
    QElapsedTimer timer;
    qint64 logic = 0;
    timer.start();
    for (int i = 0; i < 100; ++i) {
        QElapsedTimer inner;
        inner.start();
        controller.step(i % 2 ? 1 : -1);
        logic += inner.nsecsElapsed();
        QCoreApplication::processEvents();
    }
    std::cout << "dont calcul et liaisons : " << logic / 1e6 / 100 << " ms\n";
    const double perMove = timer.nsecsElapsed() / 1e6 / 100;
    std::cout << "un coup à l'écran : " << perMove << " ms\n";
    const double budget =
        qEnvironmentVariableIsSet("T64_FRAME_BUDGET") ? qEnvironmentVariableIntValue("T64_FRAME_BUDGET") : 16;
    check(perMove < budget, "Showing a move costs less than one frame");
    if (!warnings.isEmpty())
        qWarning() << "avertissements QML" << warnings;
    check(warnings.isEmpty(), "The board raises no QML warning");
    std::cout << checks << " board walk checks passed\n";
    return 0;
}
