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
    : QObject(parent), dataPath_(path), sessionPath_(path + "/session.json"), engine_(this) {
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
                node_ = game_.play(node_, move.toStdString());
                refresh(true);
            } catch (const std::exception &e) {
                playing_ = false;
                fail(e);
                refresh();
            }
        }
    });
    connect(&importer_, &QFutureWatcher<ImportResult>::finished, this, [this] {
        busy_ = false;
        emit busyChanged();
        const auto r = importer_.result();
        refreshLibrary();
        refresh();
        if (!r.error.isEmpty())
            emit message(r.error, true);
        else
            emit message(
                QString("%1 partie(s) importée(s), %2 doublon(s) ignoré(s).").arg(r.count).arg(r.duplicates));
    });
    QFile session(sessionPath_);
    if (session.open(QIODevice::ReadOnly)) {
        try {
            require(session.size() <= 64 * 1024 * 1024, "Session trop volumineuse.");
            auto json = QJsonDocument::fromJson(session.readAll()).object();
            auto games = t64::parsePgn(json["pgn"].toString().toStdString());
            if (!games.empty()) {
                game_ = std::move(games[0]);
                node_ = std::clamp(json["node"].toInt(), 0, int(game_.nodes.size()) - 1);
                gameId_ = json["library"].toString() == db_->path() ? json["id"].toVariant().toLongLong() : 0;
                dirty_ = json["dirty"].toBool();
            }
        } catch (const std::exception &) {
            session.close();
            QFile::copy(sessionPath_,
                        sessionPath_ + ".unreadable-" + QString::number(QDateTime::currentMSecsSinceEpoch()));
        }
    }
    refreshLibrary();
    refresh();
}
Controller::~Controller() {
    importer_.waitForFinished();
    try {
        if (cardId_)
            finishTraining();
        writeSession();
    } catch (...) {
    }
}
QVariantMap Controller::preferences() const {
    return {{"enginePath", settings_.value("enginePath", "")},
            {"threads", settings_.value("threads", 1)},
            {"hash", settings_.value("hash", 128)},
            {"multiPV", settings_.value("multiPV", 3)},
            {"skill", settings_.value("skill", 12)},
            {"moveTime", settings_.value("moveTime", 1000)},
            {"lightTheme", settings_.value("lightTheme", false)},
            {"coordinates", settings_.value("coordinates", true)},
            {"dataPath", dataPath_}};
}
void Controller::fail(const std::exception &e) {
    emit message(QString::fromUtf8(e.what()), true);
}
void Controller::refreshLibrary() {
    try {
        games_ = db_->search(query_, resultFilter_, favoritesOnly_,
                             positionFilter_ ? s(game_.nodes[node_].fen) : QString(), trashFilter_);
        stats_ = db_->stats();
        cards_ = db_->cards();
        emit libraryChanged();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::refresh(bool content) {
    const QString previousFen = state_["fen"].toString();
    if (content) {
        dirty_ = true;
        if (!cardId_)
            autosave_.start();
    }
    const auto board = game_.boardAt(node_);
    const auto &current = game_.nodes[node_];
    auto end = board.isGameOver();
    QString status = board.sideToMove() == chess::Color::WHITE ? "Trait aux Blancs" : "Trait aux Noirs";
    if (board.inCheck())
        status += " · Échec";
    if (end.second != chess::GameResult::NONE) {
        playing_ = false;
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
        {"playing", playing_},
        {"training", bool(cardId_)},
        {"trainingComplete", trainingComplete_},
        {"trainingProgress", QString::number(solutionIndex_) + " / " + QString::number(solution_.size())},
        {"canBack", node_ > 0},
        {"canForward", !current.children.empty()},
        {"nag", current.nags.empty() ? 0 : current.nags[0]},
        {"whiteTurn", board.sideToMove() == chess::Color::WHITE}};
    chess::Movelist legal;
    chess::movegen::legalmoves(legal, board);
    QSet<int> targets;
    if (selected_ >= 0)
        for (auto m : legal) {
            auto u = s(chess::uci::moveToUci(m));
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
            auto b = t64::checkedBoard(game_.nodes[v.parent].fen);
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
    emit changed();
    if (positionFilter_)
        refreshLibrary();
    if ((analysis_ || playing_) && !cardId_ && previousFen != s(current.fen))
        analysisTimer_.start();
}
void Controller::writeSession() {
    if (cardId_)
        return;
    // PGN parsing can reorder node IDs; persist the path as SAN-independent UCI.
    QStringList path;
    for (int n = node_; n > 0; n = game_.nodes[n].parent)
        path.prepend(s(game_.nodes[n].uci));
    auto canonical = t64::parsePgn(game_.pgn()).at(0);
    int canonicalNode = 0;
    for (const auto &m : path)
        canonicalNode = canonical.play(canonicalNode, m.toStdString());
    QJsonObject json{{"pgn", s(game_.pgn())},
                     {"node", canonicalNode},
                     {"id", gameId_},
                     {"dirty", dirty_},
                     {"library", db_->path()}};
    atomicWrite(sessionPath_, QJsonDocument(json).toJson());
}
void Controller::checkpoint() {
    if (cardId_)
        finishTraining();
    if (dirty_) {
        gameId_ = db_->save(game_, gameId_);
        dirty_ = false;
        refreshLibrary();
    }
    writeSession();
}
void Controller::replaceGame(t64::Game game, qint64 id) {
    checkpoint();
    engine_.stop();
    playing_ = false;
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
        require(!cardId_, "Terminez l’entraînement avant de sauvegarder.");
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
    try {
        const auto path = localPath(url);
        if (path.endsWith(".t64", Qt::CaseInsensitive)) {
            importBytes(t64::Archive::read(path));
            return;
        }
        QFile f(path);
        require(f.open(QIODevice::ReadOnly), f.errorString());
        require(f.size() <= 64 * 1024 * 1024, "PGN limité à 64 Mio par import.");
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
    const auto path = db_->path();
    busy_ = true;
    emit busyChanged();
    importer_.setFuture(QtConcurrent::run([path, bytes] {
        ImportResult r;
        try {
            auto games = t64::parsePgn(bytes.toStdString());
            require(!games.empty(), "Aucune partie trouvée.");
            t64::Database db(path);
            r.count = db.importGames(games);
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
        auto data = (all ? db_->exportPgn() : s(game_.pgn())).toUtf8();
        if (path.endsWith(".t64", Qt::CaseInsensitive))
            t64::Archive::append(path, data);
        else
            atomicWrite(path, data);
        emit message("Export terminé : " + QFileInfo(path).fileName());
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::backup(const QString &url) {
    try {
        require(!busy_, "Attendez la fin de l’import.");
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
        if (p == chess::Piece::NONE || p.color() != b.sideToMove()) {
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
        auto u = s(chess::uci::moveToUci(m));
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
        if (playing_) {
            auto b = game_.boardAt(node_);
            require((b.sideToMove() == chess::Color::WHITE) == humanWhite_, "Le moteur réfléchit.");
        }
        if (cardId_) {
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
        node_ = game_.play(node_, uci.toStdString());
        selected_ = -1;
        refresh(true);
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::navigate(int node) {
    if (cardId_)
        return;
    if (node < 0 || node >= int(game_.nodes.size()))
        return;
    playing_ = false;
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
    if (cardId_)
        return;
    game_.nodes[node_].comment = comment.left(100000).toStdString();
    refresh(true);
}
void Controller::setNag(int nag) {
    if (cardId_)
        return;
    game_.nodes[node_].nags.clear();
    if (nag > 0 && nag <= 255)
        game_.nodes[node_].nags.push_back(nag);
    refresh(true);
}
void Controller::setTags(const QVariantMap &tags) {
    if (cardId_)
        return;
    if (tags.contains("Result") &&
        !QStringList{"*", "1-0", "0-1", "1/2-1/2"}.contains(tags["Result"].toString())) {
        emit message("Résultat invalide : utilisez *, 1-0, 0-1 ou 1/2-1/2.", true);
        return;
    }
    for (const auto &key : QStringList{"White", "Black", "Event", "Site", "Date", "Round", "Result", "ECO",
                                       "WhiteElo", "BlackElo"})
        if (tags.contains(key))
            game_.tags[key.toStdString()] = tags[key].toString().left(1000).toStdString();
    refresh(true);
}
void Controller::promote() {
    try {
        if (cardId_)
            return;
        game_.promote(node_);
        refresh(true);
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::deleteVariation() {
    try {
        if (cardId_)
            return;
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
    refreshLibrary();
}
void Controller::favorite(qint64 id, bool value) {
    try {
        db_->favorite(id, value);
        refreshLibrary();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::trash(qint64 id, bool value) {
    try {
        db_->trash(id, value);
        refreshLibrary();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::setCollection(qint64 id, const QString &c) {
    try {
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
void Controller::setPreference(const QString &key, const QVariant &value) {
    if (!QStringList{"enginePath", "threads", "hash", "multiPV", "skill", "moveTime", "lightTheme",
                     "coordinates"}
             .contains(key))
        return;
    try {
        settings_.setValue(key, key == "enginePath" ? QVariant(localPath(value.toString())) : value);
        emit preferencesChanged();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::startEngine() {
    engine_.start(settings_.value("enginePath").toString(), settings_.value("threads", 1).toInt(),
                  settings_.value("hash", 128).toInt(), settings_.value("multiPV", 3).toInt(),
                  playing_ ? settings_.value("skill", 12).toInt() : 20);
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
    if (cardId_)
        return;
    if (playing_) {
        auto b = game_.boardAt(node_);
        if ((b.sideToMove() == chess::Color::WHITE) != humanWhite_)
            engine_.analyse(s(game_.nodes[node_].fen), positionCommand(), true,
                            settings_.value("moveTime", 1000).toInt());
    } else if (analysis_)
        engine_.analyse(s(game_.nodes[node_].fen), positionCommand());
}
void Controller::toggleAnalysis() {
    if (cardId_)
        return;
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
        int n = node_;
        for (const auto &m : line["uci"].toString().split(' ', Qt::SkipEmptyParts))
            n = game_.play(n, m.toStdString());
        refresh(true);
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::playComputer(bool humanWhite) {
    if (cardId_)
        return;
    humanWhite_ = humanWhite;
    playing_ = true;
    analysis_ = false;
    flipped_ = !humanWhite;
    startEngine();
    refresh();
}
void Controller::stopPlaying() {
    playing_ = false;
    analysis_ = false;
    analysisTimer_.stop();
    engine_.stop();
    refresh();
}
void Controller::createCard(const QString &title) {
    try {
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
        if (cardId_)
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
    if (!cardId_ || trainingComplete_)
        return;
    trainingFailed_ = true;
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
    if (!cardId_)
        return;
    game_ = beforeTraining_;
    node_ = beforeNode_;
    gameId_ = beforeId_;
    dirty_ = beforeDirty_;
    cardId_ = 0;
    trainingComplete_ = false;
    selected_ = -1;
    refresh();
}
void Controller::deleteCard(qint64 id) {
    try {
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
