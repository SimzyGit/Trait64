// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMap>
#include <QTranslator>
#include <atomic>
namespace t64 {
// Immutable offline catalogue. Only presentation strings are translated.
class UiTranslator final : public QTranslator {
public:
    UiTranslator() {
        QFile file(":/translations/en.json");
        if (file.open(QIODevice::ReadOnly)) english_ = QJsonDocument::fromJson(file.readAll()).object();
        QCoreApplication::installTranslator(this);
    }
    ~UiTranslator() override { QCoreApplication::removeTranslator(this); }
    void setLanguage(const QString &language) {
        useEnglish_.store(language == "en" || (language == "auto" && QLocale::system().language() != QLocale::French));
    }
    bool isEmpty() const override { return false; }
    QString translate(const char *, const char *source, const char * = nullptr, int = -1) const override {
        const QString text = QString::fromUtf8(source);
        if (useEnglish_.load()) return english_.value(text).toString();
        static const QMap<QString,QString> buttons{{"Close","Fermer"},{"Cancel","Annuler"},{"Save","Enregistrer"},{"Open","Ouvrir"},{"Yes","Oui"},{"No","Non"},{"Apply","Appliquer"},{"Reset","Réinitialiser"},{"Discard","Abandonner"},{"Restore Defaults","Rétablir les valeurs par défaut"}};
        return buttons.value(text);
    }
    QString display(const QString &text) const {
        auto translated = translate("Trait64",text.toUtf8().constData());
        return translated.isNull() ? text : translated;
    }
private:
    QJsonObject english_;
    std::atomic_bool useEnglish_{false};
};
}
