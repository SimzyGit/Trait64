// SPDX-License-Identifier: AGPL-3.0-or-later
#include "scid.h"
#include "game.h"
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <algorithm>
#include <array>
#include <climits>
#include <map>
#include <stdexcept>
#include <vector>

namespace t64::scid {
namespace {
constexpr int headerSize = 182, entrySize = 47;

// Les nombres de SCID s'écrivent l'octet de poids fort d'abord.
struct Reader {
    const unsigned char *at, *end;
    unsigned take(int bytes) {
        if (end - at < bytes)
            throw std::runtime_error("Fichier SCID tronqué.");
        unsigned value = 0;
        for (int i = 0; i < bytes; ++i)
            value = (value << 8) | *at++;
        return value;
    }
};

// Le chemin sans extension, et le format (4 ou 5) que dit l'extension ; 0 sinon.
std::pair<QString, int> base(const QString &path) {
    const QFileInfo file(path);
    const auto suffix = file.suffix().toLower();
    const int format = suffix.size() == 3 && suffix[0] == 's' && QStringLiteral("ing").contains(suffix[1])
                           ? suffix[2] == '4'   ? 4
                             : suffix[2] == '5' ? 5
                                                : 0
                           : 0;
    return {format ? file.absolutePath() + "/" + file.completeBaseName() : QString(), format};
}
QString suffix(char kind, int format) {
    return QStringLiteral(".s%1%2").arg(kind).arg(format);
}

QByteArray readAll(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error(("Lecture impossible : " + path).toStdString());
    return file.readAll();
}

// Les quatre listes de noms : joueurs, tournois, lieux, rondes. Chaque nom ne garde que
// ce qui le distingue du précédent (codage par préfixe), avec son numéro à lui.
std::array<std::vector<std::string>, 4> readNames(const QByteArray &bytes) {
    Reader in{reinterpret_cast<const unsigned char *>(bytes.constData()),
              reinterpret_cast<const unsigned char *>(bytes.constData()) + bytes.size()};
    if (bytes.size() < 8 || std::string(bytes.constData(), 7) != "Scid.sn")
        throw std::runtime_error("Fichier de noms SCID illisible.");
    in.at += 8;
    in.take(4); // horodatage, inutilisé
    std::array<unsigned, 4> counts{}, frequencies{};
    for (auto &count : counts)
        count = in.take(3);
    for (auto &maximum : frequencies)
        maximum = in.take(3);
    std::array<std::vector<std::string>, 4> names;
    for (int kind = 0; kind < 4; ++kind) {
        names[kind].resize(counts[kind]);
        std::string previous;
        for (unsigned i = 0; i < counts[kind]; ++i) {
            const unsigned id = in.take(counts[kind] >= 65536 ? 3 : 2);
            in.take(frequencies[kind] >= 65536 ? 3 : frequencies[kind] >= 256 ? 2 : 1);
            const unsigned length = in.take(1), prefix = i ? in.take(1) : 0;
            if (prefix > length || prefix > previous.size() || id >= counts[kind] ||
                in.end - in.at < int(length - prefix))
                throw std::runtime_error("Fichier de noms SCID endommagé.");
            previous.resize(prefix);
            previous.append(reinterpret_cast<const char *>(in.at), length - prefix);
            in.at += length - prefix;
            names[kind][id] = previous;
        }
    }
    return names;
}

struct Entry {
    unsigned offset = 0, length = 0, flags = 0, result = 0, date = 0, whiteElo = 0, blackElo = 0;
    unsigned white = 0, black = 0, event = 0, site = 0, round = 0;
};

Entry readEntry(Reader in) {
    Entry e;
    e.offset = in.take(4);
    const unsigned low = in.take(2), extra = in.take(1);
    e.length = ((extra & 0x80) << 9) | low;
    e.flags = ((extra & 0x3F) << 16) | in.take(2);
    const unsigned players = in.take(1);
    e.white = ((players & 0xF0) << 12) | in.take(2);
    e.black = ((players & 0x0F) << 16) | in.take(2);
    const unsigned places = in.take(1);
    e.event = ((places & 0xE0) << 11) | in.take(2);
    e.site = ((places & 0x1C) << 14) | in.take(2);
    e.round = ((places & 0x03) << 16) | in.take(2);
    e.result = (in.take(2) >> 12) & 0x0F;
    in.take(2); // code ECO : Trait64 reconnaît l'ouverture lui-même
    e.date = in.take(4) & 0xFFFFF;
    e.whiteElo = in.take(2) & 0xFFF;
    e.blackElo = in.take(2) & 0xFFF;
    return e;
}

// Le format 5 : des noms à la suite, chacun précédé d'un entier variable (longueur × 8
// + type ; types 0 à 3 comme au format 4, 4 pour les informations de la base).
std::array<std::vector<std::string>, 4> readNames5(const QByteArray &bytes) {
    const auto *at = reinterpret_cast<const unsigned char *>(bytes.constData());
    const auto *end = at + bytes.size();
    std::array<std::vector<std::string>, 4> names;
    while (at < end) {
        quint64 value = 0;
        for (int shift = 0;; shift += 7) {
            if (at == end || shift > 56)
                throw std::runtime_error("Fichier de noms SCID endommagé.");
            value |= quint64(*at & 127) << shift;
            if (*at++ < 128)
                break;
        }
        const auto kind = unsigned(value & 7);
        const auto length = value >> 3;
        if (kind > 4 || length > quint64(end - at))
            throw std::runtime_error("Fichier de noms SCID endommagé.");
        if (kind < 4)
            names[kind].emplace_back(reinterpret_cast<const char *>(at), size_t(length));
        at += length;
    }
    return names;
}

// Une fiche du format 5 : 56 octets, douze entiers de 32 bits écrits poids faible
// d'abord, dont les bits de tête portent un second champ, puis les pions de départ.
Entry readEntry5(const unsigned char *at, quint64 &offset) {
    const auto word = [at](int i) {
        const auto *b = at + 4 * i;
        return unsigned(b[0]) | unsigned(b[1]) << 8 | unsigned(b[2]) << 16 | unsigned(b[3]) << 24;
    };
    const auto low = [](unsigned value, int high) { return value & (0xFFFFFFFFu >> high); };
    Entry e;
    e.white = low(word(0), 4);
    e.black = low(word(1), 4);
    e.event = low(word(2), 4);
    e.site = word(3);
    e.round = low(word(4), 1);
    e.whiteElo = word(5) >> 20;
    e.date = low(word(5), 12);
    e.blackElo = word(6) >> 20;
    e.flags = low(word(7), 10);
    e.length = word(8) >> 15;
    offset = quint64(low(word(8), 17)) << 32 | word(9);
    e.result = (word(11) >> 16) & 3;
    return e;
}

std::string dateText(unsigned date) {
    const unsigned year = date >> 9, month = (date >> 5) & 15, day = date & 31;
    const auto two = [](unsigned n) {
        return n ? (n < 10 ? "0" : "") + std::to_string(n) : std::string("??");
    };
    return (year ? std::to_string(year) : std::string("????")) + "." + two(month) + "." + two(day);
}

// La position telle que SCID la tient : en plus de l'échiquier, une liste des pièces de
// chaque camp, le roi toujours en tête. Un coup désigne sa pièce par son rang dans cette
// liste ; une prise retire la pièce prise en mettant la dernière de la liste à sa place.
struct Lists {
    std::array<std::array<int, 16>, 2> list{};
    std::array<int, 2> count{};
    std::array<int, 64> rank{};

    explicit Lists(const chess::Board &board) {
        rank.fill(-1);
        // La position de départ a son ordre à elle : le roi, puis les pièces de a à h sans
        // lui, puis les pions de a à h.
        if (board.getFen() == chess::constants::STARTPOS) {
            static constexpr int order[] = {4, 0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
            for (int side = 0; side < 2; ++side) {
                for (int i = 0; i < 16; ++i)
                    place(side, i, side == 0 ? order[i] : (order[i] < 8 ? 56 + order[i] : 48 + order[i] - 8));
                count[side] = 16;
            }
            return;
        }
        // Le même ordre que SCID : la FEN se lit de la huitième rangée à la première, de
        // a à h, et un roi rencontré prend la tête en repoussant la pièce qui y était.
        for (int row = 7; row >= 0; --row)
            for (int file = 0; file < 8; ++file) {
                const int square = row * 8 + file;
                const auto piece = board.at(chess::Square(square));
                if (piece == chess::Piece::NONE)
                    continue;
                const int side = piece.color() == chess::Color::WHITE ? 0 : 1;
                if (count[side] >= 16)
                    throw std::runtime_error("Trop de pièces.");
                if (piece.type() == chess::PieceType::KING && count[side] > 0) {
                    const int moved = list[side][0];
                    list[side][count[side]] = moved;
                    rank[moved] = count[side];
                    list[side][0] = square;
                    rank[square] = 0;
                } else {
                    list[side][count[side]] = square;
                    rank[square] = count[side];
                }
                ++count[side];
            }
    }
    void place(int side, int index, int square) {
        list[side][index] = square;
        rank[square] = index;
    }
    void remove(int side, int square) {
        const int index = rank[square];
        const int last = list[side][--count[side]];
        list[side][index] = last;
        rank[last] = index;
        rank[square] = -1;
    }
};

// Le coup que désigne un octet de SCID, cherché parmi les coups légaux de la position.
struct Decoded {
    chess::Move move;
    int extraBytes = 0;
};

Decoded decodeMove(const chess::Board &board, const Lists &lists, unsigned char code,
                   const unsigned char *next, const unsigned char *end) {
    const int side = board.sideToMove() == chess::Color::WHITE ? 0 : 1;
    const int index = code >> 4, value = code & 0x0F;
    if (index >= lists.count[side])
        throw std::runtime_error("Pièce inconnue.");
    const int from = lists.list[side][index];
    const auto type = board.at(chess::Square(from)).type();
    int to = -1, extra = 0;
    chess::PieceType promotion = chess::PieceType::NONE;
    int castle = 0; // 1 : petit roque, -1 : grand roque
    const int fromFile = from & 7, fromRank = from >> 3;
    if (type == chess::PieceType::PAWN) {
        static constexpr int steps[] = {7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9, 16};
        to = side == 0 ? from + steps[value] : from - steps[value];
        if (value >= 3 && value <= 14) {
            static constexpr chess::PieceType::underlying kinds[] = {
                chess::PieceType::QUEEN, chess::PieceType::ROOK, chess::PieceType::BISHOP,
                chess::PieceType::KNIGHT};
            promotion = chess::PieceType(kinds[(value - 3) / 3]);
        }
    } else if (type == chess::PieceType::BISHOP) {
        const int shift = value % 8 - fromFile;
        to = value >= 8 ? from - 7 * shift : from + 9 * shift;
    } else if (type == chess::PieceType::KNIGHT) {
        static constexpr int steps[] = {0, -17, -15, -10, -6, 6, 10, 15, 17};
        if (value < 1 || value > 8)
            throw std::runtime_error("Coup de cavalier illisible.");
        to = from + steps[value];
    } else if (type == chess::PieceType::QUEEN && value == fromFile) {
        // Les coups de dame en diagonale prennent un second octet : la case + 64.
        if (next >= end)
            throw std::runtime_error("Coup de dame tronqué.");
        to = int(*next) - 64;
        extra = 1;
    } else if (type == chess::PieceType::QUEEN || type == chess::PieceType::ROOK) {
        to = value >= 8 ? (value - 8) * 8 + fromFile : fromRank * 8 + value;
    } else if (type == chess::PieceType::KING) {
        static constexpr int steps[] = {0, -9, -8, -7, -1, 1, 7, 8, 9};
        if (value == 0)
            throw std::runtime_error("Coup nul non pris en charge.");
        if (value <= 8)
            to = from + steps[value];
        else if (value == 9 || value == 10)
            castle = value == 10 ? 1 : -1;
        else
            throw std::runtime_error("Coup de roi illisible.");
    }
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    for (const auto &move : moves) {
        if (castle) {
            if (move.typeOf() == chess::Move::CASTLING && (move.to() > move.from()) == (castle > 0))
                return {move, 0};
            continue;
        }
        if (move.typeOf() == chess::Move::CASTLING || int(move.from().index()) != from ||
            int(move.to().index()) != to)
            continue;
        if (move.typeOf() == chess::Move::PROMOTION ? move.promotionType() == promotion
                                                    : promotion == chess::PieceType::NONE)
            return {move, extra};
    }
    throw std::runtime_error("Coup illégal dans la base SCID.");
}

// Met la liste de SCID à jour pour un coup, avant qu'il soit joué sur l'échiquier.
void follow(Lists &lists, const chess::Board &board, const chess::Move &move) {
    const int side = board.sideToMove() == chess::Color::WHITE ? 0 : 1, other = 1 - side;
    const int from = move.from().index(), to = move.to().index();
    if (move.typeOf() == chess::Move::CASTLING) {
        // chess.hpp note le roque du roi vers sa tour : roi en g ou c, tour en f ou d.
        const bool king = to > from;
        const int base = side == 0 ? 0 : 56;
        const int rookIndex = lists.rank[to];
        lists.rank[from] = lists.rank[to] = -1;
        lists.place(side, 0, base + (king ? 6 : 2));
        lists.place(side, rookIndex, base + (king ? 5 : 3));
        return;
    }
    int taken = to;
    if (move.typeOf() == chess::Move::ENPASSANT)
        taken = side == 0 ? to - 8 : to + 8;
    if (board.at(chess::Square(taken)) != chess::Piece::NONE)
        lists.remove(other, taken);
    const int index = lists.rank[from];
    lists.rank[from] = -1;
    lists.place(side, index, to);
}

std::string pgnOf(const QByteArray &record, const Entry &entry,
                  const std::array<std::vector<std::string>, 4> &names) {
    const auto *at = reinterpret_cast<const unsigned char *>(record.constData());
    const auto *end = at + record.size();
    const auto need = [&](qsizetype bytes) {
        if (end - at < bytes)
            throw std::runtime_error("Partie SCID tronquée.");
    };
    // Les étiquettes en plus des sept habituelles. Celles qui reviennent souvent tiennent
    // sur un seul octet, au-dessus de 240.
    static const char *const common[] = {"WhiteCountry", "BlackCountry", "Annotator", "PlyCount", "EventDate",
                                         "Opening",      "Variation",    "Setup",     "Source",   "SetUp"};
    std::map<std::string, std::string> extra;
    for (;;) {
        need(1);
        const unsigned length = *at++;
        if (length == 0)
            break;
        std::string name;
        if (length > 240) {
            if (length - 241 < 10)
                name = common[length - 241];
        } else {
            need(length);
            name.assign(reinterpret_cast<const char *>(at), length);
            at += length;
        }
        need(1);
        const unsigned size = length == 255 ? 3 : *at++;
        need(size);
        if (!name.empty())
            extra[name] = std::string(reinterpret_cast<const char *>(at), size);
        at += size;
    }
    need(1);
    std::string fen(chess::constants::STARTPOS);
    if (*at++ & 1) {
        const auto *stop = std::find(at, end, 0);
        if (stop == end)
            throw std::runtime_error("Position de départ tronquée.");
        fen.assign(reinterpret_cast<const char *>(at), stop - at);
        at = stop + 1;
    }
    // Chess960 : les droits de roque écrits par colonne (« HAha ») le trahissent.
    const auto castling = fen.substr(fen.find(' ', fen.find(' ') + 1) + 1);
    const bool variant960 = castling.find_first_of("ABCDEFGHabcdefgh") < castling.find(' ');
    Game game(fen, variant960);

    // Le parcours des coups : chaque variante repart de la position d'avant le dernier coup
    // joué, et la fin de variante ramène là où on en était.
    struct Line {
        int node = 0, before = 0;
        Lists lists, previous;
    };
    std::vector<Line> stack;
    Lists start(game.boardOf(0));
    Line line{0, 0, start, start};
    std::vector<int> marks; // nœuds qui attendent un commentaire, dans l'ordre
    std::vector<bool> beforeMove;
    for (;;) {
        need(1);
        const unsigned char code = *at++;
        if (code == 11) { // annotation
            need(1);
            if (line.node)
                game.nodes[line.node].nags.push_back(*at);
            ++at;
        } else if (code == 12) { // un commentaire suit ce coup
            marks.push_back(line.node);
            beforeMove.push_back(false);
        } else if (code == 13) { // début de variante
            if (!line.node)
                throw std::runtime_error("Variante sans coup.");
            stack.push_back(line);
            line = Line{line.before, line.before, line.previous, line.previous};
            // Un commentaire juste après l'ouverture de la variante la précède.
            if (at < end && *at == 12) {
                ++at;
                marks.push_back(-1 - int(stack.size()));
                beforeMove.push_back(true);
            }
        } else if (code == 14) { // fin de variante
            if (stack.empty())
                throw std::runtime_error("Fin de variante en trop.");
            line = stack.back();
            stack.pop_back();
        } else if (code == 15) { // fin de partie
            if (!stack.empty())
                throw std::runtime_error("Partie terminée dans une variante.");
            break;
        } else {
            const auto &board = game.boardOf(line.node);
            const auto decoded = decodeMove(board, line.lists, code, at, end);
            at += decoded.extraBytes;
            Lists after = line.lists;
            follow(after, board, decoded.move);
            const int child = game.play(line.node, chess::uci::moveToUci(decoded.move, variant960));
            // Un commentaire placé en tête de variante attendait ce premier coup.
            for (size_t i = 0; i < marks.size(); ++i)
                if (marks[i] == -1 - int(stack.size()))
                    marks[i] = -1000000 - child;
            line = Line{child, line.node, after, line.lists};
        }
    }
    // Les commentaires suivent, dans l'ordre des marques, chacun terminé par un zéro.
    for (size_t i = 0; i < marks.size(); ++i) {
        const auto *stop = std::find(at, end, 0);
        if (stop == end)
            break;
        std::string text(reinterpret_cast<const char *>(at), stop - at);
        at = stop + 1;
        if (text.empty())
            continue;
        int node = marks[i];
        if (node <= -1000000)
            node = -1000000 - node; // le commentaire d'avant le coup, gardé avec lui
        else if (node < 0)
            continue;
        auto &comment = game.nodes[node].comment;
        comment = comment.empty() ? text : comment + " " + text;
    }

    const auto name = [&](int kind, unsigned id) {
        return id < names[kind].size() && !names[kind][id].empty() ? names[kind][id] : std::string("?");
    };
    static const char *const results[] = {"*", "1-0", "0-1", "1/2-1/2"};
    game.tags["Event"] = name(1, entry.event);
    game.tags["Site"] = name(2, entry.site);
    game.tags["Date"] = dateText(entry.date);
    game.tags["Round"] = name(3, entry.round);
    game.tags["White"] = name(0, entry.white);
    game.tags["Black"] = name(0, entry.black);
    game.tags["Result"] = results[entry.result & 3];
    if (entry.whiteElo)
        game.tags["WhiteElo"] = std::to_string(entry.whiteElo);
    if (entry.blackElo)
        game.tags["BlackElo"] = std::to_string(entry.blackElo);
    for (const auto &[tag, value] : extra)
        if (!game.tags.contains(tag) && tag != "FEN" && tag != "SetUp" && tag != "Setup" && tag != "PlyCount")
            game.tags[tag] = value;
    return game.pgn();
}
} // namespace

QString indexPath(const QString &path) {
    const auto [stem, format] = base(path);
    return stem.isEmpty() ? QString() : stem + suffix('i', format);
}

bool isScidBase(const QString &path) {
    const auto [stem, format] = base(path);
    if (stem.isEmpty() || !QFileInfo(stem + suffix('n', format)).isFile() ||
        !QFileInfo(stem + suffix('g', format)).isFile())
        return false;
    QFile index(stem + suffix('i', format));
    if (!index.open(QIODevice::ReadOnly))
        return false;
    // Le format 5 n'a pas d'en-tête : rien que des fiches de 56 octets.
    if (format == 5)
        return index.size() % 56 == 0;
    const auto head = index.read(10);
    return head.size() == 10 && head.startsWith("Scid.si") &&
           ((unsigned char)head[8] << 8 | (unsigned char)head[9]) >= 400;
}

int games(const QString &path) {
    if (!isScidBase(path))
        return 0;
    QFile index(indexPath(path));
    if (!index.open(QIODevice::ReadOnly))
        return 0;
    if (base(path).second == 5)
        return int(std::min<qint64>(index.size() / 56, INT_MAX));
    const auto head = index.read(17);
    if (head.size() < 17)
        return 0;
    return (unsigned char)head[14] << 16 | (unsigned char)head[15] << 8 | (unsigned char)head[16];
}

Outcome exportPgn(const QString &path, QIODevice &out, const std::function<void(int, int)> &progress,
                  const std::shared_ptr<std::atomic_bool> &canceled) {
    if (!isScidBase(path))
        throw std::runtime_error(
            "Ce n’est pas une base SCID (.si4 ou .si5, avec ses fichiers de noms et de parties).");
    const auto [stem, format] = base(path);
    const bool five = format == 5;
    const auto nameBytes = readAll(stem + suffix('n', format));
    const auto names = five ? readNames5(nameBytes) : readNames(nameBytes);
    const auto index = readAll(stem + suffix('i', format));
    QFile moves(stem + suffix('g', format));
    if (!moves.open(QIODevice::ReadOnly))
        throw std::runtime_error("Fichier des parties SCID illisible.");
    const int total = games(path);
    if (!five && index.size() < headerSize + qsizetype(total) * entrySize)
        throw std::runtime_error("Index SCID tronqué.");
    Outcome outcome;
    const auto *bytes = reinterpret_cast<const unsigned char *>(index.constData());
    for (int i = 0; i < total; ++i) {
        if (canceled && canceled->load())
            break;
        quint64 offset = 0;
        const auto entry = five ? readEntry5(bytes + qsizetype(i) * 56, offset) : [&] {
            const auto *at = bytes + headerSize + qsizetype(i) * entrySize;
            auto e = readEntry(Reader{at, at + entrySize});
            offset = e.offset;
            return e;
        }();
        if (entry.flags & (1u << 3)) // marquée pour suppression dans SCID
            continue;
        try {
            if (!moves.seek(qint64(offset)))
                throw std::runtime_error("Position introuvable dans le fichier des parties.");
            const auto record = moves.read(entry.length);
            if (record.size() != qsizetype(entry.length))
                throw std::runtime_error("Partie tronquée.");
            const auto text = pgnOf(record, entry, names) + "\n\n";
            if (out.write(text.data(), qint64(text.size())) != qint64(text.size()))
                throw std::runtime_error("Écriture impossible.");
            ++outcome.written;
        } catch (const std::runtime_error &) {
            ++outcome.skipped;
        }
        if (progress && (i % 256 == 0 || i + 1 == total))
            progress(outcome.written, total);
    }
    return outcome;
}
} // namespace t64::scid
