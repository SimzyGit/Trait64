// SPDX-License-Identifier: AGPL-3.0-or-later
// Préparer n'importe quel joueur : ses parties rassemblées dans « Préparation : <nom> »,
// puis son arbre, ses choix au fil des ans, ses faiblesses observées et ses sorties de
// mon répertoire.
#include "controller.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPdfWriter>
#include <QSaveFile>
#include <QTextDocument>

namespace {
void need(bool ok, const QString &text) {
    if (!ok)
        throw std::runtime_error(text.toStdString());
}
QStringList toNames(const QVariant &value) {
    return value.toStringList();
}
} // namespace

void Controller::prepareOpponent(const QString &site, const QString &query) {
    try {
        const auto wanted = query.simplified();
        need(!wanted.isEmpty(), tr("Indiquez un pseudo, un nom ou un identifiant FIDE."));
        need(!busy(), tr("Un traitement est déjà en cours."));
        if (site == "lichess" || site == "chesscom") {
            need(t64::OnlineGames::validUser(wanted),
                 tr("Pseudo invalide : lettres, chiffres, - et _ uniquement."));
            opponent_ = {
                {"status", "downloading"}, {"site", site}, {"name", wanted}, {"names", QStringList{wanted}}};
            emit opponentChanged();
            // À la fin du téléchargement et de l'import, quel qu'en soit l'issue.
            auto *done = new QMetaObject::Connection;
            *done = connect(this, &Controller::busyChanged, this, [this, done, site, wanted] {
                if (busy())
                    return;
                disconnect(*done);
                delete done;
                qint64 source = 0;
                for (const auto &row : db_->sources())
                    if (row.toMap()["slug"].toString() == "prep:" + site + ":" + wanted.toLower())
                        source = row.toMap()["id"].toLongLong();
                finishOpponent(wanted, {wanted}, source);
            });
            importOnlineInto(site, wanted, 5000, true);
            return;
        }
        if (site == "fide" && wanted.contains(QRegularExpression("^\\d{3,10}$"))) {
            // Le nom officiel d'un identifiant FIDE, tel que Lichess le tient de la FIDE.
            opponent_ = {{"status", "searching"}, {"site", site}, {"name", wanted}};
            emit opponentChanged();
            if (!opponentNetwork_)
                opponentNetwork_ = new QNetworkAccessManager(this);
            QNetworkRequest request(QUrl("https://lichess.org/api/fide/player/" + wanted));
            request.setRawHeader("User-Agent", "Trait64 (https://github.com/SimzyGit/Trait64)");
            request.setTransferTimeout(20000);
            auto *reply = opponentNetwork_->get(request);
            connect(reply, &QNetworkReply::finished, this, [this, reply, wanted] {
                reply->deleteLater();
                const auto json = QJsonDocument::fromJson(reply->readAll()).object();
                const auto name = json["name"].toString();
                if (reply->error() != QNetworkReply::NoError || name.isEmpty()) {
                    opponent_ = {
                        {"status", "error"},
                        {"error", tr("Identifiant FIDE introuvable chez Lichess (%1).").arg(wanted)}};
                    emit opponentChanged();
                    return;
                }
                const auto names = db_->playerSpellings(name);
                const auto source = db_->collectPlayerGames(
                    {{"id", "prep:fide:" + wanted}, {"title", tr("Préparation : %1").arg(name)}}, names);
                refreshLibrary();
                finishOpponent(name, names, source);
            });
            return;
        }
        // Un nom (FIDE sans identifiant, ou joueur de la bibliothèque) : ses graphies, puis
        // ses parties, diffusions comprises si la base de référence a été mise à jour.
        const auto names = db_->playerSpellings(wanted);
        const auto source = db_->collectPlayerGames(
            {{"id", "prep:name:" + wanted.toLower()}, {"title", tr("Préparation : %1").arg(wanted)}}, names);
        refreshLibrary();
        finishOpponent(wanted, names, source);
    } catch (const std::exception &e) {
        opponent_ = {{"status", "error"}, {"error", QString::fromUtf8(e.what())}};
        emit opponentChanged();
        fail(e);
    }
}

void Controller::finishOpponent(const QString &name, const QStringList &names, qint64 source) {
    const auto white = db_->opponentTree(names, true, {}), black = db_->opponentTree(names, false, {});
    const int games = white["games"].toInt() + black["games"].toInt();
    opponent_ = {{"status", games ? "ready" : "empty"},
                 {"name", name},
                 {"names", names},
                 {"source", source},
                 {"games", games},
                 {"whiteGames", white["games"]},
                 {"blackGames", black["games"]},
                 {"whiteScore", white["score"]},
                 {"blackScore", black["score"]}};
    emit opponentChanged();
}

QVariantMap Controller::opponentTree(bool white, const QStringList &path) const {
    try {
        return db_->opponentTree(toNames(opponent_["names"]), white, path);
    } catch (const std::exception &e) {
        return {{"error", QString::fromUtf8(e.what())}};
    }
}
QVariantList Controller::opponentWeaknesses() const {
    try {
        return db_->opponentWeaknesses(toNames(opponent_["names"]));
    } catch (const std::exception &) {
        return {};
    }
}
QVariantMap Controller::opponentEvolution() const {
    try {
        return db_->playerRepertoire(toNames(opponent_["names"]), 8, 8);
    } catch (const std::exception &) {
        return {};
    }
}
QVariantMap Controller::opponentDeviations(qint64 repertoire) const {
    try {
        return db_->repertoireGap(repertoire, toNames(opponent_["names"]), 50, {}, true);
    } catch (const std::exception &e) {
        return {{"error", QString::fromUtf8(e.what())}};
    }
}
void Controller::openLine(const QStringList &path) {
    try {
        newGame();
        for (const auto &uci : path)
            move(uci);
        emit showAnalysis();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void Controller::loadGameAtPly(qint64 id, int ply) {
    loadGame(id);
    if (gameId_ != id)
        return;
    const auto line = game_.mainline();
    // Le pli compte les demi-coups joués : la position avant le coup `ply`.
    if (ply >= 0 && ply < int(line.size()))
        navigate(line[size_t(ply)]);
}

namespace {
// Un diagramme en HTML depuis une FEN, pour le PDF : cases colorées, pièces Unicode.
QString diagram(const QString &fen, int cell = 24) {
    static const QString glyphs = "PNBRQKpnbrqk", unicode = "♙♘♗♖♕♔♟♞♝♜♛♚";
    QString html = "<table cellspacing='0' cellpadding='0' style='margin:4px 0'>";
    const auto ranks = fen.section(' ', 0, 0).split('/');
    for (int row = 0; row < ranks.size() && row < 8; ++row) {
        html += "<tr>";
        int col = 0;
        for (const QChar c : ranks[row]) {
            const int empty = c.isDigit() ? c.digitValue() : 0;
            for (int k = 0; k < std::max(1, empty) && col < 8; ++k, ++col) {
                const int at = empty ? -1 : int(glyphs.indexOf(c));
                html += QString("<td width='%1' height='%1' align='center' bgcolor='%2' "
                                "style='font-size:%3pt;color:#111'>%4</td>")
                            .arg(cell)
                            .arg((row + col) % 2 ? "#9ca88e" : "#eee4d0")
                            .arg(cell * 2 / 3)
                            .arg(at >= 0 ? QString(unicode[at]) : QString(" "));
            }
        }
        html += "</tr>";
    }
    return html + "</table>";
}
} // namespace

void Controller::exportOpponentPdf(const QString &url) {
    try {
        need(opponent_["status"] == "ready", tr("Préparez d’abord un joueur."));
        const auto names = toNames(opponent_["names"]);
        const auto name = opponent_["name"].toString();
        db_->validateOutputPath(localPath(url));
        QSaveFile file(localPath(url));
        need(file.open(QIODevice::WriteOnly), file.errorString());
        {
            QPdfWriter writer(&file);
            writer.setResolution(96);
            writer.setPageSize(QPageSize(QPageSize::A4));
            writer.setTitle(tr("Préparation : %1").arg(name));
            QTextDocument doc;
            doc.setDefaultFont(QFont("DejaVu Sans", 10));
            const auto esc = [](const QString &t) { return t.toHtmlEscaped(); };
            QString html = "<h1>" + esc(tr("Préparation : %1").arg(name)) + "</h1>";
            html +=
                "<p>" +
                esc(tr("%1 partie(s) · avec les Blancs : %2 (score %3 %) · avec les Noirs : %4 (score %5 %)")
                        .arg(opponent_["games"].toInt())
                        .arg(opponent_["whiteGames"].toInt())
                        .arg(opponent_["whiteScore"].toInt())
                        .arg(opponent_["blackGames"].toInt())
                        .arg(opponent_["blackScore"].toInt())) +
                "</p>";
            // Ses lignes régulières, chacune avec le diagramme de la position où elle mène.
            const auto evolution = db_->playerRepertoire(names, 8, 6);
            for (const auto colour : {"white", "black"}) {
                html += "<h2>" +
                        esc(colour == QString("white") ? tr("Avec les Blancs") : tr("Avec les Noirs")) +
                        "</h2>";
                for (const auto &row : evolution[colour].toList()) {
                    const auto line = row.toMap();
                    html +=
                        "<p><b>" + esc(line["line"].toString()) + "</b> — " +
                        esc(tr("%1 partie(s) · %2 %").arg(line["games"].toInt()).arg(line["score"].toInt())) +
                        "</p>";
                    t64::Game g;
                    int node = 0;
                    try {
                        for (const auto &san : line["line"].toString().split(' ', Qt::SkipEmptyParts)) {
                            const auto move = san.contains('.') ? san.section('.', 1) : san;
                            if (!move.isEmpty())
                                node = g.playSan(node, move.toStdString());
                        }
                        html += diagram(QString::fromStdString(g.nodes[node].fen));
                    } catch (const std::exception &) {
                    }
                }
            }
            const auto weak = db_->opponentWeaknesses(names);
            if (!weak.isEmpty()) {
                html += "<h2>" + esc(tr("Ouvertures où il marque peu")) + "</h2><ul>";
                for (const auto &row : weak) {
                    const auto w = row.toMap();
                    html +=
                        "<li>" +
                        esc(tr("%1 avec les %2 : %3 partie(s), %4 %")
                                .arg(w["eco"].toString(), w["white"].toBool() ? tr("Blancs") : tr("Noirs"))
                                .arg(w["games"].toInt())
                                .arg(w["score"].toInt())) +
                        "</li>";
                }
                html += "</ul>";
            }
            html += "<h2>" + esc(tr("Évolution de ses choix")) + "</h2><table cellpadding='3'><tr><th>" +
                    esc(tr("Année")) + "</th><th>" + esc(tr("Blancs")) + "</th><th>" + esc(tr("Noirs")) +
                    "</th></tr>";
            for (const auto &row : evolution["years"].toList()) {
                const auto y = row.toMap();
                html += "<tr><td>" + esc(y["year"].toString()) + "</td><td>" +
                        esc(y["white"].toMap()["move"].toString()) + "</td><td>" +
                        esc(y["black"].toMap()["move"].toString()) + "</td></tr>";
            }
            html += "</table><p style='color:#666'>" +
                    esc(tr("Fréquences et scores observés dans votre bibliothèque ; ils décrivent ses "
                           "parties, ils ne prouvent rien.")) +
                    "</p>";
            doc.setHtml(html);
            doc.setPageSize(writer.pageLayout().paintRectPixels(writer.resolution()).size());
            doc.print(&writer);
        }
        need(file.commit(), file.errorString());
        emit message(tr("Dossier de préparation enregistré."));
    } catch (const std::exception &e) {
        fail(e);
    }
}
