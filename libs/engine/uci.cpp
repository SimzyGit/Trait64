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
QString Uci::optionValue(const QVariantMap &option, const QVariant &value) {
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
        const auto number = text.toLongLong(&ok);
        const auto minimum = option["min"].toLongLong(&minOk);
        const auto maximum = option["max"].toLongLong(&maxOk);
        if (!ok || !minOk || !maxOk || minimum > maximum || number < minimum || number > maximum)
            invalid();
        return QString::number(number);
    } else if (type == "combo") {
        if (!option["choices"].toStringList().contains(text))
            invalid();
        return text;
    } else if (type == "string")
        return text.isEmpty() ? "<empty>" : text;
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
        for (const auto &pair : QList<QPair<QString, int>>{
                 {"Threads", threads_}, {"Hash", hash_}, {"MultiPV", multiPV_}, {"Skill Level", skill_}})
            if (options_.contains(pair.first))
                command("setoption name " + pair.first + " value " + QString::number(pair.second));
        if (options_.contains("SyzygyPath") && !syzygyPath_.isEmpty())
            command("setoption name SyzygyPath value " + syzygyPath_);
        const QStringList managed{"Threads", "Hash", "MultiPV", "Skill Level", "UCI_Chess960", "SyzygyPath"};
        for (const auto &value : descriptors_) {
            const auto option = value.toMap();
            const auto name = option["name"].toString();
            if (!managed.contains(name) && customOptions_.contains(name)) {
                try {
                    command("setoption name " + name + " value " + optionValue(option, customOptions_[name]));
                } catch (const std::exception &e) {
                    shutdown();
                    emit failed(name + " : " + QString::fromUtf8(e.what()));
                    emit status(name + " : " + QString::fromUtf8(e.what()));
                    return;
                }
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
        beginSearch();
        return;
    }
    if (line.startsWith("bestmove ") && (state_ == Searching || state_ == Stopping)) {
        const bool complete = state_ == Searching && !pending_;
        const bool deliver = complete && activePlay_;
        timeout_.stop();
        state_ = Ready;
        if (complete) {
            QVariantList result;
            for (const auto &v : exactVariations_)
                result << v;
            emit completed(activeFen_, result);
        }
        if (deliver)
            emit bestMove(line.section(' ', 1, 1), activeFen_);
        beginSearch();
        return;
    }
    if (!line.startsWith("info ") || state_ != Searching || !line.contains(" pv "))
        return;
    const auto parts = line.split(' ', Qt::SkipEmptyParts);
    auto value = [&](const QString &key, QString fallback = QString()) {
        const int i = parts.indexOf(key);
        return i >= 0 && i + 1 < parts.size() ? parts[i + 1] : fallback;
    };
    const int rank = std::clamp(value("multipv", "1").toInt(), 1, 5);
    QString eval = "—";
    double score = 0;
    const int sign = activeFen_.section(' ', 1, 1) == "w" ? 1 : -1;
    const int si = parts.indexOf("score");
    if (si >= 0 && si + 2 < parts.size()) {
        const int raw = parts[si + 2].toInt() * sign;
        if (parts[si + 1] == "mate") {
            eval = (raw >= 0 ? "+M" : "−M") + QString::number(qAbs(raw));
            score = raw >= 0 ? 100 : -100;
        } else {
            score = raw / 100.0;
            eval = (score > 0 ? "+" : "") + QString::number(score, 'f', 2);
        }
        if (parts.contains("lowerbound") || parts.contains("upperbound"))
            eval += " ≈";
    }
    QStringList san, uci;
    const int pi = parts.indexOf("pv");
    try {
        auto board = checkedBoard(activeFen_.toStdString(), active960_);
        for (int i = pi + 1; i < parts.size() && uci.size() < 24; ++i) {
            if (!QRegularExpression("^[a-h][1-8][a-h][1-8][qrbn]?$").match(parts[i]).hasMatch())
                break;
            chess::Movelist legal;
            chess::movegen::legalmoves(legal, board);
            bool found = false;
            for (auto m : legal)
                if (chess::uci::moveToUci(m, active960_) == parts[i].toStdString()) {
                    san << QString::fromStdString(chess::uci::moveToSan(board, m));
                    uci << parts[i];
                    board.makeMove(m);
                    found = true;
                    break;
                }
            if (!found)
                break;
        }
    } catch (...) {
        return;
    }
    variations_[rank] = {{"rank", rank},
                         {"eval", eval},
                         {"score", score},
                         {"depth", value("depth")},
                         {"nps", value("nps")},
                         {"san", san.join(' ')},
                         {"uci", uci.join(' ')},
                         {"fen", activeFen_},
                         {"tbhits", value("tbhits", "0")},
                         {"bound", parts.contains("lowerbound") || parts.contains("upperbound")}};
    if (si >= 0 && !variations_[rank]["bound"].toBool() && !uci.empty())
        exactVariations_[rank] = variations_[rank];
    QVariantList list;
    for (const auto &v : variations_)
        list << v;
    emit lines(list);
}
} // namespace t64
