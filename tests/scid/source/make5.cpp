// Écrit une base SCID 5 avec le code de SCID lui-même, pour les essais de Trait64.
#include "scidbase.h"
#include <cstdio>
#include <string>
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    scidBaseT base;
    if (auto e = base.open("SCID5", FMODE_Create, argv[2])) { std::printf("open %d\n", e); return 1; }
    std::string msg;
    auto e = base.importGames(ICodecDatabase::PGN, argv[1], {}, msg);
    std::printf("import %d games %u %s\n", e, base.numGames(), msg.c_str());
    base.Close();
    return e;
}
