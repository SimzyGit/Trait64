// SPDX-License-Identifier: AGPL-3.0-or-later
// Reconnaissance par silhouettes : chaque case est séparée de sa couleur de fond, sa
// silhouette comparée à celles des jeux de pièces fournis, sa teinte donne le camp.
#include "diagram_reader.h"
#include <QDir>
#include <QImageReader>
#include <QObject>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <vector>

namespace t64 {
namespace {
constexpr int kGrid = 24;
using Mask = std::array<float, kGrid * kGrid>;

int distance(QRgb a, QRgb b) {
    return std::abs(qRed(a) - qRed(b)) + std::abs(qGreen(a) - qGreen(b)) + std::abs(qBlue(a) - qBlue(b));
}
// La silhouette, recadrée sur sa boîte et ramenée à kGrid × kGrid : la taille de la
// pièce dans sa case varie d'un site à l'autre, pas sa forme.
Mask normalize(const std::vector<char> &inside, int w, int h, float *aspect) {
    int left = w, right = -1, top = h, bottom = -1;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (inside[size_t(y * w + x)])
                left = std::min(left, x), right = std::max(right, x), top = std::min(top, y),
                bottom = std::max(bottom, y);
    Mask mask{};
    if (right < left)
        return mask;
    *aspect = float(right - left + 1) / float(bottom - top + 1);
    for (int gy = 0; gy < kGrid; ++gy)
        for (int gx = 0; gx < kGrid; ++gx) {
            const int x = left + (gx * 2 + 1) * (right - left + 1) / (2 * kGrid);
            const int y = top + (gy * 2 + 1) * (bottom - top + 1) / (2 * kGrid);
            mask[size_t(gy * kGrid + gx)] = inside[size_t(y * w + x)] ? 1.f : 0.f;
        }
    return mask;
}
float overlap(const Mask &a, const Mask &b) {
    float both = 0, either = 0;
    for (size_t i = 0; i < a.size(); ++i)
        both += std::min(a[i], b[i]), either += std::max(a[i], b[i]);
    return either > 0 ? both / either : 0;
}
struct Template {
    char type; // pnbrqk
    Mask mask;
    float aspect = 1;
};
const std::vector<Template> &templates() {
    static const std::vector<Template> all = [] {
        std::vector<Template> out;
        for (const auto &set : QDir(":/assets/pieces").entryList(QDir::Dirs))
            for (const char *code :
                 {"wP", "wN", "wB", "wR", "wQ", "wK", "bP", "bN", "bB", "bR", "bQ", "bK"}) {
                QImageReader reader(":/assets/pieces/" + set + "/" + code + ".svg");
                reader.setScaledSize(QSize(96, 96));
                const auto image = reader.read().convertToFormat(QImage::Format_ARGB32);
                if (image.isNull())
                    continue;
                std::vector<char> inside(96 * 96);
                for (int y = 0; y < 96; ++y)
                    for (int x = 0; x < 96; ++x)
                        inside[size_t(y * 96 + x)] = qAlpha(image.pixel(x, y)) > 128;
                Template t{char(QChar(code[1]).toLower().toLatin1()), {}, 1};
                t.mask = normalize(inside, 96, 96, &t.aspect);
                out.push_back(t);
            }
        return out;
    }();
    return all;
}
// Tout ce que le fond de la case n'atteint pas depuis le bord : le contour d'une pièce
// blanche sur case claire suffit à enfermer son intérieur.
std::vector<char> silhouette(const QImage &cell, QRgb background) {
    const int w = cell.width(), h = cell.height();
    std::vector<char> reached(size_t(w * h), 0);
    std::vector<int> stack;
    const auto push = [&](int x, int y) {
        const auto i = size_t(y * w + x);
        if (!reached[i] && distance(cell.pixel(x, y), background) < 90)
            reached[i] = 1, stack.push_back(int(i));
    };
    for (int x = 0; x < w; ++x)
        push(x, 0), push(x, h - 1);
    for (int y = 0; y < h; ++y)
        push(0, y), push(w - 1, y);
    while (!stack.empty()) {
        const int i = stack.back();
        stack.pop_back();
        const int x = i % w, y = i / w;
        if (x > 0)
            push(x - 1, y);
        if (x + 1 < w)
            push(x + 1, y);
        if (y > 0)
            push(x, y - 1);
        if (y + 1 < h)
            push(x, y + 1);
    }
    for (auto &r : reached)
        r = !r;
    return reached;
}
} // namespace

QString readDiagram(const QImage &source, QString *why) {
    const auto refuse = [&](const char *text) {
        if (why)
            *why = QObject::tr(text);
        return QString();
    };
    if (source.isNull())
        return refuse("Image illisible.");
    QImage image = source.convertToFormat(QImage::Format_RGB32);
    if (std::max(image.width(), image.height()) > 1200)
        image = image.scaled(1200, 1200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    // Une marge unie autour de l'échiquier est retirée.
    // Une ligne de marge est presque toute de la couleur du coin : les coordonnées
    // écrites dans la marge n'y changent rien.
    const QRgb margin = image.pixel(0, 0);
    const auto uniformRow = [&](int y) {
        int off = 0;
        for (int x = 0; x < image.width(); ++x)
            off += distance(image.pixel(x, y), margin) > 30;
        return off * 4 < image.width();
    };
    const auto uniformColumn = [&](int x, int top, int bottom) {
        int off = 0;
        for (int y = top; y <= bottom; ++y)
            off += distance(image.pixel(x, y), margin) > 30;
        return off * 4 < bottom - top + 1;
    };
    int top = 0, bottom = image.height() - 1, left = 0, right = image.width() - 1;
    while (top < bottom && uniformRow(top))
        ++top;
    while (bottom > top && uniformRow(bottom))
        --bottom;
    while (left < right && uniformColumn(left, top, bottom))
        ++left;
    while (right > left && uniformColumn(right, top, bottom))
        --right;
    const int w = right - left + 1, h = bottom - top + 1;
    if (std::min(w, h) < 64)
        return refuse("L’échiquier est trop petit dans cette image.");
    if (std::abs(w - h) > std::max(w, h) / 8)
        return refuse("Recadrez l’image sur l’échiquier : il doit être carré.");
    const int side = std::min(w, h);
    const QImage board = image.copy(left + (w - side) / 2, top + (h - side) / 2, side, side);
    const double cell = side / 8.0;
    std::array<char, 64> squares{};
    std::array<float, 64> lightness{};
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col) {
            const int inset = int(cell * .06);
            const QRect box(int(col * cell) + inset, int(row * cell) + inset, int(cell) - 2 * inset,
                            int(cell) - 2 * inset);
            const QImage square = board.copy(box);
            // Le fond : la moyenne des quatre coins, que les pièces touchent rarement.
            int r = 0, g = 0, b = 0;
            for (const QPoint corner :
                 {QPoint(1, 1), QPoint(square.width() - 2, 1), QPoint(1, square.height() - 2),
                  QPoint(square.width() - 2, square.height() - 2)}) {
                const auto c = square.pixel(corner);
                r += qRed(c), g += qGreen(c), b += qBlue(c);
            }
            const QRgb background = qRgb(r / 4, g / 4, b / 4);
            const auto inside = silhouette(square, background);
            // Le camp : la majorité des pixels de la pièce, clairs ou foncés. Le contour
            // noir d'une pièce blanche n'en est qu'une petite part.
            int count = 0, light = 0;
            for (int y = 0; y < square.height(); ++y)
                for (int x = 0; x < square.width(); ++x)
                    if (inside[size_t(y * square.width() + x)])
                        ++count, light += qGray(square.pixel(x, y)) > 128;
            const char index = char((7 - row) * 8 + col);
            if (count < square.width() * square.height() * 0.06) {
                squares[size_t(index)] = 0;
                continue;
            }
            float aspect = 1;
            const auto mask = normalize(inside, square.width(), square.height(), &aspect);
            float best = -1;
            char type = 0;
            for (const auto &t : templates()) {
                const float score = overlap(mask, t.mask) - 0.3f * std::abs(std::log(aspect / t.aspect));
                if (score > best)
                    best = score, type = t.type;
            }
            if (best < 0.45f)
                return refuse(
                    "Une case n’a pas pu être lue : essayez une image plus nette ou mieux recadrée.");
            squares[size_t(index)] = type;
            lightness[size_t(index)] = float(light) / float(count);
        }
    // Les deux camps : la part de pixels clairs se partage en deux groupes, au seuil qui
    // les sépare le mieux (méthode d'Otsu). Un contour épais assombrit une pièce blanche,
    // les reflets éclaircissent une noire, sans que les deux groupes se mêlent.
    std::vector<float> shades;
    for (int i = 0; i < 64; ++i)
        if (squares[size_t(i)])
            shades.push_back(lightness[size_t(i)]);
    std::sort(shades.begin(), shades.end());
    float threshold = 0.5f, separation = -1, total = 0;
    for (float v : shades)
        total += v;
    float below = 0;
    for (size_t i = 1; i < shades.size(); ++i) {
        below += shades[i - 1];
        const float n0 = float(i), n1 = float(shades.size() - i);
        const float m0 = below / n0, m1 = (total - below) / n1;
        if (const float score = n0 * n1 * (m1 - m0) * (m1 - m0); score > separation)
            separation = score, threshold = (shades[i] + shades[i - 1]) / 2;
    }
    for (int i = 0; i < 64; ++i)
        if (squares[size_t(i)] && lightness[size_t(i)] > threshold)
            squares[size_t(i)] = char(std::toupper(squares[size_t(i)]));
    // Les Noirs en bas : l'image est retournée.
    int whiteKing = -1, blackKing = -1;
    for (int i = 0; i < 64; ++i)
        whiteKing = squares[size_t(i)] == 'K' ? i : whiteKing,
        blackKing = squares[size_t(i)] == 'k' ? i : blackKing;
    if (whiteKing < 0 || blackKing < 0)
        return refuse("Il faut un roi de chaque camp sur l’image.");
    if (whiteKing / 8 > blackKing / 8 && whiteKing / 8 >= 4)
        std::reverse(squares.begin(), squares.end());
    QString placement;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const char piece = squares[size_t(rank * 8 + file)];
            if (!piece) {
                ++empty;
                continue;
            }
            if ((piece == 'p' || piece == 'P') && (rank == 0 || rank == 7))
                return refuse("Un pion lu sur la première ou la dernière rangée : l’image est mal reconnue.");
            if (empty)
                placement += QString::number(empty), empty = 0;
            placement += QChar(piece);
        }
        if (empty)
            placement += QString::number(empty);
        if (rank)
            placement += '/';
    }
    // Les droits de roque : roi et tour à leur place d'origine.
    const auto at = [&](int square, char piece) { return squares[size_t(square)] == piece; };
    QString castling;
    if (at(4, 'K') && at(7, 'R'))
        castling += 'K';
    if (at(4, 'K') && at(0, 'R'))
        castling += 'Q';
    if (at(60, 'k') && at(63, 'r'))
        castling += 'k';
    if (at(60, 'k') && at(56, 'r'))
        castling += 'q';
    return placement + " w " + (castling.isEmpty() ? "-" : castling) + " - 0 1";
}
} // namespace t64
