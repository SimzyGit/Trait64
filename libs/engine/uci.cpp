// SPDX-License-Identifier: AGPL-3.0-or-later
#include "uci.h"
#include "game.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>

namespace t64 {
Uci::Uci(QObject *parent) : QObject(parent) {
    timeout_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, [this] {
        shutdown();
        emit status("Le moteur ne répond pas. Vérifiez l’exécutable UCI.");
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
    });
    connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int, QProcess::ExitStatus) {
                state_ = Off;
                timeout_.stop();
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
void Uci::start(const QString &executable, int threads, int hash, int multiPV, int skill) {
    shutdown();
    threads_ = std::clamp(threads, 1, 32);
    hash_ = std::clamp(hash, 16, 4096);
    multiPV_ = std::clamp(multiPV, 1, 5);
    skill_ = std::clamp(skill, 0, 20);
    if (!QFileInfo(executable).isExecutable()) {
        emit status("Choisissez un exécutable UCI valide (Stockfish).");
        return;
    }
    buffer_.clear();
    options_.clear();
    variations_.clear();
    pending_ = false;
    name_ = "Moteur UCI";
    state_ = Handshake;
    emit status("Connexion au moteur…");
    process_.setProgram(executable);
    process_.setArguments({});
    process_.start();
}
void Uci::shutdown() {
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
void Uci::analyse(const QString &fen, const QString &position, bool play, int milliseconds) {
    if (state_ == Off) {
        emit status("Démarrez un moteur dans les paramètres.");
        return;
    }
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
    activeFen_ = pendingFen_;
    activePlay_ = pendingPlay_;
    pending_ = false;
    variations_.clear();
    emit lines({});
    command(pendingPosition_);
    state_ = Searching;
    command(activePlay_ ? "go movetime " + QString::number(moveTime_) : "go depth 24");
    emit status(name_ + (activePlay_ ? " · réfléchit…" : " · analyse locale"));
}
void Uci::consume() {
    buffer_ += process_.readAllStandardOutput();
    if (buffer_.size() > 1024 * 1024) {
        shutdown();
        emit status("Réponse du moteur trop volumineuse.");
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
        options_.insert(line.mid(12).section(" type ", 0, 0));
        return;
    }
    if (line == "uciok" && state_ == Handshake) {
        for (const auto &pair : QList<QPair<QString, int>>{
                 {"Threads", threads_}, {"Hash", hash_}, {"MultiPV", multiPV_}, {"Skill Level", skill_}})
            if (options_.contains(pair.first))
                command("setoption name " + pair.first + " value " + QString::number(pair.second));
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
        const bool deliver = state_ == Searching && activePlay_ && !pending_;
        timeout_.stop();
        state_ = Ready;
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
        auto board = checkedBoard(activeFen_.toStdString());
        for (int i = pi + 1; i < parts.size() && uci.size() < 24; ++i) {
            if (!QRegularExpression("^[a-h][1-8][a-h][1-8][qrbn]?$").match(parts[i]).hasMatch())
                break;
            chess::Movelist legal;
            chess::movegen::legalmoves(legal, board);
            bool found = false;
            for (auto m : legal)
                if (chess::uci::moveToUci(m) == parts[i].toStdString()) {
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
    variations_[rank] = {
        {"rank", rank},        {"eval", eval},         {"score", score},       {"depth", value("depth")},
        {"nps", value("nps")}, {"san", san.join(' ')}, {"uci", uci.join(' ')}, {"fen", activeFen_}};
    QVariantList list;
    for (const auto &v : variations_)
        list << v;
    emit lines(list);
}
} // namespace t64
