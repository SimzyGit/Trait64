// SPDX-License-Identifier: AGPL-3.0-or-later
#include "qr.h"
#include "qrcodegen.hpp"
#include <stdexcept>
namespace t64 {
std::string lichessAnalysisUrl(const std::string &fen, bool chess960) {
    std::string path = fen;
    for (auto &c : path)
        if (c == ' ')
            c = '_';
    return std::string("https://lichess.org/analysis/") + (chess960 ? "chess960/" : "standard/") + path;
}

std::vector<std::vector<bool>> qrModules(const std::string &text) {
    try {
        const auto code = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
        const int size = code.getSize(), border = 4;
        std::vector<std::vector<bool>> grid(size_t(size + 2 * border),
                                            std::vector<bool>(size_t(size + 2 * border)));
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                grid[size_t(y + border)][size_t(x + border)] = code.getModule(x, y);
        return grid;
    } catch (const qrcodegen::data_too_long &) {
        return {};
    }
}

std::string qrSvg(const std::string &text) {
    try {
        const auto code = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
        const int size = code.getSize(), border = 4, side = size + 2 * border;
        // Un seul chemin, un carré par module : quelques kilo-octets pour une position.
        std::string path;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                if (code.getModule(x, y))
                    path += "M" + std::to_string(x + border) + "," + std::to_string(y + border) + "h1v1h-1z";
        const auto box = std::to_string(side);
        return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " + box + " " + box +
               "\" shape-rendering=\"crispEdges\"><rect width=\"100%\" height=\"100%\" fill=\"#fff\"/><path "
               "d=\"" +
               path + "\" fill=\"#000\"/></svg>";
    } catch (const qrcodegen::data_too_long &) {
        return {};
    }
}
} // namespace t64
