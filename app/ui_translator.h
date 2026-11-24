// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMap>
#include <QReadWriteLock>
#include <QTranslator>
namespace t64 {
// Immutable offline catalogue. Only presentation strings are translated. The software is
// written in French; each other language has its catalogue in translations/<code>.json,
// looked up by the French text. A phrase missing from a catalogue falls back to English,
// then to French: never to nothing.
class UiTranslator final : public QTranslator {
public:
    UiTranslator() {
        english_ = load("en");
        QCoreApplication::installTranslator(this);
    }
    ~UiTranslator() override { QCoreApplication::removeTranslator(this); }
    // "fr", "en", "es", or "auto" (the system language when it is one of these, else English).
    static QString resolve(const QString &language) {
        if (language != "auto")
            return language;
        switch (QLocale::system().language()) {
        case QLocale::French: return "fr";
        case QLocale::Spanish: return "es";
        default: return "en";
        }
    }
    void setLanguage(const QString &language) {
        const auto code = resolve(language);
        QWriteLocker lock(&lock_);
        language_ = code;
        other_ = code == "en" || code == "fr" ? QJsonObject() : load(code);
    }
    bool isEmpty() const override { return false; }
    // Un même mot français peut se traduire de deux façons : qsTr("Annuler", "coup")
    // se cherche sous la clé « Annuler|coup ».
    QString translate(const char *, const char *source, const char *context = nullptr,
                      int = -1) const override {
        const QString text = context && *context
                                 ? QString::fromUtf8(source) + '|' + QString::fromUtf8(context)
                                 : QString::fromUtf8(source);
        QReadLocker lock(&lock_);
        if (language_ == "en") return english_.value(text).toString();
        if (language_ != "fr") {
            const auto found = other_.value(text).toString();
            if (!found.isEmpty()) return found;
            // Qt's own standard buttons, then English for a phrase not translated yet.
            const auto button = buttons(language_).value(text);
            return button.isEmpty() ? english_.value(text).toString() : button;
        }
        return buttons("fr").value(text);
    }
    bool english() const { QReadLocker lock(&lock_); return language_ == "en"; }
    QString language() const { QReadLocker lock(&lock_); return language_; }
    QString display(const QString &text) const {
        auto translated = translate("Trait64",text.toUtf8().constData());
        if (!translated.isEmpty())
            return translated;
        // Les messages du cœur qui finissent par un détail (« Coup illégal : e2e5 ») :
        // le début se traduit, le détail reste tel quel.
        for (const char *separator : {": ", " : ", "≥ "}) {
            const auto cut = text.indexOf(QString::fromUtf8(separator));
            if (cut <= 0)
                continue;
            const auto head = text.left(cut + QString::fromUtf8(separator).size());
            const auto start = translate("Trait64", head.toUtf8().constData());
            if (!start.isEmpty())
                return start + text.mid(head.size());
        }
        return text;
    }
private:
    static QJsonObject load(const QString &code) {
        QFile file(":/translations/" + code + ".json");
        return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
    }
    static QMap<QString,QString> buttons(const QString &code) {
        if (code == "es")
            return {{"Close","Cerrar"},{"Cancel","Cancelar"},{"Save","Guardar"},{"Open","Abrir"},{"Yes","Sí"},{"No","No"},{"Apply","Aplicar"},{"Reset","Restablecer"},{"Discard","Descartar"},{"Restore Defaults","Restaurar valores predeterminados"},{"OK","Aceptar"}};
        return {{"Close","Fermer"},{"Cancel","Annuler"},{"Save","Enregistrer"},{"Open","Ouvrir"},{"Yes","Oui"},{"No","Non"},{"Apply","Appliquer"},{"Reset","Réinitialiser"},{"Discard","Abandonner"},{"Restore Defaults","Rétablir les valeurs par défaut"}};
    }
    QJsonObject english_, other_;
    QString language_ = "fr";
    mutable QReadWriteLock lock_;
};
}
