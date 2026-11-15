// SPDX-License-Identifier: AGPL-3.0-or-later
// Corriger l'orthographe des noms : « Carlsen, Magnus », « Carlsen,M. » et « Magnus
// Carlsen » sont le même joueur. On les regroupe, on choisit l'écriture à garder, et les
// autres sont réécrites dans les colonnes comme dans l'en-tête du PGN.
#include "names.h"
#include "database.h"
#include <QHash>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>

namespace t64 {
QString nameKey(const QString &name, bool player) {
    // Sans accents, sans casse, la ponctuation devenue espace.
    QString plain;
    for (const QChar c : name.normalized(QString::NormalizationForm_D))
        if (c.category() != QChar::Mark_NonSpacing)
            plain += c;
    plain = plain.toLower();
    static const QRegularExpression punctuation("[^\\w,]+");
    if (!player) {
        plain.replace(',', ' ');
        return plain.replace(punctuation, " ").simplified();
    }
    // Un joueur : « Nom, Prénom » ou « Prénom Nom ». La clé est le nom et l'initiale du
    // prénom, pour que « Carlsen, M » rejoigne « Carlsen, Magnus ».
    QString surname, given;
    if (const auto comma = plain.indexOf(','); comma >= 0) {
        surname = plain.left(comma);
        given = plain.mid(comma + 1);
    } else {
        auto words = plain.replace(punctuation, " ").simplified().split(' ', Qt::SkipEmptyParts);
        if (words.isEmpty())
            return {};
        surname = words.takeLast();
        given = words.join(' ');
    }
    surname = surname.replace(punctuation, " ").simplified();
    given = given.replace(punctuation, " ").simplified();
    return surname.isEmpty() ? QString() : surname + '|' + given.left(1);
}

namespace {
QString column(const QString &field) {
    return field == "event" ? "event" : "player";
}
// L'en-tête PGN échappe les guillemets et les barres obliques inverses.
QString tag(const QString &name, const QString &value) {
    auto escaped = value;
    escaped.replace("\\", "\\\\").replace("\"", "\\\"");
    return '[' + name + " \"" + escaped + "\"]";
}
} // namespace

QVariantList Database::nameVariants(const QString &field, int limit) const {
    const bool player = column(field) == "player";
    QSqlQuery q(db_);
    q.setForwardOnly(true);
    q.exec(player ? "SELECT name,COUNT(*) FROM (SELECT white AS name FROM games WHERE deleted=0 UNION ALL "
                    "SELECT black FROM games WHERE deleted=0) WHERE name<>'' GROUP BY name"
                  : "SELECT event,COUNT(*) FROM games WHERE deleted=0 AND event<>'' GROUP BY event");
    if (q.lastError().isValid())
        throw std::runtime_error(q.lastError().text().toStdString());
    QHash<QString, QVariantList> groups;
    while (q.next()) {
        const auto name = q.value(0).toString();
        if (name == "?" || name.trimmed().isEmpty())
            continue;
        const auto key = nameKey(name, player);
        if (!key.isEmpty())
            groups[key] << QVariantMap{{"name", name}, {"games", q.value(1).toLongLong()}};
    }
    // Les groupes de plusieurs écritures, du plus joué au moins joué ; dans un groupe,
    // l'écriture la plus fréquente d'abord : c'est d'ordinaire celle à garder.
    QList<QVariantMap> found;
    for (auto &spellings : groups) {
        if (spellings.size() < 2)
            continue;
        std::sort(spellings.begin(), spellings.end(), [](const QVariant &a, const QVariant &b) {
            return a.toMap()["games"].toLongLong() > b.toMap()["games"].toLongLong();
        });
        qint64 total = 0;
        for (const auto &s : spellings)
            total += s.toMap()["games"].toLongLong();
        found << QVariantMap{{"names", spellings}, {"games", total}};
    }
    std::sort(found.begin(), found.end(), [](const QVariantMap &a, const QVariantMap &b) {
        return a["games"].toLongLong() > b["games"].toLongLong();
    });
    QVariantList result;
    for (int i = 0; i < found.size() && i < limit; ++i)
        result << found[i];
    return result;
}

qint64 Database::renameName(const QString &field, const QString &from, const QString &to) {
    const auto target = to.trimmed();
    if (target.isEmpty() || from == target)
        return 0;
    const bool player = column(field) == "player";
    if (!db_.transaction())
        throw std::runtime_error(db_.lastError().text().toStdString());
    qint64 changed = 0;
    try {
        for (const auto &[col, header] :
             player ? QList<std::pair<QString, QString>>{{"white", "White"}, {"black", "Black"}}
                    : QList<std::pair<QString, QString>>{{"event", "Event"}}) {
            QSqlQuery q(db_);
            q.prepare("UPDATE games SET " + col + "=?, pgn=replace(pgn,?,?) WHERE " + col + "=?");
            q.addBindValue(target);
            q.addBindValue(tag(header, from));
            q.addBindValue(tag(header, target));
            q.addBindValue(from);
            if (!q.exec())
                throw std::runtime_error(q.lastError().text().toStdString());
            changed += q.numRowsAffected();
        }
        if (!db_.commit())
            throw std::runtime_error(db_.lastError().text().toStdString());
    } catch (...) {
        db_.rollback();
        throw;
    }
    return changed;
}
} // namespace t64
