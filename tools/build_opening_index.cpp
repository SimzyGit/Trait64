// SPDX-License-Identifier: AGPL-3.0-or-later
// g++ -std=c++23 -O2 -Ilibs/core -Ithird_party/chess-library tools/build_opening_index.cpp libs/core/game.cpp
// libs/core/formats.cpp -o /tmp/t64-openings
#include "game.h"
#include <fstream>
#include <iostream>
#include <map>
int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    std::map<std::string, std::pair<std::string, std::string>> index;
    for (char file = 'a'; file <= 'e'; ++file) {
        std::ifstream input(std::string(argv[1]) + "/" + file + ".tsv");
        if (!input)
            return 3;
        std::string line;
        std::getline(input, line);
        while (std::getline(input, line)) {
            auto a = line.find('\t'), b = line.find('\t', a + 1);
            if (a == std::string::npos || b == std::string::npos)
                return 4;
            try {
                auto games = t64::parsePgn(line.substr(b + 1));
                auto &g = games.at(0);
                index[t64::positionKey(g.nodes[g.mainline().back()].fen)] = {line.substr(0, a),
                                                                             line.substr(a + 1, b - a - 1)};
            } catch (const std::exception &e) {
                std::cerr << line << "\n" << e.what() << "\n";
                return 5;
            }
        }
    }
    std::ofstream out(argv[2]);
    for (const auto &[fen, name] : index)
        out << fen << '\t' << name.first << '\t' << name.second << '\n';
    std::cout << index.size() << " indexed opening positions\n";
}
