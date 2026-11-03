// SPDX-License-Identifier: AGPL-3.0-or-later
#include "uci.h"
#include "game.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>

namespace t64 {
QVariantMap Uci::parseOption(const QString &line) {
    if (line.size() > 8192 || line.contains('\n') || line.contains('\r') || line.contains(QChar(0)))
        return {};
    static const QRegularExpression header(
        "^option name (.+?) type (check|spin|combo|string|button)(?: (.*))?$");
    const auto match = header.match(line);
    if (!match.hasMatch())
        return {};
    QVariantMap option{{"name", match.captured(1)}, {"type", match.captured(2)}};
    static const QRegularExpression fields(
        "(?:^| )(default|min|max|var) (.*?)(?= (?:default|min|max|var) |$)");
    auto it = fields.globalMatch(match.captured(3));
    QStringList choices;
    while (it.hasNext()) {
        auto field = it.next();
        auto value = field.captured(2);
        if (value == "<empty>")
            value.clear();
        if (field.captured(1) == "var")
            choices << value;
        else
            option[field.captured(1)] = value;
    }
    option["choices"] = choices;
    return option;
}
QString Uci::optionValue(const QVariantMap &option, const QVariant &value, bool clampSpin) {
    const auto text = value.toString();
    auto invalid = [] { throw std::runtime_error("Valeur UCI invalide ou hors limites."); };
    if (text.size() > 4096 || text.contains('\n') || text.contains('\r') || text.contains(QChar(0)))
        invalid();
    const auto type = option["type"].toString();
    if (type == "check") {
        if (text == "true" || text == "1")
            return "true";
        if (text == "false" || text == "0")
            return "false";
        invalid();
    } else if (type == "spin") {
        bool ok, minOk, maxOk;
        auto number = text.toLongLong(&ok);
        const auto minimum = option["min"].toLongLong(&minOk);
        const auto maximum = option["max"].toLongLong(&maxOk);
        if (!ok || !minOk || !maxOk || minimum > maximum)
            invalid();
        if (clampSpin)
            number = std::clamp(number, minimum, maximum);
        if (number < minimum || number > maximum)
            invalid();
        return QString::number(number);
    } else if (type == "combo") {
        if (!option["choices"].toStringList().contains(text))
            invalid();
        return text;
    } else if (type == "string")
        // <empty> is the descriptor/legacy-profile marker, not a literal command value.
        return text == "<empty>" ? QString() : text;
    invalid();
    return {};
}
bool Uci::pressOption(const QString &name) {
    if (state_ != Ready)
        return false;
    for (const auto &value : descriptors_) {
        auto option = value.toMap();
        if (option["name"] == name && option["type"] == "button") {
            command("setoption name " + name);
            state_ = WaitingReady;
            command("isready");
            timeout_.start(5000);
            return true;
        }
    }
    return false;
}
Uci::Uci(QObject *parent) : QObject(parent) {
    timeout_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, [this] {
        shutdown();
        emit status("Le moteur ne répond pas. Vérifiez l’exécutable UCI.");
        emit failed("Le moteur ne répond pas.");
    });
    connect(&process_, &QProcess::readyReadStandardOutput, this, &Uci::consume);
    connect(&process_, &QProcess::readyReadStandardError, this, [this] { process_.readAllStandardError(); });
    connect(&process_, &QProcess::started, this, [this] {
        command("uci");
        timeout_.start(5000);
    });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        state_ = Off;
        timeout_.stop();
        emit status("Moteur : " + process_.errorString());
        emit failed(process_.errorString());
    });
    connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int, QProcess::ExitStatus) {
                bool unexpected = state_ != Off;
                state_ = Off;
                timeout_.stop();
                if (unexpected)
                    emit failed("Le moteur s’est arrêté pendant son travail.");
                emit status("Moteur arrêté");
            });
}
Uci::~Uci() {
    shutdown();
}
void Uci::command(const QString &text) {
    if (process_.state() == QProcess::Running)
        process_.write(text.toUtf8() + '\n');
}
void Uci::start(const QString &executable, int threads, int hash, int multiPV, int skill,
                const QString &syzygyPath, const QVariantMap &customOptions) {
    shutdown();
    descriptors_.clear();
    options_.clear();
    emit optionsChanged();
    customOptions_ = customOptions;
    threads_ = std::clamp(threads, 1, 32);
    hash_ = std::clamp(hash, 16, 4096);
    multiPV_ = std::clamp(multiPV, 1, 5);
    skill_ = std::clamp(skill, 0, 20);
    syzygyPath_ = syzygyPath.contains('\n') || syzygyPath.contains('\r') ? QString() : syzygyPath;
    if (!QFileInfo(executable).isExecutable()) {
        emit status("Choisissez un exécutable UCI valide (Stockfish).");
        emit failed("Exécutable UCI invalide.");
        return;
    }
    buffer_.clear();
    options_.clear();
    variations_.clear();
    exactVariations_.clear();
    pending_ = false;
    name_ = "Moteur UCI";
    state_ = Handshake;
    emit status("Connexion au moteur…");
    process_.setProgram(executable);
    process_.setWorkingDirectory(QFileInfo(executable).absolutePath());
    process_.setArguments({});
    process_.start();
}
void Uci::shutdown() {
    ++generation_;
    state_ = Off;
    timeout_.stop();
    pending_ = false;
    if (process_.state() != QProcess::NotRunning) {
        command("stop");
        command("quit");
        if (!process_.waitForFinished(500)) {
            process_.kill();
            process_.waitForFinished(500);
        }
    }
    state_ = Off;
    activePlay_ = false;
}
void Uci::analyse(const QString &fen, const QString &position, bool play, int milliseconds, int depth,
                  bool variant960) {
    if (state_ == Off) {
        emit status("Démarrez un moteur dans les paramètres.");
        return;
    }
    if (position.contains('\n') || position.contains('\r')) {
        emit failed("Commande de position invalide.");
        return;
    }
    pending960_ = variant960;
    ++generation_;
    depth_ = std::clamp(depth, 1, 99);
    pendingFen_ = fen;
    pendingPosition_ = position;
    pendingPlay_ = play;
    moveTime_ = std::clamp(milliseconds, 100, 10000);
    pending_ = true;
    if (state_ == Searching) {
        state_ = Stopping;
        command("stop");
        timeout_.start(5000);
    } else if (state_ == Ready)
        beginSearch();
}
void Uci::stop() {
    ++generation_;
    pending_ = false;
    activePlay_ = false;
    if (state_ == Searching) {
        state_ = Stopping;
        command("stop");
        timeout_.start(5000);
    }
}
void Uci::beginSearch() {
    if (!pending_ || state_ != Ready)
        return;
    if (pending960_ && !options_.contains("UCI_Chess960")) {
        pending_ = false;
        emit failed("Ce moteur ne déclare pas la prise en charge de Chess960.");
        return;
    }
    active960_ = pending960_;
    if (options_.contains("UCI_Chess960"))
        command(QString("setoption name UCI_Chess960 value ") + (active960_ ? "true" : "false"));
    activeFen_ = pendingFen_;
    activePlay_ = pendingPlay_;
    pending_ = false;
    variations_.clear();
    exactVariations_.clear();
    emit lines({});
    command(pendingPosition_);
    state_ = Searching;
    command(activePlay_ ? "go movetime " + QString::number(moveTime_)
                        : "go depth " + QString::number(depth_));
    if (activePlay_)
        timeout_.start(moveTime_ + 10000);
    emit status(name_ + (activePlay_ ? " · réfléchit…" : " · analyse locale"));
}
void Uci::consume() {
    buffer_ += process_.readAllStandardOutput();
    if (buffer_.size() > 1024 * 1024) {
        shutdown();
        emit status("Réponse du moteur trop volumineuse.");
        emit failed("Réponse du moteur trop volumineuse.");
        return;
    }
    qsizetype pos;
    while ((pos = buffer_.indexOf('\n')) >= 0) {
        const auto line = QString::fromUtf8(buffer_.left(pos)).trimmed();
        buffer_.remove(0, pos + 1);
        parse(line);
    }
}
void Uci::parse(const QString &line) {
    if (line.startsWith("id name ")) {
        name_ = line.mid(8);
        return;
    }
    if (line.startsWith("option name ")) {
        const auto option = parseOption(line);
        const auto name = option["name"].toString();
        if (state_ == Handshake && !option.isEmpty() && descriptors_.size() < 256 &&
            !options_.contains(name)) {
            options_.insert(name);
            descriptors_ << option;
        }
        return;
    }
    if (line == "uciok" && state_ == Handshake) {
        QVariantMap automatic{{"Threads", threads_}, {"Hash", hash_},
                              {"MultiPV", multiPV_}, {"Skill Level", skill_}};
        QStringList managed{"Threads", "Hash", "MultiPV", "Skill Level", "UCI_Chess960", "SyzygyPath"};
        appliedElo_ = 0;
        if (limitElo_ > 0 && options_.contains("UCI_LimitStrength") && options_.contains("UCI_Elo")) {
            // An Elo target replaces Skill Level, which would otherwise weaken the engine twice.
            automatic["Skill Level"] = 20;
            automatic["UCI_LimitStrength"] = true;
            automatic["UCI_Elo"] = limitElo_;
            managed << "UCI_LimitStrength" << "UCI_Elo";
            for (const auto &value : descriptors_)
                if (value.toMap()["name"] == "UCI_Elo")
                    appliedElo_ = optionValue(value.toMap(), limitElo_, true).toInt();
        }
        for (const auto &value : descriptors_) {
            const auto option = value.toMap();
            const auto name = option["name"].toString();
            try {
                if (automatic.contains(name))
                    command("setoption name " + name + " value " + optionValue(option, automatic[name], true));
                else if (name == "SyzygyPath" && !syzygyPath_.isEmpty())
                    command("setoption name " + name + " value " + optionValue(option, syzygyPath_));
                else if (!managed.contains(name) && customOptions_.contains(name))
                    command("setoption name " + name + " value " + optionValue(option, customOptions_[name]));
            } catch (const std::exception &e) {
                shutdown();
                emit failed(name + " : " + QString::fromUtf8(e.what()));
                emit status(name + " : " + QString::fromUtf8(e.what()));
                return;
            }
        }
        emit optionsChanged();
        state_ = WaitingReady;
        command("isready");
        timeout_.start(5000);
        return;
    }
    if (line == "readyok" && state_ == WaitingReady) {
        timeout_.stop();
        state_ = Ready;
        emit status(name_ + " · prêt");
        emit initialized();
        beginSearch();
        return;
    }
    if (line.startsWith("bestmove ") && (state_ == Searching || state_ == Stopping)) {
        const bool complete = state_ == Searching && !pending_;
        const bool deliver = complete && activePlay_;
        const auto generation = generation_;
        const auto completedFen = activeFen_;
        timeout_.stop();
        state_ = Ready;
        if (complete) {
            QVariantList result;
            for (const auto &v : exactVariations_)
                result << v;
            emit completed(completedFen, result);
        }
        if (deliver && generation == generation_)
            emit bestMove(line.section(' ', 1, 1), completedFen);
        beginSearch();
        return;
    }
    if (!line.startsWith("info ") || state_ != Searching || !line.contains(" pv "))
        return;
    auto info = parseInfo(line, activeFen_, active960_);
    if (info.isEmpty())
        return;
    const int rank = info["rank"].toInt();
    variations_[rank] = info;
    if (!info["bound"].toBool() && !info["uci"].toString().isEmpty())
        exactVariations_[rank] = info;
    QVariantList list;
    for (const auto &v : variations_)
        list << v;
    emit lines(list);
}
QVariantMap Uci::parseInfo(const QString &line, const QString &fen, bool chess960) {
    if (!line.startsWith("info ") || line.startsWith("info string ") || line.size() > 65536)
        return {};
    const int pv = line.indexOf(" pv ");
    if (pv < 0)
        return {};
    const auto parts = line.left(pv).split(' ', Qt::SkipEmptyParts);
    auto value = [&](const QString &key, QString fallback = QString()) {
        const int i = parts.indexOf(key);
        return i >= 0 && i + 1 < parts.size() ? parts[i + 1] : fallback;
    };
    bool rankOk, scoreOk;
    const int rank = value("multipv", "1").toInt(&rankOk);
    if (!rankOk || rank < 1 || rank > 5)
        return {};
    const int sign = fen.section(' ', 1, 1) == "w" ? 1 : -1;
    const int si = parts.indexOf("score");
    if (si < 0 || si + 2 >= parts.size() || (parts[si + 1] != "mate" && parts[si + 1] != "cp"))
        return {};
    const int raw = parts[si + 2].toInt(&scoreOk);
    if (!scoreOk || raw < -1000000 || raw > 1000000)
        return {};
    const bool mate = parts[si + 1] == "mate";
    const int oriented = raw * sign;
    // Mate zero means the side to move is mated. The plot ordinate is never a cp value.
    const int winner = raw == 0 ? -sign : (oriented > 0 ? 1 : -1);
    const double score = mate ? winner * 100. : oriented / 100.;
    QString eval = mate ? (winner > 0 ? "+M" : "−M") + QString::number(qAbs(raw))
                        : (score > 0 ? "+" : "") + QString::number(score, 'f', 2);
    const bool bound = parts.contains("lowerbound") || parts.contains("upperbound");
    if (bound)
        eval += " ≈";
    QStringList san, uci;
    try {
        auto board = checkedBoard(fen.toStdString(), chess960);
        const auto moves = line.mid(pv + 4).split(' ', Qt::SkipEmptyParts);
        static const QRegularExpression movePattern("^[a-h][1-8][a-h][1-8][qrbn]?$");
        for (const auto &move : moves) {
            if (uci.size() >= 24 || !movePattern.match(move).hasMatch())
                break;
            chess::Movelist legal;
            chess::movegen::legalmoves(legal, board);
            bool found = false;
            for (auto m : legal)
                if (chess::uci::moveToUci(m, chess960) == move.toStdString()) {
                    san << QString::fromStdString(chess::uci::moveToSan(board, m));
                    uci << move;
                    board.makeMove(m);
                    found = true;
                    break;
                }
            if (!found)
                break;
        }
    } catch (...) {
        return {};
    }
    auto number = [&](const QString &key) -> qulonglong {
        bool ok;
        auto n = value(key, "0").toULongLong(&ok);
        return ok ? n : 0;
    };
    QVariantMap result = {{"rank", rank},
                          {"eval", eval},
                          {"score", score},
                          {"scoreType", mate ? "mate" : "cp"},
                          {"cp", mate ? QVariant() : QVariant(oriented)},
                          {"mate", mate ? QVariant(oriented) : QVariant()},
                          {"winner", mate ? winner : 0},
                          {"depth", number("depth")},
                          {"nps", number("nps")},
                          {"nodes", number("nodes")},
                          {"time", number("time")},
                          {"san", san.join(' ')},
                          {"uci", uci.join(' ')},
                          {"fen", fen},
                          {"tbhits", number("tbhits")},
                          {"bound", bound}};
    const int wi = parts.indexOf("wdl");
    if (wi >= 0 && wi + 3 < parts.size()) {
        bool a, b, c;
        const int w = parts[wi + 1].toInt(&a), d = parts[wi + 2].toInt(&b), l = parts[wi + 3].toInt(&c);
        if (a && b && c && w >= 0 && w <= 1000 && d >= 0 && d <= 1000 && l >= 0 && l <= 1000 &&
            w + d + l == 1000)
            result["wdl"] = QVariantList{sign > 0 ? w : l, d, sign > 0 ? l : w};
    }
    return result;
}
} // namespace t64
