// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QUuid>
#include <algorithm>
#include <cmath>

QVariantMap Controller::preferences() const {
    QVariantMap defaults{{"enginePath", ""},
                         {"uciOptions", QVariantMap()},
                         {"threads", 1},
                         {"hash", 128},
                         {"multiPV", 3},
                         {"skill", 12}, {"playElo", 0},
                         {"moveTime", 1000},
                         {"analysisDepth", 24},
                         {"importWorkers", 2},
                         {"syzygyPath", ""}, {"bookPath", ""},
                         {"lightTheme", false},
                         {"fontFamily", QGuiApplication::font().family()},
                         {"fontScale", 1.0}, {"uiScale", 1.0},
                         {"sidebarCompact", false}, {"highContrast", false},
                         {"reduceMotion", false}, {"language", "fr"},
                         {"analysisPanelRatio", .58}, {"notebookLayout", "table"},
                         {"coordinates", true},
                         {"pieceStyle", "cburnett"},
                         {"customPiecePath", ""},
                         {"boardLight", "#e9dfcb"},
                         {"boardDark", "#71838c"},
                         {"boardHighlight", "#d6a166"},
                         {"backgroundColor", ""},
                         {"accentColor", "#ef997a"},
                         {"legalMoves", true},
                         {"lastMove", true},
                         {"dailyGoal", 10},
                         {"autoBackup", true},
                         {"lastBackup", settings_.value("backup/last")},
                         {"engineProfiles", QVariantList()}};
    for (auto it = defaults.begin(); it != defaults.end(); ++it)
        it.value() = settings_.value(it.key(), it.value());
    defaults["dataPath"] = dataPath_;
    static const auto fonts = QFontDatabase::families();
    for (const auto &key : {"fontScale", "uiScale", "analysisPanelRatio"}) {
        const double value = defaults[key].toDouble();
        const bool ratio = key == QString("analysisPanelRatio");
        defaults[key] = std::isfinite(value) ? std::clamp(value, ratio ? .3 : .9, ratio ? .75 : key == QString("fontScale") ? 1.5 : 1.25) : ratio ? .58 : 1.0;
    }
    if (!fonts.contains(defaults["fontFamily"].toString())) defaults["fontFamily"] = QGuiApplication::font().family();
    defaults["fontFamilies"] = fonts;
    QColor bg(defaults["backgroundColor"].toString());
    if (!bg.isValid())
        bg = QColor(defaults["lightTheme"].toBool() ? "#f3efe6" : "#171b21");
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
    defaults["panel"] = blend(.045);
    defaults["raised"] = blend(.085);
    defaults["line"] = blend(.17);
    defaults["foreground"] = light ? "#242930" : "#f1ede4";
    defaults["muted"] = light ? "#565b63" : "#b6bac2";
    if (defaults["highContrast"].toBool()) {
        defaults["foreground"] = light ? "#000000" : "#ffffff";
        defaults["muted"] = light ? "#202020" : "#eeeeee";
        defaults["line"] = light ? "#656565" : "#b0b0b0";
    }
    if (!settings_.contains("accentColor"))
        defaults["accentColor"] = light ? "#a1442b" : "#ef997a";
    return defaults;
}
void Controller::setPreference(const QString &key, const QVariant &value) {
    try {
        const QMap<QString, QPair<int, int>> limits{{"threads", {1, 32}},       {"hash", {16, 4096}},
                                                    {"multiPV", {1, 5}},        {"skill", {0, 20}}, {"playElo", {0, 4000}},
                                                    {"moveTime", {100, 10000}}, {"analysisDepth", {1, 99}},
                                                    {"dailyGoal", {1, 1000}},   {"importWorkers", {1, 8}}};
        QVariant clean = value;
        if (limits.contains(key)) {
            bool ok;
            int n = value.toInt(&ok);
            if (!ok)
                throw std::runtime_error("Nombre invalide.");
            clean = std::clamp(n, limits[key].first, limits[key].second);
        } else if (key == "fontScale" || key == "uiScale" || key == "analysisPanelRatio") {
            bool ok = false; const double number = value.toDouble(&ok);
            if (!ok || !std::isfinite(number)) throw std::runtime_error("Échelle invalide.");
            clean = key == "analysisPanelRatio" ? std::clamp(number,.3,.75) : std::clamp(number,.9,key == "fontScale" ? 1.5 : 1.25);
        } else if (key == "fontFamily") {
            clean = QFontDatabase::families().contains(value.toString()) ? value.toString() : QGuiApplication::font().family();
        } else if (key == "notebookLayout") {
            if (!QStringList{"table", "flow"}.contains(value.toString())) return;
        } else if (key == "language") {
            if (!QStringList{"auto","fr","en"}.contains(value.toString())) return;
        } else if (QStringList{"sidebarCompact", "highContrast", "reduceMotion", "lightTheme", "coordinates", "legalMoves", "lastMove", "autoBackup"}.contains(
                       key))
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
        } else if (key == "enginePath" || key == "syzygyPath" || key == "bookPath") {
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
            emit notationChanged();
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
void Controller::applyBoardPreset(const QString &name) {
    static const QMap<QString, QStringList> presets{
        {"Trait64", {"#e9dfcb", "#71838c", "#d6a166"}}, {"Sauge", {"#dce4d3", "#718c7c", "#c9ba55"}},
        {"Noyer", {"#f0d9b5", "#b58863", "#dac35e"}},   {"Océan", {"#dee7eb", "#6487a3", "#a3bb52"}},
        {"Lavande", {"#ece4f4", "#8b779e", "#d8bd70"}}, {"Rose", {"#f5e2dc", "#b77980", "#cbba63"}},
        {"Ardoise", {"#e5e7e8", "#727b86", "#8dbac2"}}, {"Contraste", {"#ffffff", "#494949", "#e2ab31"}},
        {"Sable", {"#eee6cf", "#9e986f", "#bdad4b"}}};
    if (!presets.contains(name))
        return;
    const auto c = presets[name];
    settings_.setValue("boardLight", c[0]);
    settings_.setValue("boardDark", c[1]);
    settings_.setValue("boardHighlight", c[2]);
    emit preferencesChanged();
}
void Controller::retranslateUi(QObject *window) {
    if (auto *engine = qmlEngine(window)) engine->retranslate();
}
void Controller::resetAppearance() {
    for (const auto &key :
         QStringList{"fontFamily", "fontScale", "uiScale", "sidebarCompact", "highContrast", "reduceMotion", "analysisPanelRatio", "lightTheme", "pieceStyle", "boardLight", "boardDark", "boardHighlight",
                     "backgroundColor", "accentColor", "coordinates", "legalMoves", "lastMove"})
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
        settings_.setValue("customPiecePath", destination);
        settings_.setValue("pieceStyle", "custom");
        emit preferencesChanged();
        emit message("Jeu de pièces personnel installé.");
    } catch (const std::exception &e) {
        if (!destination.isEmpty())
            QDir(destination).removeRecursively();
        fail(e);
    }
}
