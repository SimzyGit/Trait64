// SPDX-License-Identifier: AGPL-3.0-or-later
#include "croissant.h"
#include "moveorder.h"
#include <QFileInfo>
#include <QIODevice>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <stdexcept>

namespace t64::croissant {
namespace {
// Les quatre octets réservés du format : au-delà de 251, ce n'est plus un rang de coup.
constexpr unsigned char VariationStart = 255, VariationEnd = 254, Comment = 253, Nag = 252;

void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
// Chaque base ouvre sa propre connexion, le temps de la lire : deux imports simultanés
// ne doivent pas se marcher dessus.
struct Connection {
    QString name = "croissant-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlDatabase db;
    explicit Connection(const QString &path) {
        db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(path);
        db.setConnectOptions("QSQLITE_OPEN_READONLY");
    }
    ~Connection() {
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
    }
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;
    bool open() { return db.open(); }
};
QString textValue(const QSqlQuery &query, const char *column) {
    const auto value = query.value(column);
    return value.isNull() ? QString() : value.toString().trimmed();
}
// Un résultat qui n'est pas l'un des quatre du PGN n'est pas un résultat : plutôt
// qu'écrire un fichier que personne ne relira, on note la partie comme inachevée.
QString resultOf(const QString &value) {
    static const QStringList known{"1-0", "0-1", "1/2-1/2", "*"};
    return known.contains(value) ? value : "*";
}
// Le PGN veut des guillemets et des contre-obliques échappés dans les en-têtes.
QString tagValue(QString value) {
    return value.replace('\\', "\\\\").replace('"', "\\\"").replace('\n', ' ');
}
struct Frame {
    chess::Board board;
    std::string text;
    // La position d'avant le dernier coup : c'est d'elle que part une variante.
    chess::Board before;
    bool hasMove = false;
    int number = 1;
    bool whiteToMove = true;
    bool needsNumber = true;
};
void appendMove(Frame &frame, const std::string &san) {
    if (!frame.text.empty())
        frame.text += ' ';
    if (frame.whiteToMove)
        frame.text += std::to_string(frame.number) + ". " + san;
    else {
        if (frame.needsNumber)
            frame.text += std::to_string(frame.number) + "... ";
        frame.text += san;
        ++frame.number;
    }
    frame.whiteToMove = !frame.whiteToMove;
    frame.needsNumber = false;
}
void appendToken(Frame &frame, const std::string &token) {
    if (!frame.text.empty())
        frame.text += ' ';
    frame.text += token;
}
Frame frameFor(const chess::Board &board) {
    Frame frame;
    frame.board = board;
    frame.before = board;
    frame.whiteToMove = board.sideToMove() == chess::Color::WHITE;
    frame.number = static_cast<int>(board.fullMoveNumber());
    frame.needsNumber = true;
    return frame;
}
} // namespace

std::string movetext(const QByteArray &blob, const std::string &fen) {
    std::vector<Frame> stack;
    stack.push_back(frameFor(chess::Board(fen)));
    int cursor = 0;
    const int size = int(blob.size());
    while (cursor < size) {
        const auto byte = static_cast<unsigned char>(blob[cursor++]);
        if (byte == VariationStart) {
            // La variante repart d'avant le dernier coup joué, comme dans un PGN.
            auto &parent = stack.back();
            auto child = frameFor(parent.hasMove ? parent.before : parent.board);
            stack.push_back(child);
            continue;
        }
        if (byte == VariationEnd) {
            require(stack.size() > 1, "Variante fermée sans avoir été ouverte.");
            const auto inner = stack.back().text;
            stack.pop_back();
            appendToken(stack.back(), "(" + inner + ")");
            continue;
        }
        if (byte == Comment || byte == Nag) {
            require(cursor + 2 <= size, "Longueur de commentaire tronquée.");
            const int length = static_cast<unsigned char>(blob[cursor]) |
                               (static_cast<unsigned char>(blob[cursor + 1]) << 8);
            cursor += 2;
            require(cursor + length <= size, "Commentaire tronqué.");
            auto payload = QString::fromUtf8(blob.mid(cursor, length)).toStdString();
            cursor += length;
            if (byte == Comment) {
                // Les accolades fermeraient le commentaire au milieu : on les remplace.
                for (auto &c : payload)
                    if (c == '{' || c == '}')
                        c = ' ';
                appendToken(stack.back(), "{" + payload + "}");
            } else
                appendToken(stack.back(), payload);
            continue;
        }
        auto &frame = stack.back();
        const auto moves = movesInShakmatyOrder(frame.board);
        require(byte < moves.size(), "Coup hors de la liste des coups légaux.");
        const auto move = moves[byte];
        const auto san = chess::uci::moveToSan(frame.board, move);
        frame.before = frame.board;
        frame.board.makeMove(move);
        frame.hasMove = true;
        appendMove(frame, san);
    }
    require(stack.size() == 1, "Variante restée ouverte.");
    return stack.front().text;
}

bool isCroissantBase(const QString &path) {
    if (!QFileInfo(path).isFile())
        return false;
    Connection connection(path);
    if (!connection.open())
        return false;
    const auto tables = connection.db.tables();
    // Le nouveau format d'En Croissant a une table « games » en minuscules et un tout
    // autre contenu : le confondre avec l'ancien donnerait des parties fausses.
    return tables.contains("Games") && tables.contains("Players") && tables.contains("Events");
}

QString title(const QString &path) {
    Connection connection(path);
    if (connection.open() && connection.db.tables().contains("Info")) {
        QSqlQuery query(connection.db);
        query.prepare("SELECT Value FROM Info WHERE Name = 'Title'");
        if (query.exec() && query.next()) {
            const auto found = query.value(0).toString().trimmed();
            if (!found.isEmpty())
                return found;
        }
    }
    return QFileInfo(path).completeBaseName();
}

int games(const QString &path) {
    Connection connection(path);
    if (!connection.open())
        return 0;
    QSqlQuery query(connection.db);
    return query.exec("SELECT COUNT(*) FROM Games") && query.next() ? query.value(0).toInt() : 0;
}

Outcome exportPgn(const QString &path, QIODevice &out, const std::function<void(int, int)> &progress,
                  const std::shared_ptr<std::atomic_bool> &canceled) {
    Connection connection(path);
    if (!connection.open())
        throw std::runtime_error("Base En Croissant illisible : " +
                                 connection.db.lastError().text().toStdString());
    const int total = [&] {
        QSqlQuery count(connection.db);
        return count.exec("SELECT COUNT(*) FROM Games") && count.next() ? count.value(0).toInt() : 0;
    }();
    QSqlQuery query(connection.db);
    query.setForwardOnly(true);
    require(query.exec("SELECT g.ID AS id, e.Name AS event, s.Name AS site, g.Date AS date, "
                       "g.UTCTime AS time, CAST(g.Round AS TEXT) AS round, w.Name AS white, "
                       "b.Name AS black, CAST(g.WhiteElo AS TEXT) AS white_elo, "
                       "CAST(g.BlackElo AS TEXT) AS black_elo, CAST(g.Result AS TEXT) AS result, "
                       "g.TimeControl AS time_control, g.ECO AS eco, g.FEN AS fen, g.Moves AS moves "
                       "FROM Games g LEFT JOIN Events e ON e.ID = g.EventID "
                       "LEFT JOIN Sites s ON s.ID = g.SiteID "
                       "LEFT JOIN Players w ON w.ID = g.WhiteID "
                       "LEFT JOIN Players b ON b.ID = g.BlackID ORDER BY g.ID"),
            "Schéma En Croissant inattendu.");
    Outcome outcome;
    QByteArray buffer;
    buffer.reserve(1 << 20);
    while (query.next()) {
        if (canceled && canceled->load())
            break;
        const auto fen = textValue(query, "fen");
        std::string moves;
        try {
            moves = movetext(query.value("moves").toByteArray(),
                             fen.isEmpty() ? std::string(chess::constants::STARTPOS) : fen.toStdString());
        } catch (const std::exception &) {
            // Une partie illisible n'arrête pas les neuf millions d'autres.
            ++outcome.skipped;
            continue;
        }
        const auto result = resultOf(textValue(query, "result"));
        QString tags;
        const auto add = [&tags](const char *name, const QString &value) {
            if (!value.isEmpty())
                tags += QString("[%1 \"%2\"]\n").arg(name, tagValue(value));
        };
        add("Event", textValue(query, "event"));
        add("Site", textValue(query, "site"));
        add("Date", textValue(query, "date"));
        add("Round", textValue(query, "round"));
        add("White", textValue(query, "white"));
        add("Black", textValue(query, "black"));
        tags += QString("[Result \"%1\"]\n").arg(result);
        add("WhiteElo", textValue(query, "white_elo"));
        add("BlackElo", textValue(query, "black_elo"));
        add("ECO", textValue(query, "eco"));
        add("TimeControl", textValue(query, "time_control"));
        add("UTCTime", textValue(query, "time"));
        if (!fen.isEmpty()) {
            tags += "[SetUp \"1\"]\n";
            add("FEN", fen);
        }
        buffer += tags.toUtf8();
        buffer += '\n';
        buffer += QByteArray::fromStdString(moves);
        if (!moves.empty())
            buffer += ' ';
        buffer += result.toUtf8() + "\n\n";
        ++outcome.written;
        if (buffer.size() > (1 << 20)) {
            require(out.write(buffer) == buffer.size(), "Écriture du PGN interrompue.");
            buffer.clear();
        }
        if (progress && outcome.written % 5000 == 0)
            progress(outcome.written, total);
    }
    if (!buffer.isEmpty())
        require(out.write(buffer) == buffer.size(), "Écriture du PGN interrompue.");
    if (progress)
        progress(outcome.written, total);
    return outcome;
}
} // namespace t64::croissant
