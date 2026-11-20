// SPDX-License-Identifier: AGPL-3.0-or-later
#include "broadcasts.h"
#include "explorer.h"
#include "fide.h"
#include "lichess_login.h"
#include "online.h"
#include "puzzles.h"
#include "syzygy.h"
#include "syzygy_download.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QSet>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUrlQuery>
#include <cstring>
#include <iostream>
#include <memory>

int checks = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
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
            if (isFinished())
                return;
            setFinished(true);
            emit finished();
        });
    }
    void abort() override {
        if (isFinished())
            return;
        setError(OperationCanceledError, "Canceled");
        setFinished(true);
        emit finished();
    }
    qint64 bytesAvailable() const override { return bytes.size() - offset + QNetworkReply::bytesAvailable(); }

  protected:
    qint64 readData(char *data, qint64 max) override {
        const auto n = std::min(max, bytes.size() - offset);
        if (!n)
            return -1;
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
        out += QString("[Event \"%1 %2\"]\n[Result \"*\"]\n\n1. e4 {[Event fake]} *\n\n")
                   .arg(prefix)
                   .arg(i)
                   .toUtf8();
    return out;
}
// Runs one fetch and returns (path or error, game count).
std::pair<QString, int> run(t64::OnlineGames &online, const std::function<void()> &start) {
    QSignalSpy done(&online, &t64::OnlineGames::finished), failed(&online, &t64::OnlineGames::failed);
    start();
    for (int i = 0; i < 200 && done.isEmpty() && failed.isEmpty(); ++i)
        QTest::qWait(5);
    if (!failed.isEmpty())
        return {failed.first().first().toString(), -1};
    check(!done.isEmpty(), "Fetch completes");
    return {done.first().first().toString(), done.first().at(1).toInt()};
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir dir;
        const auto file = dir.filePath("online.pgn");
        int count = 0;
        check(t64::OnlineGames::lastGames(games("A", 5), 2, &count).startsWith("[Event \"A 4\"]") &&
                  count == 2,
              "Only the newest games are kept, comments are not split");
        check(t64::OnlineGames::validUser("Magnus_C-1") && !t64::OnlineGames::validUser("a/../b") &&
                  !t64::OnlineGames::validUser("x") && !t64::OnlineGames::validUser("a b"),
              "Usernames cannot change the URL path");

        Network network;
        t64::OnlineGames online(nullptr, &network);
        network.responses = {{200, games("L", 2)}};
        auto [path, lichess] =
            run(online, [&] { online.fetch("lichess", "Simzy", 50, file, 1700000000000); });
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
            {200,
             R"({"archives":["https://api.chess.com/pub/player/simzy/games/2026/08","https://api.chess.com/pub/player/simzy/games/2026/09","https://api.chess.com/pub/player/simzy/games/2026/10"]})"},
            {200, games("Oct", 2)},
            {200, games("Sep", 2)}};
        auto [chesscom, total] = run(online, [&] { online.fetch("chesscom", "Simzy", 3, file, 0); });
        QFile saved(file);
        check(saved.open(QIODevice::ReadOnly), "Chess.com PGN saved");
        const auto text = saved.readAll();
        // Windows refuse de remplacer un fichier encore ouvert, là où POSIX l'accepte :
        // garder ce descripteur faisait échouer l'écriture de la synchronisation suivante.
        saved.close();
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
        {
            QSignalSpy upToDate(&online, &t64::OnlineGames::upToDate);
            network.responses = {{200, {}}};
            online.fetch("lichess", "simzy", 3, file, 1);
            check(upToDate.wait(2000), "An account with no new game is reported as up to date");
            // Chess.com sync: only the months since the last sync are downloaded.
            network.requests.clear();
            network.responses = {
                {200,
                 R"({"archives":["https://api.chess.com/pub/player/simzy/games/2026/08","https://api.chess.com/pub/player/simzy/games/2026/09","https://api.chess.com/pub/player/simzy/games/2026/10"]})"},
                {200, games("Oct", 1)},
                {200, games("Sep", 1)}};
            const qint64 september15 = QDateTime(QDate(2026, 9, 15), QTime(12, 0)).toMSecsSinceEpoch();
            auto [path, count] =
                run(online, [&] { online.fetch("chesscom", "simzy", 50, file, september15); });
            // Trois affirmations distinctes : réunies sous un seul message, un échec ne
            // disait pas laquelle avait cédé, et les valeurs observées restaient muettes.
            if (count != 2 || network.requests.size() != 3)
                std::cout << "Chess.com sync observed " << count << " game(s) over "
                          << network.requests.size() << " request(s), last "
                          << network.requests.last().url().toString().toStdString() << '\n';
            check(network.requests.size() == 3,
                  "Chess.com sync asks for the archive list and the two months since the last sync");
            check(!network.requests.last().url().path().contains("/08/"),
                  "Chess.com sync skips months before the last sync");
            check(count == 2, "Chess.com sync keeps one game from each downloaded month");
            network.responses = {
                {200, R"({"archives":["https://api.chess.com/pub/player/simzy/games/2026/08"]})"}};
            upToDate.clear();
            online.fetch("chesscom", "simzy", 50, file, september15);
            check(upToDate.wait(2000), "Chess.com account without newer months is up to date");
        }
        const int before = int(network.requests.size());
        check(run(online, [&] { online.fetch("lichess", "../x", 3, file); }).second == -1 &&
                  network.requests.size() == before,
              "Invalid username never reaches the network");
        network.responses = {{200, games("L", 1)}};
        check(run(online,
                  [&] {
                      online.fetch("lichess", "simzy", 3, file);
                      online.cancel();
                  })
                  .first.contains("annulé"),
              "A fetch can be canceled");
        {
            const QByteArray csv =
                "PuzzleId,FEN,Moves,Rating,RatingDeviation,Popularity,NbPlays,Themes,GameUrl,OpeningTags\n"
                "00aaa,r5k1/5ppp/8/8/8/8/5PPP/3R2K1 b - - 0 1,a8a7 d1d8,1200,75,95,1000,backRankMate mate "
                "mateIn1 oneMove,https://lichess.org/x#1,\n"
                "00bad,r5k1/5ppp/8/8/8/8/5PPP/3R2K1 b - - 0 1,a8a7 "
                "d1d9,1200,75,95,1000,mate,https://lichess.org/y#1,\n"
                "00hard,r5k1/5ppp/8/8/8/8/5PPP/3RR1K1 b - - 0 1,a8a7 "
                "d1d8,2500,75,90,10,mateIn1,https://lichess.org/z#1,\n";
            t64::PuzzleRecord record;
            check(t64::parseLichessPuzzle(QString::fromUtf8(csv.split('\n')[1]), record) &&
                      record.ref == "lichess:00aaa" &&
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
                const int n = t64::readLichessPuzzles(
                    path, max, minimum, maximum,
                    [&](std::vector<t64::PuzzleRecord> &&b) { all.insert(all.end(), b.begin(), b.end()); });
                check(n == int(all.size()), "Reader reports what it delivered");
                return all;
            };
            check(read(plain.fileName(), 100, 0, 0).size() == 2 &&
                      read(plain.fileName(), 1, 0, 0).size() == 1 &&
                      read(plain.fileName(), 100, 2000, 3000).front().ref == "lichess:00hard",
                  "Reader honours the maximum and the rating range");
            if (QProcess::execute("gzip", {"-k", plain.fileName()}) == 0)
                check(read(plain.fileName() + ".gz", 100, 0, 0).size() == 2,
                      "Compressed exports are read directly");
            bool refused = false;
            try {
                read(dir.filePath("missing.csv"), 10, 0, 0);
            } catch (const std::exception &) {
                refused = true;
            }
            check(refused, "A missing file is an error, not an empty bank");
        }
        {
            // Lecture des listes FIDE. Le format publié place chaque joueur dans un
            // <player> ; on vérifie surtout qu'une liste énorme se lit par morceaux, car
            // la liste standard dépasse le million de joueurs.
            const QByteArray head = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<playerslist>\n";
            const QByteArray wanted =
                "<player><fideid>1503014</fideid><name>Nom, Prenom</name><country>FRA</country>"
                "<sex>M</sex><title>g</title><rating>2612</rating><games>14</games><k>10</k>"
                "<birthday>1990</birthday></player>\n";
            const QByteArray other =
                "<player><fideid>600001</fideid><name>Autre, Joueur</name><country>NOR</country>"
                "<rating>2100</rating><games>0</games><birthday>1985</birthday></player>\n";
            QByteArray document = head;
            // Le joueur cherché est noyé au milieu, pour qu'un parcours partiel se voie.
            for (int i = 0; i < 500; ++i)
                document += other;
            document += wanted;
            for (int i = 0; i < 500; ++i)
                document += other;
            document += "</playerslist>\n";

            auto source = [&document](int blockSize) {
                auto offset = std::make_shared<int>(0);
                return [&document, offset, blockSize]() -> QByteArray {
                    if (*offset >= document.size())
                        return {};
                    const auto block = document.mid(*offset, blockSize);
                    *offset += block.size();
                    return block;
                };
            };
            const auto found = t64::FideList::findInChunks(source(4096), "1503014");
            check(found.found && found.name == "Nom, Prenom" && found.rating == 2612 &&
                      found.country == "FRA" && found.title == "g" && found.games == 14 &&
                      found.birthYear == 1990,
                  "A player is found in a long rating list");
            // Blocs minuscules : un élément, et même un texte, se retrouve coupé en deux.
            // Le lecteur doit recoller sans perdre de caractères.
            const auto split = t64::FideList::findInChunks(source(7), "1503014");
            check(split.found && split.name == "Nom, Prenom" && split.rating == 2612,
                  "Fields split across blocks are reassembled");
            check(!t64::FideList::findInChunks(source(4096), "999999999").found,
                  "An absent player is reported as absent, not as an error");
            check(t64::FideList::validId("1503014") && !t64::FideList::validId("150a014") &&
                      !t64::FideList::validId(""),
                  "Only a numeric identifier is accepted");
            check(t64::FideList::listUrl("standard").toString().endsWith("standard_rating_list_xml.zip") &&
                      t64::FideList::listUrl("bullet").isEmpty(),
                  "Only the three published speeds have a list");
            bool refusedXml = false;
            try {
                auto broken = std::make_shared<bool>(false);
                t64::FideList::findInChunks(
                    [broken]() -> QByteArray {
                        if (*broken)
                            return {};
                        *broken = true;
                        return "<playerslist><player><fideid>1</fideid></wrong>";
                    },
                    "1503014");
            } catch (const std::exception &) {
                refusedXml = true;
            }
            check(refusedXml, "Malformed XML is an error, not a silent absence");
        }
        {
            // L'explorateur en ligne : l'adresse construite et la réponse lue, sans réseau.
            using t64::OpeningExplorer;
            const auto start = QString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
            const auto lichess =
                OpeningExplorer::buildUrl("lichess", start,
                                          {{"speeds", QStringList{"blitz", "rapid", "pigeon"}},
                                           {"ratings", QStringList{"2000", "9999"}},
                                           {"since", "2019-01"}});
            check(lichess.host() == "explorer.lichess.ovh" && lichess.path() == "/lichess",
                  "The explorer is asked on its own host");
            const QUrlQuery asked(lichess.query());
            check(asked.queryItemValue("speeds") == "blitz,rapid" &&
                      asked.queryItemValue("ratings") == "2000" && asked.queryItemValue("since") == "2019-01",
                  "Unknown speeds and rating bands are dropped rather than sent");
            const QUrlQuery mastersAsked(
                OpeningExplorer::buildUrl("masters", start, {{"since", "2019-01"}, {"until", "1952"}})
                    .query());
            // Les maîtres comptent en années : un mois y ferait une erreur 400.
            check(!mastersAsked.hasQueryItem("since") && mastersAsked.queryItemValue("until") == "1952" &&
                      !mastersAsked.hasQueryItem("speeds"),
                  "The masters base takes years, and takes no speed filter");
            check(OpeningExplorer::buildUrl("elsewhere", start, {}).isEmpty() &&
                      OpeningExplorer::buildUrl("lichess", "", {}).isEmpty(),
                  "An unknown base and an empty position ask nothing");
            const auto body = QByteArray(R"({"white":10,"draws":4,"black":6,
                "opening":{"eco":"B90","name":"Sicilian Defense: Najdorf"},
                "moves":[{"uci":"e2e4","san":"e4","white":7,"draws":2,"black":3,"averageRating":2310}],
                "topGames":[{"id":"abcd1234","winner":"black","year":1972,"month":7,
                    "white":{"name":"Spassky","rating":2660},"black":{"name":"Fischer","rating":2785}}]})");
            const auto read = OpeningExplorer::parse(body);
            check(read["total"].toInt() == 20 && read["name"] == "B90 Sicilian Defense: Najdorf",
                  "Totals and opening name are read as the library's own report");
            const auto move = read["moves"].toList().first().toMap();
            check(move["san"] == "e4" && move["count"].toInt() == 12 && move["rating"].toInt() == 2310,
                  "A move carries its own total, not the position's");
            const auto top = read["games"].toList().first().toMap();
            check(top["result"] == "0-1" && top["date"] == "1972.07" && top["whiteElo"].toInt() == 2660 &&
                      top["black"] == "Fischer",
                  "A top game reads as players, result and date");
            check(OpeningExplorer::parse("not json").contains("error") &&
                      OpeningExplorer::parse("[]").contains("error"),
                  "A malformed answer is an error, not an empty position");
            // Les tables de finales : sept pièces au plus, et le verdict de chaque coup
            // retourné pour se lire du camp au trait.
            const auto ending = QString("8/8/8/8/8/4k3/4p3/4K3 w - - 0 1");
            check(OpeningExplorer::pieces(ending) == 3 && OpeningExplorer::pieces(start) == 32 &&
                      OpeningExplorer::buildUrl("tablebase", ending, {}).host() == "tablebase.lichess.ovh" &&
                      OpeningExplorer::buildUrl("tablebase", start, {}).isEmpty(),
                  "Tablebases are asked for seven pieces or fewer, and nothing beyond");
            const auto won = OpeningExplorer::parseTablebase(R"({"category":"win","dtz":3,"dtm":17,
                "moves":[{"uci":"h7h8q","san":"h8=Q+","category":"loss","dtz":-2,"dtm":-16},
                         {"uci":"g7g8","san":"Kg8","category":"draw","dtz":0,"dtm":null},
                         {"uci":"g7h8","san":"Kh8","category":"win","dtz":4,"dtm":12}]})");
            const auto lines = won["moves"].toList();
            check(won["verdict"] == "win" && won["dtm"].toInt() == 17 &&
                      lines[0].toMap()["verdict"] == "win" && lines[0].toMap()["dtm"].toInt() == 16 &&
                      lines[1].toMap()["verdict"] == "draw" && !lines[1].toMap()["dtm"].isValid() &&
                      lines[2].toMap()["verdict"] == "loss",
                  "A move that leaves the opponent lost reads as winning for the side to move");
            const auto already = OpeningExplorer::parseTablebase(
                R"({"category":"win","moves":[{"uci":"a1a2","san":"Ra2","category":"win"}]})");
            check(already["moves"].toList()[0].toMap()["verdict"] == "win",
                  "An answer already given from the mover's side is not turned over a second time");
            check(OpeningExplorer::parseTablebase("oops").contains("error"),
                  "A malformed tablebase answer is an error");
            // Le nuage de Lichess : trois lignes au plus, évaluées du côté des Blancs, mises en
            // notation ; un mat garde son nombre de coups.
            const auto cloudUrl = OpeningExplorer::buildUrl("cloud", start, {});
            check(cloudUrl.host() == "lichess.org" && cloudUrl.path() == "/api/cloud-eval" &&
                      QUrlQuery(cloudUrl).queryItemValue("multiPv") == "3" &&
                      QUrlQuery(cloudUrl).queryItemValue("fen", QUrl::FullyDecoded) == start,
                  "The cloud evaluation asks Lichess for three lines");
            const auto cloud = OpeningExplorer::parseCloud(
                R"({"fen":"x","knodes":12000,"depth":40,"pvs":[{"moves":"e2e4 e7e5 g1f3","cp":31},
                {"moves":"d2d4 d7d5","cp":-5},{"moves":"g1f3","mate":-3}]})",
                start);
            const auto cloudLines = cloud["moves"].toList();
            check(cloud["cloud"].toBool() && cloud["depth"].toInt() == 40 && cloudLines.size() == 3 &&
                      cloudLines[0].toMap()["san"] == "e4" &&
                      cloudLines[0].toMap()["line"] == "1. e4 e5 2. Nf3" &&
                      cloudLines[0].toMap()["eval"] == "+0.31" && cloudLines[1].toMap()["eval"] == "-0.05" &&
                      cloudLines[2].toMap()["eval"] == "#-3" && cloudLines[2].toMap()["score"].toDouble() < 0,
                  "Cloud lines read in notation, from White's side, mates kept as mates");
            // L'exemple de la documentation de Lichess : roque écrit du roi vers la tour.
            const auto spanish = OpeningExplorer::parseCloud(
                R"({"fen":"r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3","knodes":203656,
                "depth":60,"pvs":[{"moves":"g8f6 e1h1 f6e4 f1e1 e4d6 f3e5 c6e5 e1e5 f8e7 b5f1","cp":18},
                {"moves":"a7a6 b5a4 g8f6 e1h1 f8e7 f1e1 b7b5 a4b3 e8h8 a2a4","cp":19}]})",
                "r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3");
            const auto berlin = spanish["moves"].toList();
            check(berlin.size() == 2 &&
                      berlin[0].toMap()["line"].toString().startsWith("3… Nf6 4. O-O Nxe4 5. Re1") &&
                      berlin[1].toMap()["line"].toString().contains("O-O") &&
                      berlin[1].toMap()["line"].toString().split(' ').size() > 12,
                  "Lichess's king-to-rook castling reads as O-O, to the end of the line");
            check(OpeningExplorer::parseCloud("oops", start).contains("error"),
                  "A malformed cloud answer is an error");
            {
                // Les tables de l'ordinateur, lues par Fathom : KQK, KRK, KPK, KBK, KNK et
                // KQKQ, rangées dans un sous-dossier comme les paquets de Lichess.
                QTemporaryDir home;
                QDir().mkpath(home.filePath("3-4"));
                for (const auto &name : QDir(QString(T64_TEST_DATA) + "/syzygy").entryList(QDir::Files))
                    QFile::copy(QString(T64_TEST_DATA) + "/syzygy/" + name, home.filePath("3-4/" + name));
                const auto tables = t64::syzygy::open(home.path());
                check(tables.files == 6 && tables.largest == 4, "Tables in a subfolder are found and sized");
                // Roi et pion contre roi : seuls les pas de roi de côté gagnent.
                const auto pawn = t64::syzygy::probe("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
                const auto rows = pawn["moves"].toList();
                QStringList winning;
                for (const auto &row : rows)
                    if (row.toMap()["verdict"] == "win")
                        winning << row.toMap()["san"].toString();
                winning.sort();
                check(pawn["local"].toBool() && pawn["verdict"] == "win" && rows.size() == 6 &&
                          winning == QStringList{"Kd2", "Kf2"} && rows.last().toMap()["verdict"] == "draw",
                      "King and pawn: the winning king steps come first, the pushes draw");
                const auto defence = t64::syzygy::probe("4k3/8/8/8/8/8/4P3/4K3 b - - 0 1");
                check(defence["verdict"] == "draw" &&
                          defence["moves"].toList().last().toMap()["verdict"] == "loss",
                      "Seen from the defender, the wrong king step loses");
                const auto mate = t64::syzygy::probe("k7/8/1K6/8/8/8/8/7Q b - - 0 1");
                check(mate["verdict"] == "loss" && mate["moves"].toList().size() == 1,
                      "A lost king has its one move");
                const auto mated = t64::syzygy::probe("8/8/8/8/8/1k6/1q6/K7 w - - 0 1");
                check(mated["verdict"] == "loss" && mated["moves"].toList().isEmpty(),
                      "A mated king has no move");
                check(t64::syzygy::probe("8/8/8/4k3/8/8/2RP4/4K3 w - - 0 1").isEmpty() &&
                          t64::syzygy::probe("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1").isEmpty() &&
                          t64::syzygy::probe("not a fen").isEmpty(),
                      "Five pieces, castling rights or nonsense are left to Lichess");
                OpeningExplorer offline;
                offline.setLocalTables(home.path());
                QSignalSpy answered(&offline, &OpeningExplorer::ready);
                offline.query("tablebase", "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
                check(answered.size() == 1 && answered.first().first().toMap()["local"].toBool() &&
                          !offline.busy(),
                      "The explorer answers from the computer's tables without the network");
                // Les noms des paquets : 145 tables jusqu'à cinq pièces, 365 à six, sans doublon,
                // et ceux des fichiers de test en font partie.
                QStringList upToFive;
                for (const int n : {3, 4, 5})
                    upToFive << t64::syzygy::tableNames(n);
                const auto six = t64::syzygy::tableNames(6);
                check(t64::syzygy::tableNames(3).size() == 5 && t64::syzygy::tableNames(4).size() == 30 &&
                          upToFive.size() == 145 &&
                          QSet<QString>(upToFive.cbegin(), upToFive.cend()).size() == 145 &&
                          six.size() == 365 && upToFive.contains("KQvKQ") && upToFive.contains("KBNvK") &&
                          upToFive.contains("KRPvKB") && !upToFive.contains("KBvKQ") &&
                          six.contains("KQRvKQR"),
                      "Table names follow Fathom's order, stronger side first");
                // La somme de contrôle de fin de fichier.
                const auto table = QString(T64_TEST_DATA) + "/syzygy/KQvK.rtbz";
                QFile original(table);
                original.open(QIODevice::ReadOnly);
                const auto good = original.readAll();
                auto broken = good;
                broken[100] = char(broken[100] ^ 1);
                const auto write = [](const QString &path, const QByteArray &bytes) {
                    QFile out(path);
                    return out.open(QIODevice::WriteOnly) && out.write(bytes) == bytes.size();
                };
                check(write(home.filePath("good.rtbz"), good) && write(home.filePath("bad.rtbz"), broken) &&
                          write(home.filePath("short.rtbz"), good.left(good.size() - 10)),
                      "Write checksum samples");
                check(t64::syzygy::intact(home.filePath("good.rtbz")) &&
                          !t64::syzygy::intact(home.filePath("bad.rtbz")) &&
                          !t64::syzygy::intact(home.filePath("short.rtbz")),
                      "A table checks its own checksum: one flipped bit or a cut file is seen");
                check(t64::SyzygyDownload::candidates("6", "KQRvKQR.rtbz")
                              .first()
                              .endsWith("/6-dtz/KQRvKQR.rtbz") &&
                          t64::SyzygyDownload::candidates("3-4-5", "KQvK.rtbw").size() == 3,
                      "Each file has its mirrors");
                // Un téléchargement : tout est là sauf trois fichiers. Le premier manque au
                // premier miroir, le deuxième reprend une partie déjà reçue, le troisième
                // arrive abîmé et s'arrête là.
                QTemporaryDir store;
                const auto pack = QDir(store.path()).filePath("3-4-5");
                QDir().mkpath(pack);
                QStringList wanted;
                for (const auto &name : upToFive)
                    wanted << name + ".rtbw" << name + ".rtbz";
                const QStringList missing{"KQvK.rtbw", "KQvK.rtbz", "KRvK.rtbw"};
                for (const auto &file : wanted)
                    if (!missing.contains(file))
                        write(QDir(pack).filePath(file), QByteArray(80, 'x'));
                const auto fixture = [](const QString &name) {
                    QFile in(QString(T64_TEST_DATA) + "/syzygy/" + name);
                    in.open(QIODevice::ReadOnly);
                    return in.readAll();
                };
                const auto queen = fixture("KQvK.rtbz"), rook = fixture("KRvK.rtbw");
                write(QDir(pack).filePath("KQvK.rtbz.part"), queen.left(100));
                auto damaged = rook;
                damaged[50] = char(damaged[50] ^ 4);
                Network mirrors;
                mirrors.responses = {
                    {404, {}}, {200, fixture("KQvK.rtbw")}, {206, queen.mid(100)}, {200, damaged}};
                t64::SyzygyDownload download(nullptr, &mirrors);
                QSignalSpy ended(&download, &t64::SyzygyDownload::finished);
                download.start(store.path(), "3-4-5");
                for (int i = 0; i < 400 && ended.isEmpty(); ++i)
                    QTest::qWait(5);
                check(ended.size() == 1 && ended.first().first().toString().contains("KRvK.rtbw") &&
                          t64::syzygy::intact(QDir(pack).filePath("KQvK.rtbw")) &&
                          t64::syzygy::intact(QDir(pack).filePath("KQvK.rtbz")) &&
                          !QFile::exists(QDir(pack).filePath("KRvK.rtbw.part")) &&
                          !QFile::exists(QDir(pack).filePath("KRvK.rtbw")),
                      "Files arrive checked, a damaged one is thrown away and stops the download");
                check(mirrors.requests.size() == 4 &&
                          mirrors.requests[1].url().path().contains("3-4-5-wdl") &&
                          mirrors.requests[2].rawHeader("Range") == "bytes=100-",
                      "A missing file is asked of the next mirror, a partial one resumes");
                mirrors.responses = {{200, rook}};
                download.start(store.path(), "3-4-5");
                for (int i = 0; i < 400 && ended.size() < 2; ++i)
                    QTest::qWait(5);
                check(ended.size() == 2 && ended.last().first().toString().isEmpty() &&
                          t64::syzygy::intact(QDir(pack).filePath("KRvK.rtbw")) &&
                          mirrors.requests.size() == 5,
                      "Started again, only the missing file is fetched");
                // Retransmissions : la liste, une ronde, son PGN, sans réseau.
                const QByteArray top =
                    R"({"active":[{"tour":{"id":"t1","name":"Tata Steel Masters","slug":"tata-steel-masters","tier":5},
                    "round":{"id":"abcdefgh","name":"Round 5","slug":"round-5","ongoing":true,"startsAt":1}}],
                    "past":{"currentPageResults":[{"tour":{"name":"Bad","slug":"../x"},"round":{"id":"zz","name":"R","slug":"r"}},
                    {"tour":{"name":"Old Open","slug":"old-open"},"round":{"id":"hgfedcba","name":"Round 9","slug":"round-9","finishedAt":5}}]}})";
                const auto tours = t64::Broadcasts::parseTop(top);
                check(tours.size() == 2 && tours[0].toMap()["ongoing"].toBool() &&
                          tours[0].toMap()["roundId"] == "abcdefgh" && tours[1].toMap()["finished"].toBool(),
                      "Broadcast list: live first, an unsafe slug is dropped");
                const QByteArray roundJson =
                    R"({"round":{"id":"abcdefgh","name":"Round 5","ongoing":true},"tour":{"name":"Tata Steel Masters"},
                    "games":[{"id":"g1","name":"A - B","fen":"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1","lastMove":"e2e4",
                    "status":"½-½","players":[{"name":"Gukesh D","title":"GM","rating":2783,"fed":"IND","clock":540000},
                    {"name":"Abdusattorov, Nodirbek","title":"GM","rating":2766,"clock":12300}]}]})";
                const auto parsed = t64::Broadcasts::parseRound(roundJson);
                const auto board = parsed["games"].toList().value(0).toMap();
                check(parsed["round"] == "Round 5" && board["status"] == "1/2-1/2" &&
                          board["lastMove"] == "e2e4" && board["white"].toMap()["clock"].toInt() == 5400 &&
                          board["black"].toMap()["name"] == "Abdusattorov, Nodirbek" &&
                          t64::Broadcasts::parseRound("oops").contains("error"),
                      "Broadcast round: players, clocks in seconds, ½-½ read as a draw");
                {
                    Network lichess;
                    lichess.responses = {
                        {200, top}, {200, roundJson}, {200, "[Event \"X\"]\n\n1. e4 *\n"}, {429, {}}};
                    t64::Broadcasts feed(nullptr, &lichess);
                    QSignalSpy listed(&feed, &t64::Broadcasts::listed),
                        ready(&feed, &t64::Broadcasts::roundReady), pgn(&feed, &t64::Broadcasts::pgnReady),
                        failed(&feed, &t64::Broadcasts::failed);
                    feed.list();
                    feed.round("tata-steel-masters", "round-5", "abcdefgh");
                    feed.pgn("abcdefgh");
                    feed.pgn("abcdefgh");
                    feed.round("../etc", "x", "y");
                    for (int i = 0; i < 200 && (listed.isEmpty() || ready.isEmpty() || pgn.isEmpty() ||
                                                failed.size() < 2);
                         ++i)
                        QTest::qWait(5);
                    check(listed.size() == 1 && ready.size() == 1 && pgn.size() == 1 && failed.size() == 2 &&
                              lichess.requests.size() == 4 &&
                              lichess.requests[1].url().path() ==
                                  "/api/broadcast/tata-steel-masters/round-5/abcdefgh" &&
                              lichess.requests[2].url().toString().endsWith(
                                  "/api/broadcast/round/abcdefgh.pgn?clocks=true"),
                          "Broadcasts ask Lichess the right addresses, refuse an unsafe one and report a "
                          "slow-down");
                }
                check(t64::syzygy::open({}).largest == 0 &&
                          t64::syzygy::probe("k7/8/1K6/8/8/8/8/7Q b - - 0 1").isEmpty(),
                      "Closing the tables leaves every ending to Lichess");
            }
            Network network;
            OpeningExplorer explorer(nullptr, &network);
            QSignalSpy ready(&explorer, &OpeningExplorer::ready);
            QSignalSpy failed(&explorer, &OpeningExplorer::failed);
            network.responses << Response{200, body};
            explorer.query("masters", start);
            for (int i = 0; i < 200 && ready.isEmpty() && failed.isEmpty(); ++i)
                QTest::qWait(5);
            check(ready.size() == 1 && failed.isEmpty() &&
                      ready.first().first().toMap()["total"].toInt() == 20,
                  "A real asynchronous query hands back the report");
            network.responses << Response{429, "slow down"};
            explorer.query("lichess", start);
            for (int i = 0; i < 200 && failed.isEmpty(); ++i)
                QTest::qWait(5);
            check(failed.size() == 1 && failed.first().first().toString().contains("ralentir"),
                  "Being rate-limited is said plainly");
        }
        {
            // Connexion à Lichess (OAuth2 PKCE, comme En Croissant). Le défi attendu est
            // calculé à part (hashlib de Python) ; le parcours complet passe par un faux
            // Lichess : retour du navigateur sur le port local, échange du code, puis
            // identité du compte.
            check(t64::LichessLogin::challenge("dBjftJeZ4CVP-mJ92K9qPpFHwRnZ8xUMBdGwvTyW-cM") ==
                      "1wHdrIKq_XNdXa0jVUWLQ7ing83pS-ieBtEEvxYtSJM",
                  "The PKCE challenge is the base64url SHA-256 of the verifier");
            Network network;
            network.responses << Response{200, R"({"token_type":"Bearer","access_token":"lip_abcdef123"})"}
                              << Response{200, R"({"id":"simzy","username":"Simzy"})"};
            t64::LichessLogin login(nullptr, &network);
            login.setBase("https://lichess.test");
            QSignalSpy connected(&login, &t64::LichessLogin::connected),
                failed(&login, &t64::LichessLogin::failed);
            check(login.start() && login.running(), "The local sign-in server listens");
            const QUrlQuery asked(login.authorizeUrl());
            const QUrl redirect(asked.queryItemValue("redirect_uri", QUrl::FullyDecoded));
            check(login.authorizeUrl().host() == "lichess.test" &&
                      asked.queryItemValue("code_challenge_method") == "S256" &&
                      redirect.host() == "127.0.0.1" && !asked.queryItemValue("scope").size(),
                  "The browser is sent to Lichess with a PKCE challenge, a loopback redirect and no scope");
            // Une requête étrangère (mauvais état) n'interrompt pas l'attente.
            const auto browse = [&](const QString &query) {
                QTcpSocket browser;
                browser.connectToHost("127.0.0.1", quint16(redirect.port()));
                check(browser.waitForConnected(2000), "The browser reaches the local server");
                browser.write(("GET /?" + query + " HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n").toUtf8());
                browser.flush();
                // Lu au fil de l'eau : le serveur vit dans ce même fil, il faut lui rendre la main.
                QByteArray received;
                for (int i = 0; i < 100 && browser.state() == QAbstractSocket::ConnectedState; ++i) {
                    QTest::qWait(10);
                    received += browser.readAll();
                }
                return received + browser.readAll();
            };
            check(browse("code=forged&state=wrong").contains("200 OK"), "A stray request still gets a page");
            check(login.running() && network.requests.isEmpty(), "A request with the wrong state is ignored");
            const auto page = browse("code=good&state=" + asked.queryItemValue("state"));
            for (int i = 0; i < 200 && connected.isEmpty() && failed.isEmpty(); ++i)
                QTest::qWait(5);
            check(page.contains("200 OK") && !connected.isEmpty() && connected.first().at(0) == "Simzy" &&
                      connected.first().at(1) == "lip_abcdef123" && !login.running(),
                  "The code is exchanged for a token, then the token names its account");
            check(network.requests.size() == 2 && network.requests[0].url().path() == "/api/token" &&
                      network.requests[1].rawHeader("Authorization") == "Bearer lip_abcdef123",
                  "The token exchange and the account lookup go to Lichess, the second with the token");
        }
        std::cout << checks << " online checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
