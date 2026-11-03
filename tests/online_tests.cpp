// SPDX-License-Identifier: AGPL-3.0-or-later
#include "online.h"
#include <QCoreApplication>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>
#include <cstring>
#include <iostream>

int checks = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok) throw std::runtime_error(message);
}
struct Response {
    int status = 200;
    QByteArray body;
};
class Reply : public QNetworkReply {
  public:
    QByteArray bytes;
    qint64 offset = 0;
    Reply(const QNetworkRequest &request, Response response, QObject *parent)
        : QNetworkReply(parent), bytes(response.body) {
        setRequest(request);
        setUrl(request.url());
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response.status);
        QTimer::singleShot(0, this, [this] {
            if (isFinished()) return;
            setFinished(true);
            emit finished();
        });
    }
    void abort() override {
        if (isFinished()) return;
        setError(OperationCanceledError, "Canceled");
        setFinished(true);
        emit finished();
    }
    qint64 bytesAvailable() const override { return bytes.size() - offset + QNetworkReply::bytesAvailable(); }

  protected:
    qint64 readData(char *data, qint64 max) override {
        const auto n = std::min(max, bytes.size() - offset);
        if (!n) return -1;
        memcpy(data, bytes.constData() + offset, n);
        offset += n;
        return n;
    }
};
class Network : public QNetworkAccessManager {
  public:
    QList<Response> responses;
    QList<QNetworkRequest> requests;

  protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &r, QIODevice *) override {
        requests << r;
        return new Reply(r, responses.isEmpty() ? Response{500, {}} : responses.takeFirst(), this);
    }
};
QByteArray games(const QString &prefix, int count) {
    QByteArray out;
    for (int i = 1; i <= count; ++i)
        out += QString("[Event \"%1 %2\"]\n[Result \"*\"]\n\n1. e4 {[Event fake]} *\n\n").arg(prefix).arg(i).toUtf8();
    return out;
}
// Runs one fetch and returns (path or error, game count).
std::pair<QString, int> run(t64::OnlineGames &online, const std::function<void()> &start) {
    QSignalSpy done(&online, &t64::OnlineGames::finished), failed(&online, &t64::OnlineGames::failed);
    start();
    for (int i = 0; i < 200 && done.isEmpty() && failed.isEmpty(); ++i) QTest::qWait(5);
    if (!failed.isEmpty()) return {failed.first().first().toString(), -1};
    check(!done.isEmpty(), "Fetch completes");
    return {done.first().first().toString(), done.first().at(1).toInt()};
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir dir;
        const auto file = dir.filePath("online.pgn");
        int count = 0;
        check(t64::OnlineGames::lastGames(games("A", 5), 2, &count).startsWith("[Event \"A 4\"]") && count == 2,
              "Only the newest games are kept, comments are not split");
        check(t64::OnlineGames::validUser("Magnus_C-1") && !t64::OnlineGames::validUser("a/../b") &&
                  !t64::OnlineGames::validUser("x") && !t64::OnlineGames::validUser("a b"),
              "Usernames cannot change the URL path");

        Network network;
        t64::OnlineGames online(nullptr, &network);
        network.responses = {{200, games("L", 2)}};
        auto [path, lichess] = run(online, [&] { online.fetch("lichess", "Simzy", 50, file, 1700000000000); });
        const auto request = network.requests.last();
        const QUrlQuery query(request.url());
        check(path == file && lichess == 2, "Lichess games are saved");
        check(request.url().path() == "/api/games/user/Simzy" && query.queryItemValue("max") == "50" &&
                  query.queryItemValue("since") == "1700000000000" &&
                  request.rawHeader("Accept") == "application/x-chess-pgn" &&
                  !request.rawHeader("User-Agent").contains('@'),
              "Lichess request is incremental and carries no personal data");

        network.requests.clear();
        network.responses = {
            {200, R"({"archives":["https://api.chess.com/pub/player/simzy/games/2026/08","https://api.chess.com/pub/player/simzy/games/2026/09","https://api.chess.com/pub/player/simzy/games/2026/10"]})"},
            {200, games("Oct", 2)}, {200, games("Sep", 2)}};
        auto [chesscom, total] = run(online, [&] { online.fetch("chesscom", "Simzy", 3, file, 0); });
        QFile saved(file);
        check(saved.open(QIODevice::ReadOnly), "Chess.com PGN saved");
        const auto text = saved.readAll();
        check(total == 3 && network.requests.size() == 3 &&
                  network.requests[0].url().path() == "/pub/player/simzy/games/archives" &&
                  network.requests[1].url().path().endsWith("/2026/10/pgn") &&
                  text.indexOf("Sep 2") < text.indexOf("Oct 1") && !text.contains("Sep 1"),
              "Chess.com reads the newest months only, oldest game first");

        network.responses = {{200, R"({"archives":["https://evil.example/x"]})"}};
        check(run(online, [&] { online.fetch("chesscom", "simzy", 3, file); }).second == -1,
              "Archive URLs outside api.chess.com are refused");
        network.responses = {{404, {}}};
        check(run(online, [&] { online.fetch("lichess", "nobody", 3, file); }).first.contains("introuvable"),
              "Unknown player is reported");
        network.responses = {{200, {}}};
        check(run(online, [&] { online.fetch("lichess", "simzy", 3, file, 1); }).first.contains("Aucune nouvelle"),
              "No new games is reported");
        const int before = int(network.requests.size());
        check(run(online, [&] { online.fetch("lichess", "../x", 3, file); }).second == -1 &&
                  network.requests.size() == before,
              "Invalid username never reaches the network");
        network.responses = {{200, games("L", 1)}};
        check(run(online, [&] { online.fetch("lichess", "simzy", 3, file); online.cancel(); }).first.contains("annulé"),
              "A fetch can be canceled");
        std::cout << checks << " online checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
