// SPDX-License-Identifier: AGPL-3.0-or-later
// Les tables de transcodage des coups viennent de la description publiée du format,
// telle que la reprennent cbvault (itshak/cbvault, MIT) et cbh2pgn (asdfjkl/cbh2pgn,
// MIT) ; la disposition des fichiers suit la spécification de cbvault
// (docs/format-spec.md). Le code est écrit pour Trait64.
#include "chessbase.h"
#include "game.h"
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QStringDecoder>
#include <algorithm>
#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace t64::chessbase {
namespace {
using Bytes = const unsigned char *;
constexpr qsizetype recordSize = 46;

// Chaque octet de coup passe par la table de son mode d'encodage. Le mode 0, le plus
// courant, retire le nombre de coups lus avant la table, les autres après.
constexpr std::array<unsigned char, 256> mode0{
    162, 149, 67,  245, 193, 61,  74,  108, 83,  131, 204, 124, 255, 174, 104, 173, 209, 146, 139, 141,
    53,  129, 94,  116, 38,  142, 171, 202, 253, 154, 243, 160, 165, 21,  252, 177, 30,  237, 48,  234,
    34,  235, 167, 205, 78,  111, 46,  36,  50,  148, 65,  140, 110, 88,  130, 80,  187, 2,   138, 216,
    250, 96,  222, 82,  186, 70,  172, 41,  157, 215, 223, 8,   33,  1,   102, 163, 241, 25,  39,  181,
    145, 213, 66,  14,  180, 76,  217, 24,  95,  188, 37,  166, 150, 4,   86,  106, 170, 51,  28,  43,
    115, 240, 221, 164, 55,  211, 197, 16,  191, 90,  35,  52,  117, 91,  184, 85,  210, 107, 9,   58,
    87,  18,  179, 119, 72,  133, 155, 15,  158, 199, 200, 161, 127, 122, 192, 189, 49,  109, 246, 62,
    195, 17,  113, 206, 125, 218, 168, 84,  144, 151, 31,  68,  64,  22,  201, 227, 44,  203, 132, 236,
    159, 63,  92,  230, 118, 11,  60,  32,  183, 54,  0,   220, 231, 249, 79,  247, 175, 6,   7,   224,
    26,  10,  169, 75,  12,  214, 99,  135, 137, 29,  19,  27,  228, 112, 5,   71,  103, 123, 47,  238,
    226, 232, 152, 13,  239, 207, 196, 244, 251, 176, 23,  153, 100, 242, 212, 42,  3,   77,  120, 198,
    254, 101, 134, 136, 121, 69,  59,  229, 73,  143, 45,  185, 190, 98,  147, 20,  233, 208, 56,  156,
    178, 194, 89,  93,  182, 114, 81,  248, 40,  126, 97,  57,  225, 219, 105, 128};
constexpr std::array<unsigned char, 256> mode4{
    88,  163, 238, 114, 71,  185, 31,  160, 38,  253, 220, 109, 241, 227, 203, 197, 226, 208, 41,  231,
    137, 225, 174, 123, 52,  65,  53,  75,  165, 124, 246, 229, 219, 140, 95,  55,  5,   187, 50,  84,
    76,  239, 93,  42,  127, 211, 129, 134, 223, 24,  195, 68,  22,  247, 133, 15,  77,  48,  121, 20,
    69,  170, 162, 108, 21,  146, 166, 98,  72,  25,  233, 60,  56,  148, 40,  168, 102, 9,   3,   99,
    32,  125, 54,  215, 46,  152, 236, 218, 1,   19,  36,  115, 205, 158, 207, 217, 213, 104, 139, 210,
    171, 91,  105, 176, 39,  252, 49,  27,  83,  126, 101, 118, 85,  117, 175, 10,  204, 199, 196, 178,
    181, 34,  232, 167, 190, 245, 0,   90,  33,  209, 116, 142, 222, 156, 228, 66,  43,  177, 155, 147,
    193, 120, 112, 214, 182, 173, 78,  106, 100, 122, 87,  73,  17,  157, 18,  192, 58,  47,  230, 132,
    198, 7,   206, 8,   130, 4,   144, 184, 135, 202, 110, 30,  64,  26,  80,  180, 16,  188, 67,  138,
    113, 11,  141, 92,  59,  63,  136, 186, 244, 243, 97,  29,  249, 254, 37,  81,  13,  235, 200, 89,
    23,  70,  164, 242, 86,  183, 2,   45,  51,  159, 216, 131, 251, 191, 234, 237, 172, 96,  103, 57,
    221, 150, 107, 169, 194, 119, 12,  14,  145, 250, 153, 154, 224, 161, 240, 94,  201, 6,   79,  189,
    82,  128, 212, 248, 151, 35,  62,  28,  44,  149, 74,  143, 61,  111, 255, 179};
constexpr std::array<unsigned char, 256> mode5{
    224, 27,  151, 134, 162, 72,  240, 35,  1,   55,  250, 235, 80,  118, 93,  205, 128, 26,  49,  92,
    255, 188, 120, 218, 229, 163, 117, 179, 113, 225, 161, 14,  86,  65,  63,  227, 13,  196, 37,  230,
    136, 137, 112, 220, 153, 166, 11,  154, 20,  44,  87,  158, 105, 122, 223, 85,  173, 115, 124, 197,
    3,   114, 245, 69,  98,  142, 175, 95,  204, 192, 94,  243, 24,  19,  181, 56,  9,   40,  60,  189,
    16,  139, 233, 221, 140, 148, 8,   129, 167, 254, 36,  121, 68,  43,  193, 54,  50,  127, 103, 228,
    253, 73,  75,  33,  169, 91,  119, 31,  28,  246, 64,  101, 61,  249, 90,  248, 71,  38,  83,  212,
    32,  210, 216, 79,  39,  174, 251, 182, 155, 116, 184, 66,  252, 22,  111, 215, 159, 185, 123, 96,
    222, 194, 70,  6,   156, 211, 42,  199, 5,   133, 62,  177, 0,   21,  209, 150, 206, 144, 149, 17,
    51,  143, 198, 130, 132, 183, 57,  160, 232, 52,  30,  125, 168, 208, 234, 226, 239, 76,  195, 88,
    78,  2,   29,  178, 126, 152, 45,  7,   213, 202, 104, 203, 48,  180, 172, 131, 99,  236, 135, 238,
    46,  190, 207, 109, 77,  138, 214, 4,   176, 53,  200, 201, 18,  23,  97,  141, 100, 15,  165, 84,
    41,  58,  74,  107, 187, 47,  217, 59,  147, 106, 170, 164, 34,  244, 25,  67,  110, 241, 247, 219,
    146, 237, 102, 89,  10,  191, 108, 81,  145, 82,  242, 12,  157, 171, 186, 231};

unsigned be(Bytes at, int bytes) {
    unsigned value = 0;
    for (int i = 0; i < bytes; ++i)
        value = value << 8 | at[i];
    return value;
}
int le32(Bytes at) {
    return int(quint32(at[0]) | quint32(at[1]) << 8 | quint32(at[2]) << 16 | quint32(at[3]) << 24);
}

QString base(const QString &path) {
    static const QStringList suffixes{"cbh", "cbg", "cba", "cbp",  "cbt",  "cbc",
                                      "cbs", "cbe", "cbj", "cbtt", "flags"};
    const QFileInfo file(path);
    return suffixes.contains(file.suffix().toLower()) ? file.absolutePath() + "/" + file.completeBaseName()
                                                      : QString();
}
// Un fichier de la base, en minuscules ou en majuscules : ChessBase écrit « .CBH » sous
// Windows, et une base copiée ailleurs garde ses noms.
QString sibling(const QString &stem, const QString &suffix) {
    for (const auto &candidate : {suffix, suffix.toUpper()})
        if (QFileInfo(stem + "." + candidate).isFile())
            return stem + "." + candidate;
    return {};
}

// Un fichier projeté en mémoire : le .cbg de la Mega Database pèse plusieurs gigaoctets.
struct Mapped {
    QFile file;
    Bytes data = nullptr;
    qint64 size = 0;
    explicit Mapped(const QString &path) : file(path) {
        if (!file.open(QIODevice::ReadOnly))
            throw std::runtime_error(("Lecture impossible : " + path).toStdString());
        size = file.size();
        if (size > 0 && !(data = file.map(0, size)))
            throw std::runtime_error(("Lecture impossible : " + path).toStdString());
    }
    // `bytes` octets à partir de `offset`, ou rien s'ils sortent du fichier.
    Bytes at(qint64 offset, qint64 bytes) const {
        return offset >= 0 && bytes >= 0 && offset <= size - bytes ? data + offset : nullptr;
    }
};

// Un texte de ChessBase : de l'UTF-8 quand il en est, sinon la page de code Windows 1252,
// et il s'arrête au premier zéro.
QString text(Bytes at, qsizetype bytes) {
    bytes = std::find(at, at + bytes, 0) - at;
    const QByteArrayView view(at, bytes);
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString decoded = utf8.decode(view);
    if (!utf8.hasError())
        return decoded;
    static constexpr char16_t high[32] = {0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
                                          0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD,
                                          0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
                                          0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178};
    decoded.clear();
    decoded.reserve(bytes);
    for (qsizetype i = 0; i < bytes; ++i)
        decoded += at[i] >= 0x80 && at[i] < 0xA0 ? QChar(high[at[i] - 0x80]) : QChar(at[i]);
    return decoded;
}

// Les fichiers de noms : un en-tête de 28 octets (32 dans certaines versions), puis des
// fiches de taille fixe ; un nom se désigne par le rang de sa fiche.
struct Names {
    QByteArray bytes;
    qsizetype header = 0, record = 0, count = 0;
    void open(const QString &path, int minimum) {
        QFile file(path);
        if (path.isEmpty() || !file.open(QIODevice::ReadOnly))
            return;
        bytes = file.readAll();
        const auto *head = reinterpret_cast<Bytes>(bytes.constData());
        const int data = bytes.size() >= 28 ? le32(head + 12) : 0,
                  extra = bytes.size() >= 28 ? le32(head + 24) : 0;
        if (bytes.size() < 28 || le32(head + 8) != 1234567890 || data < minimum || data > 65536 ||
            (extra != 0 && extra != 4))
            throw std::runtime_error("Fichier de noms ChessBase illisible.");
        header = 28 + extra;
        record = 9 + data;
        count = (bytes.size() - header) / record;
    }
    std::string field(unsigned id, int offset, int length) const {
        if (qsizetype(id) >= count)
            return {};
        const auto *at = reinterpret_cast<Bytes>(bytes.constData()) + header + qsizetype(id) * record;
        if (le32(at) == -999) // fiche effacée
            return {};
        return text(at + 9 + offset, length).trimmed().toStdString();
    }
};

// Les cases de ChessBase se comptent colonne par colonne (a1 = 0, a2 = 1, b1 = 8) ; celles
// de chess.hpp rangée par rangée. L'une est la transposée de l'autre.
int transpose(int square) {
    return (square % 8) * 8 + square / 8;
}

// Les listes de pièces de ChessBase, par camp : dames, tours, fous et cavaliers dans
// l'ordre où on les rencontre colonne par colonne, une pièce prise faisant remonter les
// suivantes ; les pions ont chacun leur numéro fixe, qui reste vide une fois pris.
struct Lists {
    std::array<std::array<std::vector<int>, 4>, 2> kinds;
    std::array<std::array<int, 8>, 2> pawns{};
    static int kindOf(chess::PieceType type) {
        if (type == chess::PieceType::QUEEN)
            return 0;
        if (type == chess::PieceType::ROOK)
            return 1;
        if (type == chess::PieceType::BISHOP)
            return 2;
        if (type == chess::PieceType::KNIGHT)
            return 3;
        return -1;
    }
    explicit Lists(const chess::Board &board) {
        for (auto &side : pawns)
            side.fill(-1);
        std::array<int, 2> pawnCount{};
        for (int cb = 0; cb < 64; ++cb) {
            const auto piece = board.at(chess::Square(transpose(cb)));
            if (piece == chess::Piece::NONE)
                continue;
            const int side = piece.color() == chess::Color::WHITE ? 0 : 1;
            if (piece.type() == chess::PieceType::PAWN) {
                if (pawnCount[side] == 8)
                    throw std::runtime_error("Plus de huit pions.");
                pawns[side][pawnCount[side]++] = cb;
            } else if (const int kind = kindOf(piece.type()); kind >= 0) {
                kinds[side][kind].push_back(cb);
            }
        }
    }
    // Met les listes à jour pour un coup, avant qu'il soit joué sur `board`.
    void follow(const chess::Board &board, const chess::Move &move) {
        const int side = board.sideToMove() == chess::Color::WHITE ? 0 : 1, other = 1 - side;
        const int from = transpose(move.from().index()), to = transpose(move.to().index());
        if (move.typeOf() == chess::Move::CASTLING) {
            // chess.hpp note le roque du roi vers sa tour : la tour finit en f ou en d.
            auto &rooks = kinds[side][1];
            const auto rook = std::find(rooks.begin(), rooks.end(), to);
            if (rook != rooks.end())
                *rook = (to > from ? 5 : 3) * 8 + to % 8;
            return;
        }
        int taken = to;
        if (move.typeOf() == chess::Move::ENPASSANT)
            taken = to / 8 * 8 + from % 8;
        const auto victim = board.at(chess::Square(transpose(taken)));
        if (victim != chess::Piece::NONE) {
            if (const int kind = kindOf(victim.type()); kind >= 0) {
                auto &list = kinds[other][kind];
                list.erase(std::remove(list.begin(), list.end(), taken), list.end());
            } else {
                std::replace(pawns[other].begin(), pawns[other].end(), taken, -1);
            }
        }
        const auto mover = board.at(move.from()).type();
        if (mover == chess::PieceType::PAWN) {
            auto slot = std::find(pawns[side].begin(), pawns[side].end(), from);
            if (slot == pawns[side].end())
                throw std::runtime_error("Pion introuvable.");
            if (move.typeOf() == chess::Move::PROMOTION) {
                *slot = -1;
                kinds[side][kindOf(move.promotionType())].push_back(to);
            } else {
                *slot = to;
            }
        } else if (const int kind = kindOf(mover); kind >= 0) {
            std::replace(kinds[side][kind].begin(), kinds[side][kind].end(), from, to);
        }
    }
};

// Le coup cherché parmi les coups légaux : départ et arrivée en cases de chess.hpp,
// pièce de promotion, ou roque (1 : petit, -1 : grand).
chess::Move legal(const chess::Board &board, int from, int to, chess::PieceType promotion, int castle) {
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    for (const auto &move : moves) {
        if (castle || move.typeOf() == chess::Move::CASTLING) {
            if (castle && move.typeOf() == chess::Move::CASTLING && (move.to() > move.from()) == (castle > 0))
                return move;
            continue;
        }
        if (int(move.from().index()) != from || int(move.to().index()) != to)
            continue;
        if (move.typeOf() == chess::Move::PROMOTION ? move.promotionType() == promotion
                                                    : promotion == chess::PieceType::NONE)
            return move;
    }
    throw std::runtime_error("Coup illégal dans la base ChessBase.");
}

// Un coup écrit par ses deux cases (deux octets, ou le mode 5) : départ, arrivée et, pour
// une promotion, la pièce sur deux bits.
chess::Move bySquares(const chess::Board &board, unsigned word) {
    const int from = transpose(int(word & 63)), to = transpose(int(word >> 6 & 63));
    if (from == to)
        throw std::runtime_error("Coup nul non pris en charge.");
    const auto piece = board.at(chess::Square(from));
    const int back = board.sideToMove() == chess::Color::WHITE ? 0 : 7;
    if (piece.type() == chess::PieceType::KING && from % 8 == 4 && from / 8 == back && to / 8 == back &&
        (to % 8 == 6 || to % 8 == 2))
        return legal(board, from, to, chess::PieceType::NONE, to % 8 == 6 ? 1 : -1);
    chess::PieceType promotion = chess::PieceType::NONE;
    if (piece.type() == chess::PieceType::PAWN && (to / 8 == 0 || to / 8 == 7)) {
        static constexpr chess::PieceType::underlying kinds[] = {
            chess::PieceType::QUEEN, chess::PieceType::ROOK, chess::PieceType::BISHOP,
            chess::PieceType::KNIGHT};
        promotion = chess::PieceType(kinds[word >> 12 & 3]);
    }
    return legal(board, from, to, promotion, 0);
}

// Un coup sur un octet, une fois traduit : la pièce, par son rang dans sa liste, et son
// déplacement, chaque coordonnée prise modulo 8.
chess::Move byCode(const chess::Board &board, const Lists &lists, int code) {
    static constexpr int king[8][2] = {{0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}};
    static constexpr int knight[8][2] = {{2, 1},   {1, 2},   {-1, 2}, {-2, 1},
                                         {-2, -1}, {-1, -2}, {1, -2}, {2, -1}};
    static constexpr int queen[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
    static constexpr int rook[2][2] = {{0, 1}, {1, 0}};
    static constexpr int bishop[2][2] = {{1, 1}, {1, -1}};
    const bool white = board.sideToMove() == chess::Color::WHITE;
    const int side = white ? 0 : 1;
    const auto step = [&](int cb, int files, int ranks) {
        return legal(board, transpose(cb), transpose(((cb / 8 + files) & 7) * 8 + ((cb % 8 + ranks) & 7)),
                     chess::PieceType::NONE, 0);
    };
    int kind = -1, index = 0, files = 0, ranks = 0;
    const auto line = [&](int k, const int(*dirs)[2]) {
        files = dirs[k / 7][0] * (k % 7 + 1);
        ranks = dirs[k / 7][1] * (k % 7 + 1);
    };
    if (code == 0)
        throw std::runtime_error("Coup nul non pris en charge.");
    if (code <= 8)
        return step(transpose(board.kingSq(board.sideToMove()).index()), king[code - 1][0],
                    king[code - 1][1]);
    if (code == 9 || code == 10)
        return legal(board, 0, 0, chess::PieceType::NONE, code == 9 ? 1 : -1);
    if (code >= 111 && code <= 142) {
        const int pawn = lists.pawns[side][(code - 111) / 4], how = (code - 111) % 4,
                  forward = white ? 1 : -1;
        if (pawn < 0)
            throw std::runtime_error("Pion inconnu.");
        static constexpr int moves[4][2] = {{0, 1}, {0, 2}, {1, 1}, {-1, 1}};
        return step(pawn, moves[how][0] * forward, moves[how][1] * forward);
    }
    if (code <= 38)
        kind = 0, line(code - 11, queen);
    else if (code <= 52)
        kind = 1, line(code - 39, rook);
    else if (code <= 66)
        kind = 1, index = 1, line(code - 53, rook);
    else if (code <= 80)
        kind = 2, line(code - 67, bishop);
    else if (code <= 94)
        kind = 2, index = 1, line(code - 81, bishop);
    else if (code <= 110)
        kind = 3, index = (code - 95) / 8, files = knight[(code - 95) % 8][0],
        ranks = knight[(code - 95) % 8][1];
    else if (code >= 143 && code <= 170)
        kind = 0, index = 1, line(code - 143, queen);
    else if (code >= 171 && code <= 198)
        kind = 0, index = 2, line(code - 171, queen);
    else if (code >= 199 && code <= 212)
        kind = 1, index = 2, line(code - 199, rook);
    else if (code >= 213 && code <= 226)
        kind = 2, index = 2, line(code - 213, bishop);
    else if (code >= 227 && code <= 234)
        kind = 3, index = 2, files = knight[code - 227][0], ranks = knight[code - 227][1];
    else
        throw std::runtime_error("Code de coup inconnu.");
    const auto &list = lists.kinds[side][kind];
    if (index >= int(list.size()))
        throw std::runtime_error("Pièce inconnue.");
    return step(list[index], files, ranks);
}

// La position de départ écrite par la partie : 4 octets (trait et prise en passant,
// roques, numéro du coup), puis les 64 cases colonne par colonne, un bit à zéro pour une
// case vide, sinon un bit à un, la couleur et trois bits pour la pièce.
std::string startFen(Bytes start) {
    std::array<char, 64> board{};
    int bit = 32; // après les quatre premiers octets
    const auto next = [&] {
        if (bit >= 28 * 8)
            throw std::runtime_error("Position de départ tronquée.");
        const int value = start[bit / 8] >> (7 - bit % 8) & 1;
        ++bit;
        return value;
    };
    for (int cb = 0; cb < 64; ++cb) {
        if (!next())
            continue;
        const bool black = next();
        const int code = next() << 2 | next() << 1 | next();
        static constexpr char pieces[] = " KQNBRP";
        if (code < 1 || code > 6)
            throw std::runtime_error("Pièce inconnue dans la position de départ.");
        board[transpose(cb)] = black ? char(pieces[code] + 32) : pieces[code];
    }
    std::string fen;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const char piece = board[rank * 8 + file];
            if (!piece) {
                ++empty;
                continue;
            }
            if (empty)
                fen += char('0' + empty), empty = 0;
            fen += piece;
        }
        if (empty)
            fen += char('0' + empty);
        if (rank)
            fen += '/';
    }
    const bool blackToMove = start[1] & 0x10;
    fen += blackToMove ? " b " : " w ";
    std::string castling;
    for (const auto &[mask, letter] : {std::pair{2, 'K'}, {1, 'Q'}, {8, 'k'}, {4, 'q'}})
        if (start[2] & mask)
            castling += letter;
    fen += castling.empty() ? "-" : castling;
    const int ep = start[1] & 0x0f;
    fen += ep >= 1 && ep <= 8 ? std::string(" ") + char('a' + ep - 1) + (blackToMove ? '3' : '6') : " -";
    return fen + " 0 " + std::to_string(std::max(1, int(start[3])));
}

// Les cases et les flèches de ChessBase prennent les couleurs du PGN : vert, jaune, rouge,
// bleu, cyan et orange ; les autres ne se disent pas.
char colourOf(unsigned char code) {
    switch (code) {
    case 2:
        return 'G';
    case 3:
        return 'Y';
    case 4:
        return 'R';
    case 7:
        return 'B';
    case 8:
        return 'C';
    case 9:
        return 'O';
    default:
        return 0;
    }
}
// Une case numérotée de 1 à 64, colonne par colonne.
std::string squareName(unsigned char n) {
    if (n < 1 || n > 64)
        return {};
    return {char('a' + (n - 1) / 8), char('1' + (n - 1) % 8)};
}

// Ce que les annotations ajoutent à un coup, rassemblé avant d'être écrit dans l'ordre de
// ChessBase : cases, flèches, puis les textes.
struct Notes {
    std::string squares, arrows, texts, before;
    std::vector<int> nags;
};

void readAnnotations(Bytes at, Bytes end, std::vector<Notes> &notes, size_t overflow) {
    // L'en-tête de 14 octets : numéro de la partie, marque fixe, nombre et taille.
    while (end - at >= 6) {
        const int raw = int(be(at, 3)), position = raw & 0x800000 ? raw - 0x1000000 : raw;
        const unsigned char type = at[3];
        const unsigned size = be(at + 4, 2);
        if (size < 6 || size > unsigned(end - at))
            break;
        Bytes data = at + 6;
        const qsizetype length = size - 6;
        at += size;
        // -1 : la partie entière ; au-delà du dernier coup lu, après le dernier coup de la
        // ligne principale.
        const size_t slot = position < 0                          ? 0
                            : size_t(position) + 1 < notes.size() ? size_t(position) + 1
                                                                  : overflow;
        auto &note = notes[slot];
        if ((type == 0x02 || type == 0x82) && length >= 2) {
            auto written = text(data + 2, length - 2).simplified();
            written.replace('{', '(').replace('}', ')');
            if (written.isEmpty())
                continue;
            auto &into = type == 0x82 ? note.before : note.texts;
            into += (into.empty() ? "" : " ") + written.toStdString();
        } else if (type == 0x03) {
            for (qsizetype i = 0; i < std::min<qsizetype>(length, 3); ++i)
                if (data[i])
                    note.nags.push_back(data[i]);
        } else if (type == 0x04) {
            for (qsizetype i = 0; i + 1 < length; i += 2)
                if (const char colour = colourOf(data[i]); colour && !squareName(data[i + 1]).empty())
                    note.squares +=
                        (note.squares.empty() ? "" : ",") + std::string(1, colour) + squareName(data[i + 1]);
        } else if (type == 0x05) {
            for (qsizetype i = 0; i + 2 < length; i += 3)
                if (const char colour = colourOf(data[i]);
                    colour && !squareName(data[i + 1]).empty() && !squareName(data[i + 2]).empty())
                    note.arrows += (note.arrows.empty() ? "" : ",") + std::string(1, colour) +
                                   squareName(data[i + 1]) + squareName(data[i + 2]);
        }
    }
}

std::string comment(const Notes &note) {
    std::string graphics;
    if (!note.squares.empty())
        graphics += "[%csl " + note.squares + "]";
    if (!note.arrows.empty())
        graphics += "[%cal " + note.arrows + "]";
    if (graphics.empty() || note.texts.empty())
        return graphics + note.texts;
    return graphics + " " + note.texts;
}

// Les coups d'une partie, rejoués dans l'ordre de ChessBase : la ligne principale
// d'abord, chaque variante ensuite, depuis la position d'avant le coup qu'elle remplace.
// `played` reçoit les nœuds dans l'ordre où les coups ont été lus, celui des annotations.
void readMoves(Game &game, Bytes stream, qsizetype length, int mode, std::vector<int> &played) {
    const auto &table = mode == 0 ? mode0 : mode == 4 ? mode4 : mode5;
    unsigned plies = 0;
    const auto translate = [&](unsigned char byte) -> unsigned char {
        const auto n = static_cast<unsigned char>(plies);
        return mode == 0 ? table[static_cast<unsigned char>(byte - n)]
                         : static_cast<unsigned char>(table[byte] - n);
    };
    struct Saved {
        int node;
        Lists lists;
    };
    std::vector<Saved> stack;
    int node = 0;
    Lists lists(game.boardOf(0));
    bool branchNext = false, ended = false;
    const auto play = [&](const chess::Move &move) {
        std::optional<Saved> saved;
        if (branchNext)
            saved = Saved{node, lists};
        branchNext = false;
        const auto &board = game.boardOf(node);
        lists.follow(board, move);
        node = game.play(node, chess::uci::moveToUci(move, false));
        played.push_back(node);
        ++plies;
        if (saved)
            stack.push_back(std::move(*saved));
    };
    const auto branch = [&] {
        if (branchNext || stack.size() >= 1024)
            throw std::runtime_error("Variantes illisibles.");
        branchNext = true;
    };
    const auto endLine = [&] {
        if (branchNext)
            throw std::runtime_error("Variante vide.");
        if (stack.empty()) {
            ended = true;
            return;
        }
        node = stack.back().node;
        lists = std::move(stack.back().lists);
        stack.pop_back();
    };
    if (mode == 5) {
        // Le mode simple : deux octets par coup, le bit 15 ouvre une variante sur ce coup,
        // le bit 14 ferme la ligne après lui.
        for (qsizetype i = 0; i + 1 < length && !ended; i += 2) {
            const unsigned word = unsigned(translate(stream[i])) << 8 | translate(stream[i + 1]);
            if (word & 0x8000)
                branch();
            play(bySquares(game.boardOf(node), word & 0x3fff));
            if (word & 0x4000)
                endLine();
        }
        return;
    }
    for (qsizetype i = 0; i < length && !ended;) {
        const int code = translate(stream[i++]);
        if (code == 236) // octet de remplissage
            continue;
        if (code == 254)
            branch();
        else if (code == 255)
            endLine();
        else if (code == 235) {
            if (length - i < 2)
                throw std::runtime_error("Coup tronqué.");
            const unsigned word = unsigned(translate(stream[i])) << 8 | translate(stream[i + 1]);
            i += 2;
            play(bySquares(game.boardOf(node), word));
        } else if (code > 235)
            throw std::runtime_error("Code de coup inconnu.");
        else
            play(byCode(game.boardOf(node), lists, code));
    }
    // Une partie sans le zéro final de sa ligne principale se termine quand même là.
}

std::string dateText(unsigned packed) {
    const unsigned year = packed >> 9, month = packed >> 5 & 15, day = packed & 31;
    const auto two = [](unsigned n) { return n ? std::string(n < 10 ? "0" : "") + std::to_string(n) : "??"; };
    return (year ? std::to_string(year) : std::string("????")) + "." + two(month) + "." + two(day);
}

struct Base {
    Mapped index, moves;
    std::optional<Mapped> notes;
    Names players, tournaments, annotators;
    explicit Base(const QString &stem) : index(sibling(stem, "cbh")), moves(sibling(stem, "cbg")) {
        if (const auto cba = sibling(stem, "cba"); !cba.isEmpty())
            notes.emplace(cba);
        players.open(sibling(stem, "cbp"), 50);
        tournaments.open(sibling(stem, "cbt"), 70);
        annotators.open(sibling(stem, "cbc"), 45);
    }
};

// La partie de la fiche `number`, en PGN ; vide pour un texte ou une partie effacée.
std::string pgnOf(const Base &db, qsizetype number) {
    const Bytes record = db.index.at(number * recordSize, recordSize);
    if (!record || record[0] & 0x80 || !(record[0] & 1) || record[0] & 2)
        return {};
    const Bytes head = db.moves.at(be(record + 1, 4), 4);
    if (!head)
        throw std::runtime_error("Partie introuvable dans le fichier des coups.");
    const unsigned size = be(head + 1, 3);
    const Bytes body = db.moves.at(be(record + 1, 4), size);
    if (!body || size < 4 || head[0] & 0x80)
        throw std::runtime_error("Partie tronquée.");
    const int mode = head[0] & 0x3f;
    if (mode != 0 && mode != 4 && mode != 5)
        throw std::runtime_error("Encodage ChessBase non pris en charge (Chess960 ou inconnu).");
    qsizetype at = 4;
    std::string fen(chess::constants::STARTPOS);
    if (head[0] & 0x40) {
        if (size < 4 + 28)
            throw std::runtime_error("Position de départ tronquée.");
        fen = startFen(body + 4);
        at += 28;
    }
    Game game(fen);
    std::vector<int> played;
    readMoves(game, body + at, qsizetype(size) - at, mode, played);

    // Les annotations, rangées par coup lu : la case 0 est la partie elle-même.
    std::vector<Notes> notes(played.size() + 1);
    // La ligne principale est lue la première : ses coups sont les premiers de `played`.
    const size_t mainline = game.mainline().size() - 1;
    if (const unsigned offset = be(record + 5, 4); offset && db.notes) {
        const Bytes cba = db.notes->at(offset, 14);
        if (cba && be(cba, 3) == unsigned(number)) {
            const unsigned total = be(cba + 10, 4);
            if (const Bytes all = db.notes->at(offset, total); all && total >= 14)
                readAnnotations(all + 14, all + total, notes, mainline);
        }
    }
    for (size_t i = 0; i < notes.size(); ++i) {
        auto &node = game.nodes[i == 0 ? 0 : played[i - 1]];
        node.comment = comment(notes[i]);
        node.nags = notes[i].nags;
        if (!notes[i].before.empty() && i > 0) {
            auto &parent = game.nodes[node.parent].comment;
            parent += (parent.empty() ? "" : " ") + notes[i].before;
        }
    }

    const auto orUnknown = [](const std::string &value) { return value.empty() ? std::string("?") : value; };
    const auto player = [&](unsigned id) {
        const auto last = db.players.field(id, 0, 30), first = db.players.field(id, 30, 20);
        return orUnknown(first.empty() ? last : last + ", " + first);
    };
    const unsigned tournament = be(record + 0x0f, 3);
    game.tags["Event"] = orUnknown(db.tournaments.field(tournament, 0, 40));
    game.tags["Site"] = orUnknown(db.tournaments.field(tournament, 40, 30));
    game.tags["Date"] = dateText(be(record + 0x18, 3));
    const int round = record[0x1d], subround = record[0x1e];
    game.tags["Round"] =
        round ? std::to_string(round) + (subround ? "." + std::to_string(subround) : "") : "?";
    game.tags["White"] = player(be(record + 0x09, 3));
    game.tags["Black"] = player(be(record + 0x0c, 3));
    static const char *const results[] = {"0-1", "1/2-1/2", "1-0", "*", "0-1", "1/2-1/2", "1-0", "*"};
    game.tags["Result"] = record[0x1b] < 8 ? results[record[0x1b]] : "*";
    if (const unsigned elo = be(record + 0x1f, 2))
        game.tags["WhiteElo"] = std::to_string(elo);
    if (const unsigned elo = be(record + 0x21, 2))
        game.tags["BlackElo"] = std::to_string(elo);
    if (const unsigned eco = be(record + 0x23, 2); eco >= 128 && eco < 64128) {
        const unsigned code = eco / 128 - 1;
        game.tags["ECO"] =
            std::string(1, char('A' + code / 100)) + char('0' + code % 100 / 10) + char('0' + code % 10);
    }
    if (const auto annotator = db.annotators.field(be(record + 0x12, 3), 0, 45); !annotator.empty())
        game.tags["Annotator"] = annotator;
    return game.pgn();
}
} // namespace

QString indexPath(const QString &path) {
    const auto stem = base(path);
    if (stem.isEmpty())
        return {};
    const auto found = sibling(stem, "cbh");
    return found.isEmpty() ? stem + ".cbh" : found;
}

bool isChessBase(const QString &path) {
    const auto stem = base(path);
    if (stem.isEmpty())
        return false;
    for (const auto *suffix : {"cbh", "cbg", "cbp", "cbt"})
        if (sibling(stem, suffix).isEmpty())
            return false;
    QFile index(sibling(stem, "cbh"));
    if (!index.open(QIODevice::ReadOnly))
        return false;
    // L'en-tête annonce des fiches de 46 octets.
    const auto head = index.read(recordSize);
    return head.size() == recordSize && (unsigned char)head[4] == recordSize;
}

int records(const QString &path) {
    if (!isChessBase(path))
        return 0;
    return int(std::min<qint64>(QFileInfo(indexPath(path)).size() / recordSize - 1, 1 << 30));
}

Outcome exportPgn(const QString &path, QIODevice &out, const std::function<void(int, int)> &progress,
                  const std::shared_ptr<std::atomic_bool> &canceled) {
    if (!isChessBase(path))
        throw std::runtime_error("Ce n’est pas une base ChessBase complète (.cbh, .cbg, .cbp, .cbt).");
    const Base db(base(path));
    const int total = records(path);
    Outcome outcome;
    for (int i = 1; i <= total; ++i) {
        if (canceled && canceled->load())
            break;
        try {
            const auto pgn = pgnOf(db, i);
            if (!pgn.empty()) {
                const auto text = pgn + "\n\n";
                if (out.write(text.data(), qint64(text.size())) != qint64(text.size()))
                    throw std::runtime_error("Écriture impossible.");
                ++outcome.written;
            }
        } catch (const std::runtime_error &) {
            ++outcome.skipped;
        }
        if (progress && (i % 256 == 0 || i == total))
            progress(outcome.written, total);
    }
    return outcome;
}
} // namespace t64::chessbase
