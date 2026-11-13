// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include "hardware.h"
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImageReader>
#include <QQmlEngine>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <thread>

void Controller::unleashEngineOnce() {
    // Les anciens réglages par défaut bridaient le moteur : profondeur 24, table de
    // 4 Go au plus. Ils ont été écrits tels quels dans les préférences et dans les
    // profils de moteur ; un nouveau défaut ne les atteindrait pas. On relève, une
    // seule fois, ceux qui valent encore exactement l'ancien défaut — un réglage que
    // l'utilisateur a choisi lui-même n'est pas touché.
    if (settings_.value("engine/unleashed").toBool())
        return;
    const int oldHash = t64::legacyHashMb(), newHash = t64::recommendedHashMb();
    const auto lift = [&](QVariantMap &values) {
        bool changed = false;
        if (values.value("analysisDepth").toInt() == 24) {
            values["analysisDepth"] = 0;
            changed = true;
        }
        if (values.contains("hash") && values.value("hash").toInt() == oldHash && newHash > oldHash) {
            values["hash"] = newHash;
            changed = true;
        }
        return changed;
    };
    QVariantMap own;
    for (const auto *key : {"analysisDepth", "hash"})
        if (settings_.contains(key))
            own[key] = settings_.value(key);
    if (lift(own))
        for (auto it = own.cbegin(); it != own.cend(); ++it)
            settings_.setValue(it.key(), it.value());
    auto profiles = settings_.value("engineProfiles").toList();
    bool any = false;
    for (auto &entry : profiles) {
        auto profile = entry.toMap();
        if (lift(profile)) {
            entry = profile;
            any = true;
        }
    }
    if (any)
        settings_.setValue("engineProfiles", profiles);
    settings_.setValue("engine/unleashed", true);
}

QVariantMap Controller::preferences() const {
    QVariantMap defaults{{"enginePath", ""},
                         {"uciOptions", QVariantMap()},
                         // Mesurées sur la machine : une valeur fixe n'utilisait qu'une
                         // fraction d'un processeur moderne, et personne ne songe à la régler.
                         {"threads", t64::recommendedThreads()},
                         {"hash", t64::recommendedHashMb()},
                         {"multiPV", 3},
                         {"skill", 12},
                         {"playElo", 0},
                         {"moveTime", 1000},
                         {"analysisDepth", 0},
                         {"importWorkers", t64::recommendedImportWorkers()},
                         {"syzygyPath", ""},
                         // Le dossier d'études que l'explorateur rouvre : on travaille
                         // toujours dans le même, et le retrouver à chaque fois est une corvée.
                         {"studyFolder", ""},
                         {"bookPath", ""},
                         {"lightTheme", true},
                         {"fontFamily", QGuiApplication::font().family()},
                         {"fontScale", 1.0},
                         {"uiScale", 1.0},
                         {"sidebarCompact", false},
                         {"figurines", false},
                         {"syncAtStartup", false},
                         {"highContrast", false},
                         {"reduceMotion", false},
                         {"language", "auto"},
                         {"analysisPanelRatio", .58},
                         {"notebookLayout", "table"},
                         {"coordinates", true},
                         {"boardScale", 1.0},
                         {"evalBar", true},
                         {"pieceStyle", "cburnett"},
                         // Le chemin reste celui du jeu qui sert : les échiquiers le
                         // lisent tel quel. La liste tient les jeux installés à côté.
                         {"customPiecePath", ""},
                         {"customPieceSets", QVariantList()},
                         {"boardLight", "#e9dfcb"},
                         {"boardDark", "#71838c"},
                         {"boardHighlight", "#d6a166"},
                         {"backgroundColor", ""},
                         {"accentColor", "#ef997a"},
                         {"legalMoves", true},
                         {"lastMove", true},
                         {"engineArrows", true},
                         {"sounds", true},
                         {"wheelMoves", false},
                         {"dailyGoal", 10},
                         {"autoBackup", true},
                         {"lastBackup", settings_.value("backup/last")},
                         {"engineProfiles", QVariantList()}};
    for (auto it = defaults.begin(); it != defaults.end(); ++it)
        it.value() = settings_.value(it.key(), it.value());
    defaults["dataPath"] = dataPath_;
    // Posées après la fusion : ce sont des constats sur la machine, pas des réglages.
    // L'interface s'en sert pour dire ce qu'elle a trouvé plutôt qu'un nombre nu.
    defaults["cpuCores"] = int(std::max(1u, std::thread::hardware_concurrency()));
    defaults["memoryMb"] = double(t64::installedMemoryMb());
    static const auto fonts = QFontDatabase::families();
    for (const auto &key : {"fontScale", "uiScale", "analysisPanelRatio"}) {
        const double value = defaults[key].toDouble();
        const bool ratio = key == QString("analysisPanelRatio");
        defaults[key] = std::isfinite(value) ? std::clamp(value, ratio ? .3 : .9,
                                                          ratio                         ? .75
                                                          : key == QString("fontScale") ? 1.5
                                                                                        : 1.25)
                        : ratio              ? .58
                                             : 1.0;
    }
    // Part de la place disponible qu'occupe l'échiquier. En deçà de la moitié il devient
    // illisible, et au-delà de sa case il déborderait sur la notation.
    const double board = defaults["boardScale"].toDouble();
    defaults["boardScale"] = std::isfinite(board) ? std::clamp(board, .5, 1.0) : 1.0;
    if (!fonts.contains(defaults["fontFamily"].toString()))
        defaults["fontFamily"] = QGuiApplication::font().family();
    defaults["fontFamilies"] = fonts;
    QColor bg(defaults["backgroundColor"].toString());
    if (!bg.isValid())
        bg = QColor(defaults["lightTheme"].toBool() ? "#eceef1" : "#1f2124"); // neutral greys, no tint
    auto linear = [](double x) { return x <= 0.04045 ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4); };
    const double luminance =
        .2126 * linear(bg.redF()) + .7152 * linear(bg.greenF()) + .0722 * linear(bg.blueF());
    const bool light = luminance > .179;
    // Blend toward the opposite brightness, including pure black where lighter() cannot work.
    auto blend = [&](double factor) {
        QColor to = light ? QColor("#000000") : QColor("#ffffff");
        return QColor::fromRgbF(bg.redF() * (1 - factor) + to.redF() * factor,
                                bg.greenF() * (1 - factor) + to.greenF() * factor,
                                bg.blueF() * (1 - factor) + to.blueF() * factor)
            .name();
    };
    defaults["background"] = bg.name();
    // Light theme: white panes on a grey window, like classic desktop chess software.
    const auto towardWhite = [&](double factor) {
        return QColor::fromRgbF(bg.redF() + (1 - bg.redF()) * factor,
                                bg.greenF() + (1 - bg.greenF()) * factor,
                                bg.blueF() + (1 - bg.blueF()) * factor)
            .name();
    };
    defaults["panel"] = light ? towardWhite(.85) : blend(.045);
    defaults["raised"] = blend(.085);
    defaults["line"] = blend(.17);
    defaults["foreground"] = light ? "#1d2125" : "#e6e8eb";
    defaults["muted"] = light ? "#5c6570" : "#a4abb4";
    if (defaults["highContrast"].toBool()) {
        defaults["foreground"] = light ? "#000000" : "#ffffff";
        defaults["muted"] = light ? "#202020" : "#eeeeee";
        defaults["line"] = light ? "#656565" : "#b0b0b0";
    }
    if (!settings_.contains("accentColor"))
        defaults["accentColor"] = light ? "#2f66ad" : "#6a9fdc"; // restrained blue, as in desktop chess tools
    return defaults;
}
void Controller::setPreference(const QString &key, const QVariant &value) {
    try {
        const QMap<QString, QPair<int, int>> limits{
            {"threads", {1, 1024}},     {"hash", {16, 262144}},   {"multiPV", {1, 5}},
            {"skill", {0, 20}},         {"playElo", {0, 4000}},   {"moveTime", {100, 10000}},
            {"analysisDepth", {0, 99}}, {"dailyGoal", {1, 1000}}, {"importWorkers", {1, 8}}};
        QVariant clean = value;
        if (limits.contains(key)) {
            bool ok;
            int n = value.toInt(&ok);
            if (!ok)
                throw std::runtime_error("Nombre invalide.");
            clean = std::clamp(n, limits[key].first, limits[key].second);
        } else if (key == "fontScale" || key == "uiScale" || key == "analysisPanelRatio") {
            bool ok = false;
            const double number = value.toDouble(&ok);
            if (!ok || !std::isfinite(number))
                throw std::runtime_error("Échelle invalide.");
            clean = key == "analysisPanelRatio" ? std::clamp(number, .3, .75)
                                                : std::clamp(number, .9, key == "fontScale" ? 1.5 : 1.25);
        } else if (key == "fontFamily") {
            clean = QFontDatabase::families().contains(value.toString()) ? value.toString()
                                                                         : QGuiApplication::font().family();
        } else if (key == "notebookLayout") {
            if (!QStringList{"table", "flow"}.contains(value.toString()))
                return;
        } else if (key == "language") {
            if (!QStringList{"auto", "fr", "en"}.contains(value.toString()))
                return;
        } else if (QStringList{"sidebarCompact", "figurines", "syncAtStartup", "highContrast", "reduceMotion",
                               "lightTheme", "coordinates", "legalMoves", "lastMove", "engineArrows",
                               "sounds", "wheelMoves", "autoBackup"}
                       .contains(key))
            clean = value.toBool();
        else if (QStringList{"boardLight", "boardDark", "boardHighlight", "backgroundColor", "accentColor"}
                     .contains(key)) {
            if (key == "backgroundColor" && value.toString().isEmpty())
                clean = "";
            else {
                QColor color(value.toString());
                if (!color.isValid())
                    throw std::runtime_error("Couleur invalide : utilisez #RRGGBB.");
                color.setAlpha(255);
                clean = color.name();
            }
        } else if (key == "pieceStyle") {
            if (!QStringList{"cburnett", "merida", "spatial", "pixel", "unicode", "custom"}.contains(
                    value.toString()))
                return;
            if (value == "custom" && settings_.value("customPiecePath").toString().isEmpty())
                return;
        } else if (key == "enginePath" || key == "syzygyPath" || key == "bookPath" || key == "studyFolder") {
            clean = localPath(value.toString());
            if (clean.toString().contains('\n') || clean.toString().contains('\r'))
                throw std::runtime_error("Chemin invalide.");
        } else
            return;
        if (key == "enginePath" && settings_.value(key) != clean) {
            settings_.setValue("uciOptions", QVariantMap());
            emit engineChanged();
        }
        settings_.setValue(key, clean);
        if (key == "language") {
            translator_->setLanguage(clean.toString());
            refreshNotation();
        }
        if (key == "figurines") {
            refresh();
        }
        if (key == "enginePath")
            emit engineChanged();
        if (key == "bookPath") {
            openBook();
            refresh();
        }
        emit preferencesChanged();
    } catch (const std::exception &e) {
        fail(e);
    }
}
// Une seule table pour les deux usages : l'interface listait les noms de son côté, si
// bien qu'une palette ajoutée ici pouvait n'apparaître nulle part. Une liste et non une
// table associative : l'ordre est voulu — les claires d'abord, les sombres ensuite.
static const std::vector<std::pair<QString, QStringList>> &boardPresetTable() {
    static const std::vector<std::pair<QString, QStringList>> presets{
        {"Trait64", {"#e9dfcb", "#71838c", "#d6a166"}},
        {"Sauge", {"#dce4d3", "#718c7c", "#c9ba55"}},
        {"Noyer", {"#f0d9b5", "#b58863", "#dac35e"}},
        {"Océan", {"#dee7eb", "#6487a3", "#a3bb52"}},
        {"Lavande", {"#ece4f4", "#8b779e", "#d8bd70"}},
        {"Rose", {"#f5e2dc", "#b77980", "#cbba63"}},
        {"Ardoise", {"#e5e7e8", "#727b86", "#8dbac2"}},
        {"Contraste", {"#ffffff", "#494949", "#e2ab31"}},
        {"Sable", {"#eee6cf", "#9e986f", "#bdad4b"}},
        // Les neuf premiers sont clairs : sur un thème sombre, l'échiquier éclairait
        // l'écran à lui seul. Ceux-ci tiennent dans la pénombre.
        {"Nuit", {"#9aa3ad", "#4a5560", "#7f6f3f"}},
        {"Forêt", {"#b9c4ac", "#4f6350", "#8a8b42"}},
        {"Café", {"#c9b39a", "#6b4f3a", "#9b7b3c"}},
        {"Encre", {"#aab2c4", "#454d66", "#6f6a9c"}},
        {"Brique", {"#dcc4b4", "#8c5544", "#b58243"}},
        {"Jade", {"#cfe0d8", "#527a6c", "#9aa84f"}},
        {"Papier", {"#f4f1ea", "#b6b0a2", "#c8b05a"}}};
    return presets;
}
QVariantList Controller::boardPresets() const {
    QVariantList list;
    for (const auto &[name, c] : boardPresetTable())
        list.append(QVariantMap{{"name", name}, {"light", c[0]}, {"dark", c[1]}, {"highlight", c[2]}});
    return list;
}
void Controller::applyBoardPreset(const QString &name) {
    const auto &presets = boardPresetTable();
    const auto found = std::find_if(presets.cbegin(), presets.cend(),
                                    [&](const auto &entry) { return entry.first == name; });
    if (found == presets.cend())
        return;
    const auto c = found->second;
    settings_.setValue("boardLight", c[0]);
    settings_.setValue("boardDark", c[1]);
    settings_.setValue("boardHighlight", c[2]);
    emit preferencesChanged();
}
void Controller::retranslateUi(QObject *window) {
    if (auto *engine = qmlEngine(window))
        engine->retranslate();
}
void Controller::resetAppearance() {
    for (const auto &key : QStringList{
             "fontFamily", "fontScale", "uiScale", "sidebarCompact", "highContrast", "reduceMotion",
             "analysisPanelRatio", "lightTheme", "pieceStyle", "boardLight", "boardDark", "boardHighlight",
             "backgroundColor", "accentColor", "coordinates", "legalMoves", "lastMove", "engineArrows"})
        settings_.remove(key);
    emit preferencesChanged();
}
void Controller::importPieceSet(const QString &folder) {
    QString destination;
    try {
        const QDir src(localPath(folder));
        QStringList names;
        for (QChar c : QString("wb"))
            for (QChar p : QString("KQRBNP")) {
                QString name = QString(c) + p + ".svg";
                QFileInfo f(src.filePath(name));
                if (!f.isFile() || f.size() > 1024 * 1024 || !QImageReader(f.filePath()).canRead())
                    throw std::runtime_error("Le dossier doit contenir 12 SVG lisibles : wK, wQ, wR, wB, wN, "
                                             "wP et bK, bQ, bR, bB, bN, bP.");
                names << name;
            }
        destination = dataPath_ + "/pieces/" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        QDir().mkpath(destination);
        for (const auto &name : names)
            if (!QFile::copy(src.filePath(name), destination + "/" + name))
                throw std::runtime_error("Copie du jeu de pièces impossible.");
        // Un nom lisible plutôt qu'un identifiant : c'est celui du dossier choisi, et
        // c'est sous ce nom que le joueur retrouvera son jeu dans la liste.
        auto label = src.dirName().simplified().left(40);
        if (label.isEmpty())
            label = "Jeu personnel";
        auto installed = settings_.value("customPieceSets").toList();
        if (installed.size() >= 24)
            throw std::runtime_error("Vingt-quatre jeux de pièces installés : retirez-en un.");
        installed.append(QVariantMap{{"name", label}, {"path", destination}});
        settings_.setValue("customPieceSets", installed);
        settings_.setValue("customPiecePath", destination);
        settings_.setValue("pieceStyle", "custom");
        emit preferencesChanged();
        emit message(tr("Jeu de pièces « %1 » installé.").arg(label));
    } catch (const std::exception &e) {
        if (!destination.isEmpty())
            QDir(destination).removeRecursively();
        fail(e);
    }
}
// Avant que plusieurs jeux soient possibles, un seul chemin était retenu : sans cette
// reprise, le jeu déjà installé continuerait de s'afficher sans figurer dans la liste,
// donc sans pouvoir être retiré ni remis en service.
void Controller::adoptLoneCustomPieceSet() {
    const auto path = settings_.value("customPiecePath").toString();
    if (path.isEmpty() || !settings_.value("customPieceSets").toList().isEmpty())
        return;
    settings_.setValue("customPieceSets",
                       QVariantList{QVariantMap{{"name", QStringLiteral("Jeu personnel")}, {"path", path}}});
}
void Controller::useCustomPieceSet(int index) {
    const auto installed = settings_.value("customPieceSets").toList();
    if (index < 0 || index >= installed.size())
        return;
    settings_.setValue("customPiecePath", installed[index].toMap()["path"]);
    settings_.setValue("pieceStyle", "custom");
    emit preferencesChanged();
}
void Controller::removeCustomPieceSet(int index) {
    auto installed = settings_.value("customPieceSets").toList();
    if (index < 0 || index >= installed.size())
        return;
    const auto path = installed.takeAt(index).toMap()["path"].toString();
    // On n'efface que ce que nous avons nous-mêmes copié : un chemin venu d'ailleurs
    // désignerait un dossier du joueur, qui n'a pas à disparaître.
    if (path.startsWith(dataPath_ + "/pieces/"))
        QDir(path).removeRecursively();
    settings_.setValue("customPieceSets", installed);
    // Le jeu retiré était peut-être celui qui servait : on reprend un jeu fourni
    // plutôt que de laisser l'échiquier chercher des fichiers qui n'existent plus.
    if (settings_.value("customPiecePath").toString() == path) {
        settings_.setValue("customPiecePath",
                           installed.isEmpty() ? QVariant("") : installed[0].toMap()["path"]);
        if (installed.isEmpty())
            settings_.setValue("pieceStyle", "cburnett");
    }
    emit preferencesChanged();
    emit message(tr("Jeu de pièces retiré."));
}
