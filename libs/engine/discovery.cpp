// SPDX-License-Identifier: AGPL-3.0-or-later
#include "discovery.h"
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <algorithm>

namespace t64 {
namespace {
// Les moteurs UCI qu'on rencontre réellement. La liste sert à reconnaître un fichier
// par son nom : on ne lance rien pour vérifier, ce serait exécuter n'importe quoi.
const QStringList &knownEngines() {
    static const QStringList names{"stockfish", "lc0",      "leela",    "komodo",          "dragon",
                                   "berserk",   "ethereal", "obsidian", "caissa",          "koivisto",
                                   "rubichess", "igel",     "clover",   "viridithas",      "seer",
                                   "fruit",     "crafty",   "gnuchess", "fairy-stockfish", "torch",
                                   "houdini",   "fritz",    "rybka",    "shredder"};
    return names;
}
// Ce qui porte le nom d'un moteur sans en être un.
const QStringList &notPrograms() {
    static const QStringList suffixes{".txt",      ".md",   ".pdf", ".zip", ".tar",     ".gz",  ".xz",
                                      ".7z",       ".nnue", ".bin", ".pgn", ".html",    ".png", ".svg",
                                      ".json",     ".ini",  ".cfg", ".log", ".so",      ".dll", ".dylib",
                                      ".h",        ".cpp",  ".py",  ".sh",  ".desktop", ".deb", ".rpm",
                                      ".appimage", ".dmg",  ".pkg", ".lnk", ".bat",     ".1",   ".conf"};
    return suffixes;
}
} // namespace

bool looksLikeEngine(const QString &fileName) {
    const auto name = fileName.toLower();
    for (const auto &suffix : notPrograms())
        if (name.endsWith(suffix))
            return false;
    for (const auto &engine : knownEngines())
        if (name.startsWith(engine))
            return true;
    return false;
}

QStringList engineSearchFolders(const QString &home, const QString &pathVariable, const QStringList &extra) {
    QStringList folders = extra;
#ifdef Q_OS_WIN
    folders += pathVariable.split(';', Qt::SkipEmptyParts);
    for (const auto *variable : {"ProgramFiles", "ProgramFiles(x86)", "LOCALAPPDATA", "ProgramData"})
        if (const auto value = qEnvironmentVariable(variable); !value.isEmpty())
            folders << QDir::fromNativeSeparators(value);
#else
    folders += pathVariable.split(':', Qt::SkipEmptyParts);
    // Les dossiers que les paquets utilisent, même quand le PATH d'un lancement depuis
    // le menu ne les contient pas : /usr/games n'y est pas sous Debian et Ubuntu.
    folders << "/usr/games" << "/usr/local/games" << "/usr/bin" << "/usr/local/bin" << "/opt/homebrew/bin"
            << "/snap/bin" << "/var/lib/flatpak/exports/bin" << "/opt" << "/Applications";
#endif
    // Et là où l'on pose un moteur téléchargé à la main.
    if (!home.isEmpty())
        for (const auto *relative : {".local/bin", "bin", "Downloads", "Téléchargements", "Desktop", "Bureau",
                                     "Documents", "stockfish", "Stockfish", "engines", "Engines", "chess",
                                     "Chess", "Applications", ".local/share/flatpak/exports/bin"})
            folders << home + "/" + relative;
    QStringList unique;
    QSet<QString> seen;
    for (const auto &folder : folders) {
        const auto clean = QDir::cleanPath(folder);
        if (!clean.isEmpty() && !seen.contains(clean)) {
            seen.insert(clean);
            unique << clean;
        }
    }
    return unique;
}

QStringList findEngines(const QStringList &folders) {
    QStringList found;
    QSet<QString> seen;
    const auto consider = [&](const QFileInfo &file) {
        if (!file.isFile() || !file.isExecutable() || !looksLikeEngine(file.fileName()))
            return;
        // Deux chemins vers le même fichier (lien, dossier cité deux fois) ne font qu'un.
        const auto real = file.canonicalFilePath();
        if (real.isEmpty() || seen.contains(real))
            return;
        seen.insert(real);
        found << file.absoluteFilePath();
    };
    for (const auto &folder : folders) {
        const QDir dir(folder);
        if (!dir.exists())
            continue;
        // Un dossier comme /usr/bin compte des milliers de fichiers : on ne demande au
        // système que ceux dont le nom commence comme un moteur.
        QStringList patterns;
        for (const auto &engine : knownEngines())
            patterns << engine + "*";
        for (const auto &file : dir.entryInfoList(patterns, QDir::Files))
            consider(file);
        // Une archive décompressée laisse le moteur un cran plus bas :
        // « Téléchargements/stockfish-ubuntu-x86-64-avx2/stockfish/… ». Deux niveaux,
        // et seulement dans les sous-dossiers qui portent eux-mêmes un nom de moteur.
        for (const auto &sub : dir.entryInfoList(patterns, QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QDir inner(sub.absoluteFilePath());
            for (const auto &file : inner.entryInfoList(patterns, QDir::Files))
                consider(file);
            for (const auto &deeper : inner.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
                for (const auto &file : QDir(deeper.absoluteFilePath()).entryInfoList(patterns, QDir::Files))
                    consider(file);
        }
        if (found.size() >= 64)
            break;
    }
    std::stable_sort(found.begin(), found.end(), [](const QString &a, const QString &b) {
        const bool first = QFileInfo(a).fileName().startsWith("stockfish", Qt::CaseInsensitive);
        const bool second = QFileInfo(b).fileName().startsWith("stockfish", Qt::CaseInsensitive);
        return first != second ? first : false;
    });
    return found;
}
} // namespace t64
