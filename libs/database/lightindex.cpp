// SPDX-License-Identifier: AGPL-3.0-or-later
// Schéma 12 : l'index allégé.
//
// Jusqu'ici chaque position de chaque partie était rangée trois fois (ligne principale,
// toutes positions, arêtes d'ouverture) : 20 Ko par partie de maître, dont plus des neuf
// dixièmes pour des positions de milieu et de fin de partie que personne d'autre n'a
// jamais jouées. Un million de parties demandait 20 Go.
//
// Désormais :
//  - les positions et l'arbre d'ouvertures ne couvrent que les `indexedPlies` premiers
//    demi-coups, là où les parties se recoupent ;
//  - chaque partie garde ses « signatures » — couples (matériel, squelette de pions)
//    rencontrés du premier au dernier coup. Elles répondent seules, et exactement, aux
//    recherches par matériel et par structure de pions ;
//  - une position exacte au-delà de l'horizon se retrouve en rejouant les seules parties
//    qui ont eu ce matériel et ces pions : quelques centaines au lieu de toute la base.
#include "cancellation.h"
#include "database.h"
#include "game.h"
#include <QFileInfo>
#include <QMutex>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>

namespace t64 {
namespace {
void run(QSqlQuery &q) {
    if (!q.exec())
        throw std::runtime_error((q.lastQuery() + ": " + q.lastError().text()).toStdString());
}
const char *const samePositions = "(key,uci) IN(SELECT key,uci FROM positions WHERE game_id=%1 AND uci<>'')";
} // namespace

// Dix compteurs de quatre bits, rois exclus : le matériel tient dans un entier court,
// se compare sans chaîne et reste lisible à qui voudrait le décoder.
qint64 Database::materialCode(std::string_view fen) {
    int counts[10] = {};
    for (const char piece : fen) {
        if (piece == ' ')
            break; // seul le placement compte, pas le « b » du trait
        if (const auto at = std::string_view("PNBRQpnbrq").find(piece); at != std::string_view::npos)
            counts[at] = std::min(counts[at] + 1, 15);
    }
    qint64 code = 0;
    for (const int count : counts)
        code = code << 4 | count;
    return code;
}
qint64 Database::materialCode(const QString &pieces) {
    return materialCode(std::string_view(pieces.toStdString()));
}

// Retire des statistiques d'ouverture ce qu'une partie y avait apporté. À appeler tant
// que ses positions et son ancien résultat sont encore en base.
// Une requête préparée une fois par connexion. Un import en rejoue une dizaine par
// partie : les recompiler à chaque fois coûtait autant que de les exécuter.
QSqlQuery &Database::statement(const QString &text) const {
    auto found = statements_.find(text);
    if (found == statements_.end()) {
        found = statements_.emplace(text, QSqlQuery(db_)).first;
        if (!found->second.prepare(text))
            throw std::runtime_error((text + ": " + found->second.lastError().text()).toStdString());
    }
    return found->second;
}

void Database::retireOpeningStats(qint64 id) {
    auto &retire =
        statement(QString("UPDATE opening_stats SET count=count-1,"
                          "white=white-(SELECT result='1-0' FROM games WHERE id=?),"
                          "draw=draw-(SELECT result='1/2-1/2' FROM games WHERE id=?),"
                          "black=black-(SELECT result='0-1' FROM games WHERE id=?) WHERE ") +
                  QString(samePositions).arg("?") + " AND (SELECT deleted FROM games WHERE id=?)=0");
    for (int i = 0; i < 5; ++i)
        retire.bindValue(i, id);
    run(retire);
    auto &empty =
        statement("DELETE FROM opening_stats WHERE count<=0 AND " + QString(samePositions).arg("?"));
    empty.bindValue(0, id);
    run(empty);
}
void Database::creditOpeningStats(qint64 id) {
    auto &q = statement("INSERT INTO opening_stats(key,uci,san,count,white,draw,black) "
                        "SELECT p.key,p.uci,MIN(p.san),1,g.result='1-0',g.result='1/2-1/2',g.result='0-1' "
                        "FROM positions p JOIN games g ON g.id=p.game_id "
                        "WHERE p.game_id=? AND p.uci<>'' AND g.deleted=0 GROUP BY p.key,p.uci "
                        "ON CONFLICT(key,uci) DO UPDATE SET count=count+1,white=white+excluded.white,"
                        "draw=draw+excluded.draw,black=black+excluded.black");
    q.bindValue(0, id);
    run(q);
}

void Database::indexGame(const Game &game, qint64 id, bool existing) {
    if (existing) {
        // Le matériel se retire d'après les signatures, tant qu'elles sont encore là :
        // sa table est rangée par matériel, pas par partie.
        auto &material =
            statement("DELETE FROM game_material WHERE game_id=? AND material IN(SELECT material "
                      "FROM game_signatures WHERE game_id=?)");
        material.bindValue(0, id);
        material.bindValue(1, id);
        run(material);
        for (const char *text : {"DELETE FROM game_signatures WHERE game_id=?",
                                 "DELETE FROM variation_positions WHERE game_id=?"}) {
            auto &erase = statement(text);
            erase.bindValue(0, id);
            run(erase);
        }
    }
    const auto line = game.mainline();
    auto &pos = statement("INSERT INTO positions(game_id,ply,key,uci,san) VALUES(?,?,?,?,?)");
    const size_t horizon = std::min<size_t>(line.size(), indexedPlies);
    for (size_t i = 0; i < horizon; ++i) {
        pos.bindValue(0, id);
        pos.bindValue(1, int(i));
        pos.bindValue(2, nodeKey(game.nodes[line[i]]));
        const bool last = i + 1 >= line.size();
        pos.bindValue(3, last ? QString() : QString::fromStdString(game.nodes[line[i + 1]].uci));
        pos.bindValue(4, last ? QString() : QString::fromStdString(game.nodes[line[i + 1]].san));
        run(pos);
    }
    std::vector<char> principal(game.nodes.size(), 0);
    for (const int n : line)
        principal[n] = 1;
    // Triées, les signatures entrent dans l'ordre de leur index : moins de pages touchées.
    // À chaque signature, si elle survient passé l'horizon : seules celles-là désignent
    // des parties à rejouer, les autres sont déjà dans l'index position par position.
    std::map<std::pair<qint64, qint64>, bool> signatures;
    std::set<qint64> materials;
    std::vector<std::pair<int, int>> pending{{0, 0}};
    while (!pending.empty()) {
        const auto [n, depth] = pending.back();
        pending.pop_back();
        const auto &node = game.nodes[n];
        const auto material = materialCode(node.fen);
        signatures[{pawnsKey(node.fen), material}] |= depth >= indexedPlies;
        materials.insert(material);
        if (!principal[n] && depth < indexedPlies) {
            auto &side = statement("INSERT INTO variation_positions(game_id,node,key) VALUES(?,?,?)");
            side.bindValue(0, id);
            side.bindValue(1, n);
            side.bindValue(2, nodeKey(node));
            run(side);
        }
        for (const int child : node.children)
            pending.push_back({child, depth + 1});
    }
    auto &sign = statement("INSERT INTO game_signatures(game_id,pawns,material,late) VALUES(?,?,?,?)");
    for (const auto &[signature, late] : signatures) {
        sign.bindValue(0, id);
        sign.bindValue(1, signature.first);
        sign.bindValue(2, signature.second);
        sign.bindValue(3, late ? 1 : 0);
        run(sign);
    }
    auto &owned = statement("INSERT INTO game_material(material,game_id) VALUES(?,?)");
    for (const auto material : materials) {
        owned.bindValue(0, material);
        owned.bindValue(1, id);
        run(owned);
    }
}

// Rassemble dans une table temporaire les parties où la position apparaît. Au-delà de
// l'horizon, ce sont celles qui ont eu ce matériel et ces pions, rejouées une fois.
// Renvoie vrai quand la table contient *toutes* les parties — position assez rare pour
// que la recherche parte d'elle — et faux quand elle ne contient que les parties
// rejouées, l'index se chargeant du reste. Le résultat est gardé tant que ni la position
// ni la base n'ont changé : paginer ne rejoue rien.
std::array<char, 64> Database::parsePlacement(const QString &text) {
    std::array<char, 64> wanted{};
    const auto tokens = text.split(QRegularExpression("[\\s,;]+"), Qt::SkipEmptyParts);
    if (tokens.isEmpty() || tokens.size() > 32)
        throw std::runtime_error("Placement invalide : de 1 à 32 pièces, par exemple « Ke1 Pd4 pd5 ».");
    for (const auto &token : tokens) {
        const auto piece = token.size() == 3 ? token[0].toLatin1() : char(0);
        const int file = token.size() == 3 ? token[1].toLatin1() - 'a' : -1;
        const int rank = token.size() == 3 ? token[2].toLatin1() - '1' : -1;
        if (!piece || !std::strchr("KQRBNPkqrbnp", piece) || file < 0 || file > 7 || rank < 0 || rank > 7)
            throw std::runtime_error(
                ("Placement invalide : « " + token + " ». Écrivez une pièce et sa case, par exemple « Ke1 ».")
                    .toStdString());
        auto &square = wanted[size_t((7 - rank) * 8 + file)];
        if (square && square != piece)
            throw std::runtime_error(
                ("Placement invalide : deux pièces sur la case de « " + token + " ».").toStdString());
        square = piece;
    }
    return wanted;
}
bool Database::placementMatches(const std::array<char, 64> &wanted, std::string_view fen) {
    size_t square = 0;
    for (const char c : fen) {
        if (c == ' ' || square >= 64)
            break;
        if (c == '/')
            continue;
        if (c >= '1' && c <= '8') {
            for (int skipped = c - '0'; skipped > 0 && square < 64; --skipped)
                if (wanted[square++])
                    return false;
        } else if (wanted[square] && wanted[square] != c)
            return false;
        else
            ++square;
    }
    return square == 64;
}
void Database::collectPlacementMatches(const QString &pattern,
                                       const std::shared_ptr<std::atomic_bool> &canceled) const {
    const auto wanted = parsePlacement(pattern);
    // Chaque page de résultats ouvre sa propre connexion : la liste trouvée est gardée
    // pour tout le programme, tant que le fichier de la base et son journal n'ont pas
    // bougé. Sans cela, tourner une page rejouerait toute la bibliothèque.
    struct Kept {
        QString stamp;
        QList<qint64> hits;
    };
    static QMutex guard;
    static Kept kept;
    QString stamp = path_ + "|" + pattern;
    for (const char *suffix : {"", "-wal"}) {
        const QFileInfo file(path_ + suffix);
        stamp += QString("|%1:%2").arg(file.size()).arg(file.lastModified().toMSecsSinceEpoch());
    }
    QList<qint64> hits;
    bool known = false;
    {
        const QMutexLocker lock(&guard);
        if (kept.stamp == stamp) {
            hits = kept.hits;
            known = true;
        }
    }
    if (!known) {
        QSqlQuery games(db_);
        games.setForwardOnly(true);
        games.prepare("SELECT id,pgn FROM games WHERE deleted=0");
        run(games);
        while (games.next()) {
            if (canceled && canceled->load())
                throw std::runtime_error("Recherche annulée.");
            const auto parsed = parsePgn(games.value(1).toString().toStdString());
            if (parsed.size() != 1)
                continue;
            for (const auto &node : parsed[0].nodes)
                if (placementMatches(wanted, node.fen)) {
                    hits << games.value(0).toLongLong();
                    break;
                }
        }
        games.finish();
        const QMutexLocker lock(&guard);
        kept = {stamp, hits};
    }
    sql("CREATE TEMP TABLE IF NOT EXISTS placement_hits(game_id INTEGER PRIMARY KEY)");
    sql("DELETE FROM placement_hits");
    QSqlQuery insert(db_);
    insert.prepare("INSERT OR IGNORE INTO placement_hits VALUES(?)");
    for (const auto id : hits) {
        insert.bindValue(0, id);
        run(insert);
    }
}
bool Database::collectPositionMatches(const QString &fen,
                                      const std::shared_ptr<std::atomic_bool> &canceled) const {
    const auto state = [this] {
        QSqlQuery q(db_);
        qint64 mark = 0;
        for (const char *probe : {"SELECT total_changes()", "PRAGMA data_version"})
            if (q.exec(probe) && q.next())
                mark = mark * 1000003 + q.value(0).toLongLong();
        return mark;
    };
    const auto key = hashKey(fen.toStdString());
    if (lateValid_ && lateKey_ == key && lateState_ == state())
        return lateSparse_;
    lateValid_ = false;
    sql("CREATE TEMP TABLE IF NOT EXISTS scan_hits(game_id INTEGER PRIMARY KEY)");
    sql("DELETE FROM scan_hits");
    QSqlQuery candidates(db_);
    candidates.setForwardOnly(true);
    candidates.prepare("SELECT g.id,g.pgn FROM game_signatures s JOIN games g ON g.id=s.game_id "
                       "WHERE s.pawns=? AND s.material=? AND s.late=1");
    candidates.addBindValue(pawnsKey(fen.toStdString()));
    candidates.addBindValue(materialCode(fen.toStdString()));
    run(candidates);
    QList<qint64> hits;
    while (candidates.next()) {
        if (canceled && canceled->load())
            throw std::runtime_error("Recherche annulée.");
        const auto games = parsePgn(candidates.value(1).toString().toStdString());
        if (games.size() != 1)
            continue;
        for (const auto &node : games[0].nodes)
            if (nodeKey(node) == key) {
                hits << candidates.value(0).toLongLong();
                break;
            }
    }
    candidates.finish();
    QSqlQuery insert(db_);
    insert.prepare("INSERT OR IGNORE INTO scan_hits VALUES(?)");
    for (const auto id : hits) {
        insert.bindValue(0, id);
        run(insert);
    }
    // Rare ou courante ? On compte dans l'index, sans aller plus loin que le seuil.
    QSqlQuery count(db_);
    count.prepare(
        QString("SELECT COUNT(*) FROM (SELECT game_id FROM positions WHERE key=? UNION SELECT game_id "
                "FROM variation_positions WHERE key=? LIMIT %1)")
            .arg(sparseMatches + 1));
    count.addBindValue(key);
    count.addBindValue(key);
    run(count);
    lateSparse_ = count.next() && count.value(0).toLongLong() <= sparseMatches;
    count.finish();
    if (lateSparse_)
        for (const char *table : {"positions", "variation_positions"}) {
            QSqlQuery fill(db_);
            fill.prepare(
                QString("INSERT OR IGNORE INTO scan_hits SELECT game_id FROM %1 WHERE key=?").arg(table));
            fill.addBindValue(key);
            run(fill);
        }
    lateKey_ = key;
    lateState_ = state();
    lateValid_ = true;
    return lateSparse_;
}

void Database::upgradeV12(int originalVersion) {
    if (originalVersion >= 12) {
        light_ = true;
        return;
    }
    if (originalVersion > 0)
        backup(path_ + ".before-v12-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".sqlite");
    sql("SAVEPOINT schema12");
    try {
        // Les signatures se tirent de l'ancien index, sans relire un seul PGN : le matériel
        // y est en toutes lettres, on ne le convertit qu'une fois par valeur distincte.
        // Deux tables : les couples (pions, matériel) de chaque partie, et à part le
        // matériel seul, rangé par matériel — une douzaine de lignes par partie au lieu
        // d'un troisième index sur toutes les signatures.
        sql("CREATE TABLE game_signatures(game_id INTEGER NOT NULL REFERENCES games(id) ON DELETE CASCADE,"
            "pawns INTEGER NOT NULL,material INTEGER NOT NULL,late INTEGER NOT NULL,PRIMARY "
            "KEY(game_id,pawns,material)) WITHOUT ROWID");
        sql("CREATE TABLE game_material(material INTEGER NOT NULL,game_id INTEGER NOT NULL REFERENCES "
            "games(id) "
            "ON DELETE CASCADE,PRIMARY KEY(material,game_id)) WITHOUT ROWID");
        sql("CREATE TEMP TABLE material_codes(text TEXT PRIMARY KEY,code INTEGER NOT NULL) WITHOUT ROWID");
        QSqlQuery distinct(db_), insert(db_);
        distinct.setForwardOnly(true);
        if (!distinct.exec("SELECT DISTINCT material FROM all_positions"))
            throw std::runtime_error(distinct.lastError().text().toStdString());
        insert.prepare("INSERT INTO material_codes VALUES(?,?)");
        while (distinct.next()) {
            insert.bindValue(0, distinct.value(0));
            insert.bindValue(1, materialCode(distinct.value(0).toString()));
            run(insert);
        }
        distinct.finish();
        // « Tardive » : la position est sur la ligne principale au-delà de l'horizon, ou
        // dans une variante dont l'ancien index ne dit pas la profondeur — dans le doute,
        // elle sera rejouée plutôt qu'oubliée.
        sql(QString(
                "INSERT INTO game_signatures SELECT a.game_id,a.pawns,m.code,MAX(COALESCE((SELECT MAX(p.ply) "
                "FROM positions p WHERE p.key=a.key AND p.game_id=a.game_id)>=%1,1)) FROM all_positions a "
                "JOIN material_codes m ON m.text=a.material GROUP BY a.game_id,a.pawns,m.code")
                .arg(indexedPlies));
        sql("INSERT OR IGNORE INTO game_material SELECT material,game_id FROM game_signatures");
        sql("DROP TABLE material_codes");
        sql("CREATE INDEX game_signatures_pawns ON game_signatures(pawns,material,late)");
        // Des variantes, seules les positions absentes de la ligne principale méritent
        // leur propre ligne : les autres se retrouvent déjà par la partie.
        sql("CREATE TABLE variation_positions(game_id INTEGER NOT NULL REFERENCES games(id) ON DELETE "
            "CASCADE,"
            "node INTEGER NOT NULL,key INTEGER NOT NULL,PRIMARY KEY(game_id,node)) WITHOUT ROWID");
        sql("INSERT INTO variation_positions SELECT a.game_id,a.node,a.key FROM all_positions a WHERE NOT "
            "EXISTS(SELECT 1 FROM positions p WHERE p.key=a.key AND p.game_id=a.game_id)");
        sql("CREATE INDEX variation_positions_key ON variation_positions(key)");
        sql("DROP TABLE all_positions");
        for (const char *name : {"edge_game_delete", "edge_add", "edge_remove", "edge_game_update"})
            sql(QString("DROP TRIGGER IF EXISTS ") + name);
        sql("DROP TABLE opening_edges");
        sql(QString("DELETE FROM positions WHERE ply>=%1").arg(indexedPlies));
        sql("DELETE FROM opening_stats");
        sql("INSERT INTO opening_stats SELECT e.key,e.uci,MIN(e.san),COUNT(*),SUM(g.result='1-0'),"
            "SUM(g.result='1/2-1/2'),SUM(g.result='0-1') FROM (SELECT game_id,key,uci,MIN(san) AS san FROM "
            "positions WHERE uci<>'' GROUP BY game_id,key,uci) e JOIN games g ON g.id=e.game_id WHERE "
            "g.deleted=0 GROUP BY e.key,e.uci");
        // Les statistiques suivent la corbeille, un résultat corrigé et une suppression
        // sans table d'arêtes : les positions de la partie suffisent à dire où agir.
        const auto there = QString(samePositions);
        sql("CREATE TRIGGER stats_game_delete BEFORE DELETE ON games WHEN OLD.deleted=0 BEGIN UPDATE "
            "opening_stats SET count=count-1,white=white-(OLD.result='1-0'),draw=draw-(OLD.result='1/2-1/2'),"
            "black=black-(OLD.result='0-1') WHERE " +
            there.arg("OLD.id") + "; DELETE FROM opening_stats WHERE count<=0 AND " + there.arg("OLD.id") +
            "; END");
        sql("CREATE TRIGGER stats_game_update AFTER UPDATE OF result,deleted ON games WHEN "
            "OLD.result<>NEW.result OR OLD.deleted<>NEW.deleted BEGIN INSERT INTO opening_stats SELECT "
            "key,uci,MIN(san),(NEW.deleted=0)-(OLD.deleted=0),(NEW.deleted=0 AND "
            "NEW.result='1-0')-(OLD.deleted=0 "
            "AND OLD.result='1-0'),(NEW.deleted=0 AND NEW.result='1/2-1/2')-(OLD.deleted=0 AND "
            "OLD.result='1/2-1/2'),(NEW.deleted=0 AND NEW.result='0-1')-(OLD.deleted=0 AND OLD.result='0-1') "
            "FROM positions WHERE game_id=NEW.id AND uci<>'' GROUP BY key,uci ON CONFLICT(key,uci) DO UPDATE "
            "SET "
            "count=count+excluded.count,white=white+excluded.white,draw=draw+excluded.draw,black=black+"
            "excluded.black; DELETE FROM opening_stats WHERE count<=0 AND " +
            there.arg("NEW.id") + "; END");
        sql("PRAGMA user_version=12");
        sql("RELEASE schema12");
    } catch (...) {
        sql("ROLLBACK TO schema12");
        sql("RELEASE schema12");
        throw;
    }
    light_ = true;
    // Une base existante rend ici au disque les neuf dixièmes de son index.
    if (originalVersion > 0)
        sql("VACUUM");
}
} // namespace t64
