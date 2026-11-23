// SPDX-License-Identifier: AGPL-3.0-or-later
// Statistiques personnelles d'un profil, tous comptes réunis : sa force par cadence, sa
// performance par ouverture et sa régularité des dernières semaines. Rien n'est estimé
// au-delà de ce que les parties disent : un classement est celui inscrit dans la partie,
// une performance celle de la formule FIDE sur les adversaires classés.
#include "database.h"
#include <QDate>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>
#include <stdexcept>

namespace t64 {
namespace {
QString cadenceOf(const QString &header) {
    // « 180+2 » : la durée estimée d'une partie, base + 40 × incrément, comme Lichess.
    static const QRegularExpression control("\\[TimeControl \"(\\d+)(?:\\+(\\d+))?\"\\]");
    const auto match = control.match(header);
    if (!match.hasMatch())
        return header.contains("Correspondence", Qt::CaseInsensitive) ? QString("correspondence")
                                                                      : QString("classical");
    const int seconds = match.captured(1).toInt() + 40 * match.captured(2).toInt();
    return seconds < 180 ? "bullet" : seconds < 480 ? "blitz" : seconds < 1500 ? "rapid" : "classical";
}
} // namespace

QVariantMap Database::personalStats(const QStringList &names, const QDate &today) const {
    QVariantMap out{{"cadences", QVariantList()}, {"openings", QVariantList()}, {"weeks", QVariantList()}};
    if (names.isEmpty())
        return out;
    QStringList marks;
    for (qsizetype i = 0; i < names.size(); ++i)
        marks << "?";
    QSqlQuery q(db_);
    q.setForwardOnly(true);
    q.prepare(
        "SELECT white,result,eco,date,white_elo,black_elo,substr(pgn,1,800) FROM games WHERE deleted=0 AND "
        "(white COLLATE NOCASE IN (" +
        marks.join(',') + ") OR black COLLATE NOCASE IN (" + marks.join(',') + ")) ORDER BY date,id");
    for (int i = 0; i < 2; ++i)
        for (const auto &n : names)
            q.addBindValue(n);
    if (!q.exec())
        throw std::runtime_error(q.lastError().text().toStdString());
    struct Cadence {
        int games = 0, finished = 0, latest = 0;
        double points = 0;
        QList<int> ratings;
        QString last;
    };
    struct Opening {
        int games = 0, wins = 0, losses = 0, rated = 0;
        double points = 0;
        qint64 opponents = 0;
    };
    QMap<QString, Cadence> cadences;
    QMap<QString, Opening> openings;
    QMap<int, int> weeks; // semaines écoulées → parties
    while (q.next()) {
        const bool white = names.contains(q.value(0).toString(), Qt::CaseInsensitive);
        const auto result = q.value(1).toString();
        const double points = result == "1-0"       ? (white ? 1 : 0)
                              : result == "0-1"     ? (white ? 0 : 1)
                              : result == "1/2-1/2" ? .5
                                                    : -1;
        const auto date = q.value(3).toString();
        const int mine = (white ? q.value(4) : q.value(5)).toInt(),
                  theirs = (white ? q.value(5) : q.value(4)).toInt();
        auto &c = cadences[cadenceOf(q.value(6).toString())];
        ++c.games;
        if (points >= 0)
            ++c.finished, c.points += points;
        if (mine > 0) {
            c.ratings << mine;
            c.latest = mine;
            c.last = date;
        }
        if (const auto eco = q.value(2).toString().left(3); !eco.isEmpty() && points >= 0) {
            auto &o = openings[eco];
            ++o.games;
            o.points += points;
            o.wins += points == 1;
            o.losses += points == 0;
            if (theirs > 0)
                ++o.rated, o.opponents += theirs;
        }
        const auto played = QDate::fromString(date.left(10), "yyyy.MM.dd");
        if (played.isValid()) {
            const int ago = int(played.daysTo(today) / 7);
            if (ago >= 0 && ago < 12)
                ++weeks[ago];
        }
    }
    static const QStringList order{"bullet", "blitz", "rapid", "classical", "correspondence"};
    QVariantList cadenceRows;
    for (const auto &name : order) {
        if (!cadences.contains(name))
            continue;
        const auto &c = cadences[name];
        // La force du moment : la moyenne de ses vingt derniers classements inscrits.
        const auto recent = c.ratings.mid(std::max<qsizetype>(0, c.ratings.size() - 20));
        double sum = 0;
        for (int r : recent)
            sum += r;
        cadenceRows << QVariantMap{{"cadence", name},
                                   {"games", c.games},
                                   {"score", c.finished ? qRound(100 * c.points / c.finished) : -1},
                                   {"rating", c.latest},
                                   {"recent", recent.isEmpty() ? 0 : qRound(sum / double(recent.size()))},
                                   {"date", c.last}};
    }
    QVariantList openingRows;
    for (auto it = openings.cbegin(); it != openings.cend(); ++it) {
        if (it->games < 3)
            continue;
        // Performance FIDE : moyenne des adversaires classés + 400 × (gains − pertes) / parties.
        QVariant performance;
        if (it->rated == it->games)
            performance =
                qRound(double(it->opponents) / it->rated + 400.0 * (it->wins - it->losses) / it->games);
        openingRows << QVariantMap{{"eco", it.key()},
                                   {"games", it->games},
                                   {"score", qRound(100 * it->points / it->games)},
                                   {"performance", performance}};
    }
    std::sort(openingRows.begin(), openingRows.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()["games"].toInt() > b.toMap()["games"].toInt();
    });
    QVariantList weekRows;
    for (int w = 11; w >= 0; --w)
        weekRows << QVariantMap{{"weeksAgo", w}, {"games", weeks.value(w)}};
    out["cadences"] = cadenceRows;
    out["openings"] = openingRows.mid(0, 30);
    out["weeks"] = weekRows;
    return out;
}
} // namespace t64
