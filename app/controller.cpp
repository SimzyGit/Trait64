// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include "archive.h"
#include <QClipboard>
#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent>
#include <algorithm>
#include <functional>

namespace {
QString s(const std::string &v) {
    return QString::fromStdString(v);
}
void require(bool ok, const QString &message) {
    if (!ok)
        throw std::runtime_error(message.toStdString());
}
QString squareName(int index) {
    return QString(QChar('a' + index % 8)) + QChar('1' + index / 8);
}
int squareIndex(const QString &sq) {
    return sq.size() == 2 && sq[0] >= 'a' && sq[0] <= 'h' && sq[1] >= '1' && sq[1] <= '8'
               ? (sq[1].unicode() - '1') * 8 + sq[0].unicode() - 'a'
               : -1;
}
void atomicWrite(const QString &path, const QByteArray &bytes) {
    QSaveFile f(path);
    require(f.open(QIODevice::WriteOnly), f.errorString());
    require(f.write(bytes) == bytes.size(), f.errorString());
    require(f.commit(), f.errorString());
}
} // namespace
Controller::Controller(const QString &path, QObject *parent)
    : QObject(parent), dataPath_(path), sessionPath_(path + "/session.json"), engine_(this),
      comparison_(this) {
    QDir().mkpath(path);
    db_ = std::make_unique<t64::Database>(path + "/library.sqlite");
    flipped_ = settings_.value("flipped", false).toBool();
    if (!settings_.contains("enginePath")) {
        QString stockfish = QStandardPaths::findExecutable("stockfish");
        if (stockfish.isEmpty() && QFileInfo("/usr/games/stockfish").isExecutable())
            stockfish = "/usr/games/stockfish";
        settings_.setValue("enginePath", stockfish);
    }
    autosave_.setSingleShot(true);
    autosave_.setInterval(600);
    connect(&autosave_, &QTimer::timeout, this, [this] {
        try {
            writeSession();
        } catch (const std::exception &e) {
            fail(e);
        }
    });
    analysisTimer_.setSingleShot(true);
    analysisTimer_.setInterval(180);
    connect(&analysisTimer_, &QTimer::timeout, this, &Controller::requestEngine);
    connect(&engine_, &t64::Uci::optionsChanged, this, &Controller::engineChanged);
    connect(&engine_, &t64::Uci::status, this, [this](const QString &v) {
        engineStatus_ = v;
        emit engineChanged();
    });
    connect(&engine_, &t64::Uci::lines, this, [this](const QVariantList &lines) {
        engineLines_ = lines;
        emit engineChanged();
    });
    connect(&engine_, &t64::Uci::bestMove, this, [this](const QString &move, const QString &fen) {
        if (playing_ && fen == s(game_.nodes[node_].fen)) {
            try {
                if (clockRunning_) {
                    tickClock();
                    if (!clockRunning_)
                        return;
                }
                const bool white = game_.boardAt(node_).sideToMove() == chess::Color::WHITE;
                auto candidate = game_;
                const int target = candidate.play(node_, move.toStdString());
                remember();
                game_ = std::move(candidate);
                node_ = target;
                review_.clear();
                emit reviewChanged();
                afterClockMove(white);
                refresh(true);
            } catch (const std::exception &e) {
                stopPlaying();
                fail(e);
                refresh();
            }
        }
    });
    connect(&importer_, &QFutureWatcher<ImportResult>::finished, this, [this] {
        busy_ = false;
        resumableImport_ = false;
        taskPoll_.stop();
        emit busyChanged();
        const auto r = importer_.result();
        if (r.operation == "source" && content_)
            content_->reportImport(r.error.isEmpty()
                                       ? QString("Base importée : %1 nouvelles parties, %2 déjà présentes.")
                                             .arg(r.count)
                                             .arg(r.duplicates)
                                       : r.error);
        refreshLibrary();
        refresh();
        if (!r.error.isEmpty())
            emit message(r.error, true);
        else if (r.operation == "reindex")
            emit message(QString("Index reconstruits pour %1 partie(s).").arg(r.count));
        else if (r.operation == "resumable")
            emit message(QString("Import %1 : %2 parties validées, dont %3 nouvelles.")
                             .arg(r.job["status"] == "completed" ? "terminé" : "en pause")
                             .arg(r.job["processed"].toLongLong())
                             .arg(r.job["added"].toLongLong()));
        else
            emit message(
                QString("%1 partie(s) importée(s), %2 doublon(s) ignoré(s).").arg(r.count).arg(r.duplicates));
    });
    QFile session(sessionPath_);
    if (session.open(QIODevice::ReadOnly)) {
        try {
            require(session.size() <= 64 * 1024 * 1024, "Session trop volumineuse.");
            QJsonParseError parseError;
            const auto document = QJsonDocument::fromJson(session.readAll(), &parseError);
            require(parseError.error == QJsonParseError::NoError && document.isObject(),
                    "Session JSON illisible.");
            auto json = document.object();
            auto games = t64::parsePgn(json["pgn"].toString().toStdString());
            if (!games.empty()) {
                game_ = std::move(games[0]);
                node_ = std::clamp(json["node"].toInt(), 0, int(game_.nodes.size()) - 1);
                gameId_ = json["library"].toString() == db_->path() ? json["id"].toVariant().toLongLong() : 0;
                dirty_ = json["dirty"].toBool();
            }
        } catch (const std::exception &) {
            session.close();
            const auto preserved =
                sessionPath_ + ".unreadable-" + QString::number(QDateTime::currentMSecsSinceEpoch());
            require(QFile::copy(sessionPath_, preserved),
                    "Impossible de préserver la session illisible ; ouverture interrompue.");
            QTimer::singleShot(0, this, [this, preserved] {
                emit message("Session illisible conservée pour récupération : " + preserved, true);
            });
        }
    }
    initFeatures();
    initReliability();
    initContent();
    initLibrarySearch();
    initPreparation();
    refreshLibrary();
    refresh();
}
Controller::~Controller() {
    searchEnabled_ = false;
    disconnect(&searchWorker_, nullptr, this, nullptr);
    if (cancelSearch_)
        cancelSearch_->store(true);
    searchWorker_.waitForFinished();
    preparationPending_ = false;
    disconnect(&preparationWorker_, nullptr, this, nullptr);
    if (cancelPreparation_)
        cancelPreparation_->store(true);
    preparationWorker_.waitForFinished();
    comparison_.stop();
    backupWorker_.waitForFinished();
    reviewer_.shutdown();
    cancelTask_->store(true);
    importer_.waitForFinished();
    try {
        if (trainingActive())
            finishTraining();
        writeSession();
    } catch (...) {
    }
}
void Controller::fail(const std::exception &e) {
    emit message(QString::fromUtf8(e.what()), true);
}
void Controller::refreshLibrary() {
    cancelPreparation();
    try {
        stats_ = db_->stats();
        cards_ = db_->cards();
        refreshRepertoires();
        players_ = db_->players();
        const auto jobs = db_->importJobs();
        if (jobs != importJobs_) {
            importJobs_ = jobs;
            emit importsChanged();
        }
        const auto sources = db_->sources(), history = db_->importHistory();
        if (sources != sources_ || history != importHistory_) {
            sources_ = sources;
            importHistory_ = history;
            emit sourcesChanged();
        }
        emit libraryChanged();
        requestLibrarySearch();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::refresh(bool content) {
    const QString previousFen = state_["fen"].toString();
    if (content) {
        dirty_ = true;
        if (!trainingActive())
            autosave_.start();
    }
    const auto board = game_.boardAt(node_);
    const auto &current = game_.nodes[node_];
    auto end = board.isGameOver();
    QString status = board.sideToMove() == chess::Color::WHITE ? "Trait aux Blancs" : "Trait aux Noirs";
    if (board.inCheck())
        status += " · Échec";
    if (end.second != chess::GameResult::NONE) {
        const bool final = end.first == chess::GameResultReason::CHECKMATE ||
                           end.first == chess::GameResultReason::STALEMATE ||
                           end.first == chess::GameResultReason::INSUFFICIENT_MATERIAL;
        if (final) {
            if (playing_ || localGame_)
                game_.tags["Result"] = end.first == chess::GameResultReason::CHECKMATE
                                           ? (board.sideToMove() == chess::Color::WHITE ? "0-1" : "1-0")
                                           : "1/2-1/2";
            playing_ = false;
            localGame_ = false;
            clockRunning_ = false;
            clock_.stop();
            engine_.stop();
        }
        switch (end.first) {
        case chess::GameResultReason::CHECKMATE:
            status =
                "Échec et mat · " + QString(board.sideToMove() == chess::Color::WHITE ? "les Noirs gagnent"
                                                                                      : "les Blancs gagnent");
            break;
        case chess::GameResultReason::STALEMATE:
            status = "Nulle · Pat";
            break;
        case chess::GameResultReason::INSUFFICIENT_MATERIAL:
            status = "Nulle · Matériel insuffisant";
            break;
        case chess::GameResultReason::FIFTY_MOVE_RULE:
            status = "Nulle réclamable · Règle des 50 coups";
            break;
        case chess::GameResultReason::THREEFOLD_REPETITION:
            status = "Nulle réclamable · Triple répétition";
            break;
        default:
            break;
        }
    }
    QVariantMap tags;
    for (const auto &[k, v] : game_.tags)
        tags[s(k)] = s(v);
    state_ = {
        {"fen", s(current.fen)},
        {"node", node_},
        {"selected", selected_},
        {"flipped", flipped_},
        {"status", status},
        {"comment", s(current.comment)},
        {"title", s(game_.tags["White"]) + " — " + s(game_.tags["Black"])},
        {"tags", tags},
        {"dirty", dirty_},
        {"savedId", gameId_},
        {"analysis", analysis_},
        {"playing", playing_ || localGame_},
        {"clockRunning", clockRunning_},
        {"whiteClock", QString("%1:%2").arg(whiteMs_ / 60000).arg((whiteMs_ / 1000) % 60, 2, 10, QChar('0'))},
        {"blackClock", QString("%1:%2").arg(blackMs_ / 60000).arg((blackMs_ / 1000) % 60, 2, 10, QChar('0'))},
        {"clockResult", clockResult_},
        {"chess960", game_.chess960},
        {"arrows", annotations()},
        {"activeTab", activeTab_},
        {"canUndo", !undo_.empty() && !trainingActive() && !clockRunning_},
        {"canRedo", !redo_.empty() && !trainingActive() && !clockRunning_},
        {"training", trainingActive()},
        {"trainingComplete", trainingComplete_},
        {"repertoireTraining", repertoireExercise_.value("repertoire", 0)},
        {"repertoireDue", repertoireState_.value("due", 0)},
        {"trainingProgress", QString::number(solutionIndex_) + " / " +
                                 QString::number(repertoireExercise_.isEmpty() ? solution_.size() : 1)},
        {"canBack", node_ > 0},
        {"canForward", !current.children.empty()},
        {"nag", current.nags.empty() ? 0 : current.nags[0]},
        {"whiteTurn", board.sideToMove() == chess::Color::WHITE}};
    chess::Movelist legal;
    chess::movegen::legalmoves(legal, board);
    QSet<int> targets;
    if (selected_ >= 0)
        for (auto m : legal) {
            auto u = s(chess::uci::moveToUci(m, game_.chess960));
            if (u.left(2) == squareName(selected_))
                targets.insert(squareIndex(u.mid(2, 2)));
        }
    squares_.clear();
    for (int i = 0; i < 64; ++i) {
        const auto p = board.at(chess::Square(i));
        const bool occupied = p != chess::Piece::NONE;
        static const QStringList glyphs = {"♟", "♞", "♝", "♜", "♛", "♚"};
        const QString glyph = occupied ? glyphs[int(p.type())] : QString();
        squares_ << QVariantMap{
            {"square", squareName(i)},
            {"piece", glyph},
            {"code", occupied ? (p.color() == chess::Color::WHITE ? QString("w") : QString("b")) +
                                    QString("PNBRQK")[int(p.type())]
                              : QString()},
            {"white", occupied && p.color() == chess::Color::WHITE},
            {"occupied", occupied},
            {"legal", targets.contains(i)},
            {"last", current.uci.size() >= 4 && (s(current.uci).left(2) == squareName(i) ||
                                                 s(current.uci).mid(2, 2) == squareName(i))},
            {"check", board.inCheck() && i == board.kingSq(board.sideToMove()).index()}};
    }
    notation_.clear();
    notation_ << QVariantMap{{"id", 0},
                             {"text", "Position initiale"},
                             {"depth", 0},
                             {"comment", s(game_.nodes[0].comment)},
                             {"active", node_ == 0}};
    std::function<void(int, int)> visit = [&](int n, int depth) {
        if (depth > 128)
            return;
        while (n > 0 && notation_.size() < 10000) {
            const auto &v = game_.nodes[n];
            auto b = t64::checkedBoard(game_.nodes[v.parent].fen, game_.chess960);
            QString label = QString::number(b.fullMoveNumber()) +
                            (b.sideToMove() == chess::Color::WHITE ? ". " : "… ") + s(v.san);
            static const QStringList nags = {"", "!", "?", "!!", "??", "!?", "?!"};
            for (int nag : v.nags)
                label += " " + (nag < 7 ? nags[nag] : "$" + QString::number(nag));
            notation_ << QVariantMap{{"id", n},
                                     {"text", label},
                                     {"depth", depth},
                                     {"comment", s(v.comment)},
                                     {"active", n == node_}};
            const auto &siblings = game_.nodes[v.parent].children;
            if (siblings[0] == n)
                for (size_t k = 1; k < siblings.size(); ++k)
                    visit(siblings[k], depth + 1);
            n = v.children.empty() ? -1 : v.children[0];
        }
    };
    if (!game_.nodes[0].children.empty())
        visit(game_.nodes[0].children[0], 0);
    try {
        openings_ = db_->explorer(s(current.fen));
    } catch (const std::exception &e) {
        fail(e);
    }
    detectOpening();
    try {
        refreshRepertoires(false);
    } catch (const std::exception &e) {
        fail(e);
    }
    emit changed();
    if (positionFilter_ && previousFen != s(current.fen))
        requestLibrarySearch();
    if ((analysis_ || playing_) && !trainingActive() && previousFen != s(current.fen))
        analysisTimer_.start();
    if (!trainingActive())
        comparison_.setPosition(s(current.fen), positionCommand(), game_.chess960);
}
void Controller::writeSession() {
    if (trainingActive())
        return;
    storeTab(false);
    QJsonArray tabs;
    for (const auto &study : studies_) {
        const auto &v = study.position;
        QJsonArray path;
        QStringList moves;
        for (int n = v.node; n > 0; n = v.game.nodes[n].parent)
            moves.prepend(s(v.game.nodes[n].uci));
        for (const auto &m : moves)
            path.append(m);
        tabs.append(QJsonObject{
            {"pgn", s(v.game.pgn())}, {"path", path}, {"id", QString::number(v.id)}, {"dirty", v.dirty}});
    }
    QJsonObject json{{"library", db_->path()}, {"tabs", tabs}, {"activeTab", activeTab_}};
    auto bytes = QJsonDocument(json).toJson(QJsonDocument::Compact);
    require(bytes.size() <= 64 * 1024 * 1024,
            "Session trop volumineuse : enregistrez et fermez quelques onglets.");
    atomicWrite(sessionPath_, bytes);
}
void Controller::checkpoint() {
    if (trainingActive())
        finishTraining();
    if (dirty_) {
        require(!busy_, "Attendez la fin du traitement ou interrompez-le avant de changer d’étude.");
        gameId_ = db_->save(game_, gameId_);
        dirty_ = false;
        refreshLibrary();
    }
    writeSession();
}
void Controller::replaceGame(t64::Game game, qint64 id) {
    if (id)
        for (int i = 0; i < int(studies_.size()); ++i)
            if ((i == activeTab_ ? gameId_ : studies_[i].position.id) == id) {
                switchTab(i);
                emit showAnalysis();
                return;
            }
    require(studies_.size() < 16, "16 études ouvertes : fermez un onglet avant d’en ouvrir un autre.");
    checkpoint();
    cancelReview();
    stopPlaying();
    storeTab();
    studies_.emplace_back();
    activeTab_ = int(studies_.size()) - 1;
    undo_.clear();
    redo_.clear();
    review_.clear();
    commentNode_ = -1;
    clockResult_.clear();
    emit reviewChanged();
    game_ = std::move(game);
    gameId_ = id;
    node_ = 0;
    selected_ = -1;
    dirty_ = false;
    engineLines_.clear();
    emit engineChanged();
    refresh();
    writeSession();
    emit showAnalysis();
}
void Controller::newGame() {
    try {
        replaceGame(t64::Game());
        game_.tags["Date"] = QDate::currentDate().toString("yyyy.MM.dd").toStdString();
        refresh();
        writeSession();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::loadGame(qint64 id) {
    try {
        replaceGame(db_->load(id), id);
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::saveGame(bool copy) {
    try {
        require(!trainingActive(), "Terminez l’entraînement avant de sauvegarder.");
        require(!busy_, "Attendez la fin du traitement avant d’enregistrer.");
        gameId_ = db_->save(game_, copy ? 0 : gameId_);
        dirty_ = false;
        writeSession();
        refreshLibrary();
        refresh();
        emit message("Partie enregistrée dans la bibliothèque.");
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::setFen(const QString &fen) {
    try {
        replaceGame(t64::Game(fen.trimmed().toStdString()));
        dirty_ = true;
        writeSession();
        refresh();
    } catch (const std::exception &e) {
        fail(e);
    }
}
QString Controller::localPath(const QString &input) const {
    const QUrl url(input);
    require(url.isLocalFile() || url.scheme().isEmpty(), "Seuls les fichiers locaux sont acceptés.");
    return url.isLocalFile() ? url.toLocalFile() : input;
}
void Controller::importText(const QString &text) {
    importBytes(text.toUtf8());
}
void Controller::importFile(const QString &url) {
    importSourceFile(url, {});
}
void Controller::importSourceFile(const QString &url, const QVariantMap &source) {
    try {
        require(!busy_, "Un traitement est déjà en cours.");
        const auto path = localPath(url);
        if (path.endsWith(".pgn", Qt::CaseInsensitive)) {
            require(QFileInfo(path).isFile(), "PGN introuvable.");
            if (trainingActive())
                finishTraining();
            busy_ = true;
            cancelTask_->store(false);
            taskProgress_->store(0);
            taskPoll_.start();
            emit busyChanged();
            const auto databasePath = db_->path();
            const auto canceled = cancelTask_;
            const auto progress = taskProgress_;
            importer_.setFuture(QtConcurrent::run([databasePath, path, canceled, progress, source] {
                ImportResult r;
                r.operation = source.isEmpty() ? "import" : "source";
                try {
                    t64::Database db(databasePath);
                    r.count = db.importPgnFile(
                        path,
                        [&](int n) {
                            progress->store(n);
                            return !canceled->load();
                        },
                        source);
                    r.duplicates = progress->load() - r.count;
                } catch (const std::exception &e) {
                    r.error = QString::fromUtf8(e.what());
                }
                return r;
            }));
            return;
        }
        if (path.endsWith(".t64", Qt::CaseInsensitive)) {
            importBytes(t64::Archive::read(path));
            return;
        }
        QFile f(path);
        require(f.open(QIODevice::ReadOnly), f.errorString());
        require(f.size() <= 64 * 1024 * 1024, "PGN limité à 64 Mio par import.");
        if (path.endsWith(".epd", Qt::CaseInsensitive)) {
            auto games = t64::parseEpd(f.readAll().toStdString());
            int count = db_->importGames(games);
            refreshLibrary();
            emit message(QString("%1 position(s) EPD importée(s).").arg(count));
        } else if (path.endsWith(".fen", Qt::CaseInsensitive))
            setFen(QString::fromUtf8(f.readAll()));
        else
            importBytes(f.readAll());
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::importBytes(const QByteArray &bytes) {
    if (busy_) {
        emit message("Un import est déjà en cours.", true);
        return;
    }
    if (trainingActive())
        finishTraining();
    const auto path = db_->path();
    cancelTask_->store(false);
    taskProgress_->store(0);
    busy_ = true;
    taskPoll_.start();
    emit busyChanged();
    const auto canceled = cancelTask_;
    const auto progress = taskProgress_;
    importer_.setFuture(QtConcurrent::run([path, bytes, canceled, progress] {
        ImportResult r;
        try {
            auto games = t64::parsePgn(bytes.toStdString());
            require(!games.empty(), "Aucune partie trouvée.");
            t64::Database db(path);
            r.count = db.importGames(games, [&](int n) {
                progress->store(n);
                return !canceled->load();
            });
            r.duplicates = int(games.size()) - r.count;
        } catch (const std::exception &e) {
            r.error = QString::fromUtf8(e.what());
        }
        return r;
    }));
}
void Controller::exportFile(const QString &url, bool all) {
    try {
        const auto path = localPath(url);
        db_->validateOutputPath(path);
        if (all && !path.endsWith(".epd", Qt::CaseInsensitive) &&
            !path.endsWith(".t64", Qt::CaseInsensitive)) {
            QSaveFile file(path);
            require(file.open(QIODevice::WriteOnly), file.errorString());
            db_->exportPgn(file);
            require(file.commit(), file.errorString());
        } else {
            auto data = path.endsWith(".epd", Qt::CaseInsensitive)
                            ? QByteArray::fromStdString(t64::exportEpd(game_, node_))
                            : (all ? db_->exportPgn() : s(game_.pgn())).toUtf8();
            if (path.endsWith(".t64", Qt::CaseInsensitive))
                t64::Archive::append(path, data);
            else
                atomicWrite(path, data);
        }
        emit message("Export terminé : " + QFileInfo(path).fileName());
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::backup(const QString &url) {
    try {
        require(!busy_, "Attendez la fin du traitement.");
        checkpoint();
        db_->backup(localPath(url));
        emit message("Sauvegarde complète créée, entraînements inclus.");
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::openBackup(const QString &url) {
    try {
        require(!busy_, "Attendez la fin de l’import.");
        checkpoint();
        const auto path = localPath(url);
        require(QFileInfo::exists(path), "Bibliothèque introuvable.");
        auto candidate = std::make_unique<t64::Database>(path);
        require(candidate->integrity() == "ok", "Cette bibliothèque est endommagée.");
        db_ = std::move(candidate);
        gameId_ = 0;
        dirty_ = true;
        for (auto &tab : studies_) {
            tab.position.id = 0;
            tab.position.dirty = true;
            for (auto &v : tab.undo)
                v.id = 0;
            for (auto &v : tab.redo)
                v.id = 0;
        }
        for (auto &v : undo_)
            v.id = 0;
        for (auto &v : redo_)
            v.id = 0;
        writeSession();
        advancedFilters_.clear();
        emit libraryOpened();
        refreshLibrary();
        refresh();
        emit message("Bibliothèque ouverte : " + QFileInfo(path).fileName());
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::clickSquare(const QString &sq) {
    int index = squareIndex(sq);
    if (index < 0)
        return;
    if (selected_ == index) {
        selected_ = -1;
        refresh();
        return;
    }
    if (selected_ >= 0) {
        const auto from = squareName(selected_);
        auto b = game_.boardAt(node_);
        auto p = b.at(chess::Square(index));
        if (p == chess::Piece::NONE || p.color() != b.sideToMove() ||
            (game_.chess960 && b.at(chess::Square(selected_)).type() == chess::PieceType::KING &&
             p.type() == chess::PieceType::ROOK)) {
            tryMove(from, sq);
            return;
        }
    }
    const auto b = game_.boardAt(node_);
    auto p = b.at(chess::Square(index));
    if (p != chess::Piece::NONE && p.color() == b.sideToMove())
        selected_ = index;
    else
        selected_ = -1;
    refresh();
}
void Controller::tryMove(const QString &from, const QString &to) {
    if (squareIndex(from) < 0 || squareIndex(to) < 0)
        return;
    auto b = game_.boardAt(node_);
    chess::Movelist list;
    chess::movegen::legalmoves(list, b);
    QStringList candidates;
    for (auto m : list) {
        auto u = s(chess::uci::moveToUci(m, game_.chess960));
        if (u.left(4) == from + to)
            candidates << u;
    }
    if (candidates.size() > 1) {
        emit promotionNeeded(from + to);
        return;
    }
    if (candidates.size() == 1)
        move(candidates[0]);
    else {
        selected_ = -1;
        refresh();
        emit message("Ce coup n’est pas légal.");
    }
}
void Controller::move(const QString &uci) {
    try {
        if (playRepertoireAnswer(uci))
            return;
        if (playing_) {
            auto b = game_.boardAt(node_);
            require((b.sideToMove() == chess::Color::WHITE) == humanWhite_, "Le moteur réfléchit.");
        }
        if (trainingActive()) {
            if (trainingComplete_)
                return;
            if (solutionIndex_ >= solution_.size() || uci != solution_[solutionIndex_]) {
                trainingFailed_ = true;
                emit message("Ce n’est pas le coup du répertoire. Essaie encore.");
                return;
            }
            node_ = game_.play(node_, uci.toStdString());
            ++solutionIndex_;
            if (solutionIndex_ < solution_.size()) {
                node_ = game_.play(node_, solution_[solutionIndex_].toStdString());
                ++solutionIndex_;
            }
            if (solutionIndex_ >= solution_.size()) {
                trainingComplete_ = true;
                db_->review(cardId_, !trainingFailed_);
                refreshLibrary();
                emit message(trainingFailed_ ? "Ligne terminée. Nouvelle révision demain."
                                             : "Bien joué ! La prochaine révision est programmée.");
            }
            selected_ = -1;
            refresh();
            return;
        }
        if (clockRunning_) {
            tickClock();
            if (!clockRunning_)
                return;
        }
        const bool white = game_.boardAt(node_).sideToMove() == chess::Color::WHITE;
        auto candidate = game_;
        const int target = candidate.play(node_, uci.toStdString());
        remember();
        game_ = std::move(candidate);
        node_ = target;
        review_.clear();
        emit reviewChanged();
        afterClockMove(white);
        selected_ = -1;
        refresh(true);
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::navigate(int node) {
    if (trainingActive())
        return;
    if (node < 0 || node >= int(game_.nodes.size()))
        return;
    for (int n = node; n > 0; n = game_.nodes[n].parent) {
        int p = game_.nodes[n].parent;
        if (p < 0 || std::find(game_.nodes[p].children.begin(), game_.nodes[p].children.end(), n) ==
                         game_.nodes[p].children.end())
            return;
    }
    commentNode_ = -1;
    playing_ = false;
    localGame_ = false;
    clockRunning_ = false;
    clock_.stop();
    engine_.stop();
    node_ = node;
    selected_ = -1;
    engineLines_.clear();
    emit engineChanged();
    refresh();
    autosave_.start();
}
void Controller::step(int direction) {
    if (direction == -2)
        navigate(0);
    else if (direction == -1 && node_ > 0)
        navigate(game_.nodes[node_].parent);
    else if (direction == 1 && !game_.nodes[node_].children.empty())
        navigate(game_.nodes[node_].children[0]);
    else if (direction == 2) {
        int n = node_;
        while (!game_.nodes[n].children.empty())
            n = game_.nodes[n].children[0];
        navigate(n);
    }
}
void Controller::flip() {
    flipped_ = !flipped_;
    settings_.setValue("flipped", flipped_);
    refresh();
}
void Controller::setComment(const QString &comment) {
    if (trainingActive())
        return;
    if (game_.nodes[node_].comment == comment.toStdString())
        return;
    if (commentNode_ != node_ || !commentEdit_.isValid() || commentEdit_.elapsed() > 1200) {
        remember();
        commentNode_ = node_;
    }
    commentEdit_.restart();
    game_.nodes[node_].comment = comment.left(100000).toStdString();
    refresh(true);
}
void Controller::setNag(int nag) {
    if (trainingActive())
        return;
    remember();
    game_.nodes[node_].nags.clear();
    if (nag > 0 && nag <= 255)
        game_.nodes[node_].nags.push_back(nag);
    refresh(true);
}
void Controller::setTags(const QVariantMap &tags) {
    if (trainingActive())
        return;
    if (tags.contains("Result") &&
        !QStringList{"*", "1-0", "0-1", "1/2-1/2"}.contains(tags["Result"].toString())) {
        emit message("Résultat invalide : utilisez *, 1-0, 0-1 ou 1/2-1/2.", true);
        return;
    }
    remember();
    for (const auto &key : QStringList{"White", "Black", "Event", "Site", "Date", "Round", "Result", "ECO",
                                       "WhiteElo", "BlackElo"})
        if (tags.contains(key))
            game_.tags[key.toStdString()] = tags[key].toString().left(1000).toStdString();
    refresh(true);
}
void Controller::promote() {
    try {
        if (trainingActive())
            return;
        remember();
        game_.promote(node_);
        review_.clear();
        emit reviewChanged();
        refresh(true);
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::deleteVariation() {
    try {
        if (trainingActive())
            return;
        require(node_ > 0, "La position initiale ne peut pas être supprimée.");
        remember();
        review_.clear();
        emit reviewChanged();
        int parent = game_.nodes[node_].parent;
        game_.removeVariation(node_);
        node_ = parent;
        refresh(true);
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::search(const QString &query, const QString &result, bool favorite, bool position,
                        bool trash) {
    query_ = query;
    resultFilter_ = result;
    favoritesOnly_ = favorite;
    positionFilter_ = position;
    trashFilter_ = trash;
    requestLibrarySearch();
}
void Controller::favorite(qint64 id, bool value) {
    try {
        require(!busy_, "Traitement en cours.");
        db_->favorite(id, value);
        refreshLibrary();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::trash(qint64 id, bool value) {
    try {
        require(!busy_, "Traitement en cours.");
        db_->trash(id, value);
        refreshLibrary();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::setCollection(qint64 id, const QString &c) {
    try {
        require(!busy_, "Traitement en cours.");
        db_->collection(id, c);
        refreshLibrary();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::copyText(const QString &what) {
    QGuiApplication::clipboard()->setText(what == "fen" ? s(game_.nodes[node_].fen) : s(game_.pgn()));
    emit message(what.toUpper() + " copié.");
}
void Controller::startEngine() {
    auto options = settings_.value("uciOptions").toMap();
    if (!playing_)
        options["UCI_LimitStrength"] = false;
    engine_.start(settings_.value("enginePath").toString(), settings_.value("threads", 1).toInt(),
                  settings_.value("hash", 128).toInt(), settings_.value("multiPV", 3).toInt(),
                  playing_ ? settings_.value("skill", 12).toInt() : 20,
                  settings_.value("syzygyPath").toString(), options);
    if (analysis_ || playing_)
        requestEngine();
}
QString Controller::positionCommand() const {
    QStringList moves;
    for (int n = node_; n > 0; n = game_.nodes[n].parent)
        moves.prepend(s(game_.nodes[n].uci));
    return "position fen " + s(game_.nodes[0].fen) +
           (moves.empty() ? QString() : " moves " + moves.join(' '));
}
void Controller::requestEngine() {
    if (trainingActive())
        return;
    if (playing_) {
        auto b = game_.boardAt(node_);
        if ((b.sideToMove() == chess::Color::WHITE) != humanWhite_)
            engine_.analyse(s(game_.nodes[node_].fen), positionCommand(), true,
                            settings_.value("moveTime", 1000).toInt(),
                            settings_.value("analysisDepth", 24).toInt(), game_.chess960);
    } else if (analysis_)
        engine_.analyse(s(game_.nodes[node_].fen), positionCommand(), false, 800,
                        settings_.value("analysisDepth", 24).toInt(), game_.chess960);
}
void Controller::toggleAnalysis() {
    if (trainingActive())
        return;
    comparison_.stop();
    cancelReview();
    clockRunning_ = false;
    localGame_ = false;
    clock_.stop();
    playing_ = false;
    analysis_ = !analysis_;
    if (analysis_) {
        // Restart to restore full strength after a reduced-skill playing session.
        startEngine();
        requestEngine();
    } else
        engine_.stop();
    refresh();
}
void Controller::addEngineLine(int index) {
    try {
        require(index >= 0 && index < engineLines_.size(), "Ligne indisponible.");
        auto line = engineLines_[index].toMap();
        require(line["fen"].toString() == s(game_.nodes[node_].fen),
                "Cette analyse correspond à une autre position.");
        require(!trainingActive(), "Terminez l’entraînement.");
        remember();
        int n = node_;
        for (const auto &m : line["uci"].toString().split(' ', Qt::SkipEmptyParts))
            n = game_.play(n, m.toStdString());
        refresh(true);
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::playComputer(bool humanWhite) {
    if (trainingActive())
        return;
    comparison_.stop();
    cancelReview();
    clockRunning_ = false;
    localGame_ = false;
    clock_.stop();
    humanWhite_ = humanWhite;
    playing_ = true;
    analysis_ = false;
    flipped_ = !humanWhite;
    startEngine();
    refresh();
}
void Controller::stopPlaying() {
    comparison_.stop();
    clockRunning_ = false;
    localGame_ = false;
    clock_.stop();
    playing_ = false;
    analysis_ = false;
    analysisTimer_.stop();
    engine_.stop();
    refresh();
}
void Controller::createCard(const QString &title) {
    try {
        require(!busy_, "Traitement en cours.");
        require(!game_.chess960, "Les cartes de lignes concernent pour le moment les échecs classiques.");
        QStringList moves;
        int n = node_;
        while (!game_.nodes[n].children.empty() && moves.size() < 40) {
            n = game_.nodes[n].children[0];
            moves << s(game_.nodes[n].uci);
        }
        require(!moves.empty(), "Placez-vous avant le premier coup à apprendre dans une ligne existante.");
        db_->addCard(title.isEmpty() ? "Ma position" : title, s(game_.nodes[node_].fen), moves.join(' '));
        refreshLibrary();
        emit message("Carte ajoutée à l’entraînement.");
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::startCard(qint64 id) {
    try {
        require(!busy_, "Attendez la fin de l’import avant une révision.");
        if (trainingActive())
            finishTraining();
        checkpoint();
        QVariantMap card;
        for (const auto &c : cards_)
            if (c.toMap()["id"].toLongLong() == id) {
                card = c.toMap();
                break;
            }
        require(!card.empty(), "Carte introuvable.");
        beforeTraining_ = game_;
        beforeNode_ = node_;
        beforeId_ = gameId_;
        beforeDirty_ = dirty_;
        beforeFlipped_ = flipped_;
        cancelReview();
        stopPlaying();
        cardId_ = id;
        solution_ = card["moves"].toString().split(' ', Qt::SkipEmptyParts);
        solutionIndex_ = 0;
        trainingComplete_ = trainingFailed_ = false;
        game_ = t64::Game(card["fen"].toString().toStdString());
        node_ = 0;
        selected_ = -1;
        flipped_ = game_.boardAt(0).sideToMove() == chess::Color::BLACK;
        game_.tags["White"] = card["title"].toString().toStdString();
        game_.tags["Black"] = "Entraînement";
        engineLines_.clear();
        emit engineChanged();
        refresh();
        emit showAnalysis();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::revealCard() {
    if (!trainingActive() || trainingComplete_)
        return;
    trainingFailed_ = true;
    if (!repertoireExercise_.isEmpty()) {
        QStringList answers;
        for (const auto &answer : repertoireExercise_["answers"].toList())
            answers << answer.toMap()["san"].toString();
        emit message("Réponses préparées : " + answers.join(", "));
        return;
    }
    QStringList san;
    auto g = game_;
    int n = node_;
    for (int i = solutionIndex_; i < solution_.size(); ++i) {
        n = g.play(n, solution_[i].toStdString());
        san << s(g.nodes[n].san);
    }
    emit message("Solution : " + san.join(' '));
}
void Controller::finishTraining() {
    if (!trainingActive())
        return;
    game_ = beforeTraining_;
    node_ = beforeNode_;
    gameId_ = beforeId_;
    dirty_ = beforeDirty_;
    flipped_ = beforeFlipped_;
    cardId_ = 0;
    repertoireExercise_.clear();
    trainingComplete_ = false;
    selected_ = -1;
    refresh();
}
void Controller::deleteCard(qint64 id) {
    try {
        require(!busy_, "Traitement en cours.");
        if (cardId_ == id)
            finishTraining();
        db_->deleteCard(id);
        refreshLibrary();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::loadExamples() {
    QFile f(":/assets/demo.pgn");
    if (f.open(QIODevice::ReadOnly))
        importBytes(f.readAll());
}
void Controller::verifyDatabase() {
    try {
        emit message("Vérification SQLite : " + db_->integrity());
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::runPerft(int depth) {
    try {
        require(depth >= 1 && depth <= 4, "Choisissez une profondeur de 1 à 4.");
        auto board = game_.boardAt(node_);
        auto count = t64::perft(board, depth);
        emit message(QString("Perft %1 : %2 positions légales.").arg(depth).arg(count));
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::openDataFolder() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(db_->path()).absolutePath()));
}
bool Controller::smokeTest(QString *error) {
    try {
        node_ = game_.play(0, "e2e4");
        node_ = game_.play(node_, "e7e5");
        game_.play(1, "c7c5");
        refresh(true);
        gameId_ = db_->save(game_);
        auto loaded = db_->load(gameId_);
        require(loaded.nodes.size() == 4, "Smoke: save/load mismatch");
        refreshLibrary();
        return true;
    } catch (const std::exception &e) {
        *error = QString::fromUtf8(e.what());
        return false;
    }
}
