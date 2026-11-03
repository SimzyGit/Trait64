// SPDX-License-Identifier: AGPL-3.0-or-later
#include "online.h"
#include "puzzles.h"
#include <QProcess>
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
        {
            const QByteArray csv =
                "PuzzleId,FEN,Moves,Rating,RatingDeviation,Popularity,NbPlays,Themes,GameUrl,OpeningTags\n"
                "00aaa,r5k1/5ppp/8/8/8/8/5PPP/3R2K1 b - - 0 1,a8a7 d1d8,1200,75,95,1000,backRankMate mate mateIn1 oneMove,https://lichess.org/x#1,\n"
                "00bad,r5k1/5ppp/8/8/8/8/5PPP/3R2K1 b - - 0 1,a8a7 d1d9,1200,75,95,1000,mate,https://lichess.org/y#1,\n"
                "00hard,r5k1/5ppp/8/8/8/8/5PPP/3RR1K1 b - - 0 1,a8a7 d1d8,2500,75,90,10,mateIn1,https://lichess.org/z#1,\n";
            t64::PuzzleRecord record;
            check(t64::parseLichessPuzzle(QString::fromUtf8(csv.split('\n')[1]), record) && record.ref == "lichess:00aaa" &&
                      record.fen == "6k1/r4ppp/8/8/8/8/5PPP/3R2K1 w - - 1 2" && record.solution == "d1d8" &&
                      record.rating == 1200 && record.themes.contains("backRankMate"),
                  "Lichess row: opponent move applied, solver position and solution kept");
            check(!t64::parseLichessPuzzle(QString::fromUtf8(csv.split('\n')[0]), record) &&
                      !t64::parseLichessPuzzle(QString::fromUtf8(csv.split('\n')[2]), record),
                  "Header and illegal rows are skipped");
            QFile plain(dir.filePath("puzzles.csv"));
            check(plain.open(QIODevice::WriteOnly) && plain.write(csv) == csv.size(), "Write puzzle CSV");
            plain.close();
            auto read = [&](const QString &path, int max, int minimum, int maximum) {
                std::vector<t64::PuzzleRecord> all;
                const int n = t64::readLichessPuzzles(path, max, minimum, maximum,
                                                      [&](std::vector<t64::PuzzleRecord> &&b) { all.insert(all.end(), b.begin(), b.end()); });
                check(n == int(all.size()), "Reader reports what it delivered");
                return all;
            };
            check(read(plain.fileName(), 100, 0, 0).size() == 2 && read(plain.fileName(), 1, 0, 0).size() == 1 &&
                      read(plain.fileName(), 100, 2000, 3000).front().ref == "lichess:00hard",
                  "Reader honours the maximum and the rating range");
            if (QProcess::execute("gzip", {"-k", plain.fileName()}) == 0)
                check(read(plain.fileName() + ".gz", 100, 0, 0).size() == 2, "Compressed exports are read directly");
            bool refused = false;
            try {
                read(dir.filePath("missing.csv"), 10, 0, 0);
            } catch (const std::exception &) {
                refused = true;
            }
            check(refused, "A missing file is an error, not an empty bank");
        }
        std::cout << checks << " online checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
