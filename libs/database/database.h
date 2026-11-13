// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "game.h"
#include <QDateTime>
#include <QIODevice>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariantList>
#include <QVariantMap>
#include <array>
#include <atomic>
#include <map>
#include <memory>

namespace t64 {
using RepertoireProgress = std::function<bool(qint64)>;
struct ImportProgress {
    qint64 job = 0, bytes = 0, total = 0, processed = 0, added = 0, committed = 0, committedBytes = 0;
    bool verifying = true;
};
struct Puzzle {
    QString ref, fen, solution, themes, source;
    int rating = 1500, popularity = 0;
};
class Database {
  public:
    explicit Database(const QString &path);
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;
    qint64 save(const Game &game, qint64 id = 0, const QString &collection = {});
    int importGames(const std::vector<Game> &games, const std::function<bool(int)> &progress = {});
    int importPgnFile(const QString &path, const std::function<bool(int)> &progress,
                      const QVariantMap &source = {});
    QVariantList sources() const;
    // Le fichier d'un recueil a changé de nom ou de dossier : son chemin suit.
    void moveSourcePath(const QString &from, const QString &to);
    QVariantList importHistory() const;
    qint64 createImportJob(const QString &path, const QVariantMap &source = {});
    QVariantMap runImportJob(qint64 id, const std::function<bool(const ImportProgress &)> &progress = {},
                             const QString &replacement = {}, int batchSize = 250, int workers = 1);
    QVariantList importJobs() const;
    QVariantMap importJob(qint64 id) const;
    void forgetImportJob(qint64 id);
    qint64 createAnalysisJob(const QString &pgn, const QVariantMap &config, bool variations, int total,
                             qint64 gameId = 0);
    // Parmi les `count` parties les plus récentes, celles qui restent à passer en revue :
    // non supprimées et pas déjà parcourues à une profondeur au moins égale.
    QVariantList recentGamesToReview(int count, int depth) const;
    // Les parties d'un joueur qui restent à passer en revue. Mêmes règles de
    // correspondance que reviewSummary : sans cela on analyserait un lot et le tableau
    // de bord en compterait un autre.
    QVariantList gamesToReviewFor(const QStringList &names, int depth, int count = 1000) const;
    // Bilan agrégé des parties d'un joueur. `names` sont les pseudos sous lesquels il
    // joue : un profil en rassemble plusieurs. Seules les parties déjà passées en revue
    // comptent, et chaque note est prise du côté où le joueur se trouvait.
    QVariantMap reviewSummary(const QStringList &names) const;
    // Même bilan, découpé par mois, du plus récent au plus ancien. Sert à comparer le
    // joueur à sa propre progression plutôt qu'à une référence extérieure.
    QVariantList reviewHistory(const QStringList &names, int months = 12) const;
    QVariantList analysisJobs() const;
    QVariantMap analysisJob(qint64 id, bool includePgn = true) const;
    QVariantList analysisResults(qint64 id, int offset = 0, int limit = 200) const;
    void setAnalysisStatus(qint64 id, const QString &status, const QString &error = QStringLiteral(""));
    void setAnalysisIdentity(qint64 id, const QVariantMap &identity);
    void recoverAnalyses();
    void forgetAnalysis(qint64 id);
    void saveAnalysisResult(qint64 id, int ordinal, int node, const QString &fen, const QVariantList &lines,
                            const QString &cacheKey = {}, bool cached = false);
    QVariantList cachedAnalysis(const QString &key) const;
    void clearAnalysisCache();
    qint64 createRepertoire(const QString &name, bool white, const Game &root,
                            const RepertoireProgress &progress = {}, qint64 model = 0);
    QVariantList repertoires() const;
    QVariantMap repertoire(qint64 id) const;
    void renameRepertoire(qint64 id, const QString &name);
    void deleteRepertoire(qint64 id);
    void addToRepertoire(qint64 id, const Game &game, qint64 model = 0,
                         const RepertoireProgress &progress = {});
    int importRepertoireFile(qint64 id, const QString &path, const RepertoireProgress &progress = {});
    // Le même import, depuis du PGN déjà en mémoire : la couche application déplie
    // elle-même les études compressées, la décompression ne vit pas dans cette couche.
    int importRepertoirePgn(qint64 id, const QByteArray &pgn, const RepertoireProgress &progress = {});
    QVariantMap repertoireOverview(qint64 id, const std::shared_ptr<std::atomic_bool> &canceled = {}) const;
    void importRepertoire(qint64 id, const std::vector<Game> &games);
    QVariantList repertoireMoves(qint64 id, const QString &fen) const;
    void editRepertoireMove(qint64 id, const QString &fen, const QString &uci, const QString &note,
                            bool principal);
    void removeRepertoireMove(qint64 id, const QString &fen, const QString &uci);
    Game exportRepertoire(qint64 id, const RepertoireProgress &progress = {}) const;
    QVariantMap nextRepertoirePosition(qint64 id, bool includeFuture = false) const;
    void reviewRepertoire(qint64 id, const QString &fen, bool success);
    QVariantList repertoireWeaknesses(qint64 id) const;
    QVariantList repertoireGaps(qint64 id) const;
    // Deux répertoires position par position : combien de positions chacun a en propre,
    // combien ils partagent, et celles où ils ne préparent pas les mêmes coups.
    QVariantMap compareRepertoires(qint64 first, qint64 second) const;
    // L'écart entre un répertoire et les parties réellement jouées : combien de parties y
    // sont entrées, dans combien le joueur a quitté sa propre préparation, et où. Personne
    // d'autre ne peut le dire : il faut le répertoire et le corpus du joueur au même endroit.
    QVariantMap repertoireGap(qint64 repertoire, const QStringList &names, int limit = 50,
                              const std::shared_ptr<std::atomic_bool> &canceled = {}) const;
    void exportSource(QIODevice &output, qint64 source) const;
    Game load(qint64 id) const;
    QVariantList search(const QString &query = {}, const QString &result = {}, bool favorite = false,
                        const QString &position = {}, bool trash = false, const QVariantMap &filters = {},
                        const std::shared_ptr<std::atomic_bool> &canceled = {}) const;
    QVariantMap stats() const;
    QVariantList explorer(const QString &position) const;
    // Dossier d'une position : ce que la bibliothèque sait de ce qui s'y joue. Les coups
    // avec leur score, le même découpage par force des joueurs — un coup qui réussit chez
    // les amateurs et disparaît au-dessus de 2300 ne se voit pas autrement — et les
    // parties les mieux classées qui y sont passées.
    QVariantMap openingReport(const QString &fen, int models = 6,
                              const std::shared_ptr<std::atomic_bool> &canceled = {}) const;
    QVariantList positionStatistics(const QString &fen, const QVariantMap &filters = {},
                                    const std::shared_ptr<std::atomic_bool> &canceled = {}) const;
    void favorite(qint64 id, bool value);
    void trash(qint64 id, bool value);
    void collection(qint64 id, const QString &value);
    // Étiquette chaque partie avec le nom de famille de son ouverture, d'après la table
    // ECO fournie par l'appelant. Renvoie le nombre de parties rangées.
    int groupByOpening(const QVariantMap &labels, const RepertoireProgress &progress = {});
    // Donne son code ECO à chaque partie qui n'en a pas. `ecoByPosition` associe la clé
    // de position (son empreinte Zobrist, en texte) au code : la table vient de
    // l'appelant, qui seul a la ressource. Renvoie le nombre de parties classées.
    int classifyOpenings(const QVariantMap &ecoByPosition, const RepertoireProgress &progress = {});
    QString exportPgn() const;
    void exportPgn(QIODevice &output) const;
    void validateOutputPath(const QString &path) const;
    void backup(const QString &path);
    QString integrity(const std::shared_ptr<std::atomic_bool> &canceled = {}) const;
    // Puzzle bank: insert ignores references already present; returns the number added.
    int addPuzzles(const std::vector<Puzzle> &puzzles);
    // An unsolved puzzle near `rating` (± spread), optionally of a theme and a source.
    // Un puzzle manqué revient : le lendemain, puis trois et sept jours après chaque
    // réussite. Trois réussites de suite, et il est tenu pour acquis. Rend le plus ancien
    // puzzle dû, ou rien. `now` ne sert qu'aux tests.
    QVariantMap duePuzzle(const QString &theme = {}, const QString &source = {},
                          const QDateTime &now = QDateTime::currentDateTimeUtc()) const;
    int duePuzzles(const QDateTime &now = QDateTime::currentDateTimeUtc()) const;
    QVariantMap nextPuzzle(int rating, int spread = 150, const QString &theme = {},
                           const QString &source = {}) const;
    void recordPuzzle(qint64 id, bool success, double seconds, int ratingAfter);
    QVariantMap puzzleStats() const;
    // First main-line move no other game of the library played from that position.
    // `self` is the game's own library id (its moves are not counted), 0 if unsaved.
    QVariantMap novelty(const Game &game, qint64 self = 0) const;
    // Writes the library's opening statistics as a Polyglot book, weighted by games played,
    // keeping moves played at least `minGames` times. Returns the number of entries.
    int exportPolyglot(const QString &path, int minGames = 2) const;
    void addCard(const QString &title, const QString &fen, const QString &moves);
    QVariantList cards() const;
    void review(qint64 id, bool success);
    void deleteCard(qint64 id);
    QVariantList players() const;
    QVariantMap player(const QString &name) const;
    QVariantList duplicates() const;
    int rebuildIndexes(const std::function<bool(int)> &progress = {});
    QString path() const { return path_; }
    // Demi-coups de la ligne principale dont chaque position est indexée. Au-delà, une
    // partie se retrouve par son matériel et ses pions, puis en étant rejouée.
    static constexpr int indexedPlies = 40;
    // Cache de pages d'une connexion ordinaire. Celui de SQLite, 2 Mo, fait relire le
    // disque à chaque recherche dès que la bibliothèque compte quelques milliers de parties.
    static constexpr int defaultCacheKib = 32 * 1024;
    static qint64 materialCode(std::string_view fen);
    static qint64 materialCode(const QString &pieces);
    // Placement partiel : « Ke1 Pd4 pd5 » — une lettre de pièce (majuscule pour les
    // Blancs, comme dans une FEN) et sa case. Rend, par case de 0 (a8) à 63 (h1) dans
    // l'ordre d'une FEN, la pièce exigée ou 0 ; lève une erreur si le texte ne se lit pas.
    static std::array<char, 64> parsePlacement(const QString &text);
    static bool placementMatches(const std::array<char, 64> &wanted, std::string_view fen);

  private:
    bool enhanced_ = false, fts_ = false;
    bool keyed_ = false; // schema 8: positions indexed by Zobrist key, not FEN text
    void upgradeV8(int originalVersion);
    void createPuzzleTables();
    static qint64 hashKey(const std::string &fen);
    static qint64 pawnsKey(const std::string &fen);
    static qint64 nodeKey(const Node &node) { return node.hash ? qint64(node.hash) : hashKey(node.fen); }
    void upgradeV3(int originalVersion);
    void upgradeV4(int originalVersion);
    void upgradeV5(int originalVersion);
    void upgradeV6(int originalVersion);
    void upgradeV7(int originalVersion);
    void upgradeV9(int originalVersion);
    void upgradeV10(int originalVersion);
    void upgradeV11(int originalVersion);
    void upgradeV12(int originalVersion);
    // Schéma 12 : index allégé. Voir lightindex.cpp.
    bool light_ = false;
    void saveLegacyIndex(const Game &game, qint64 id);
    void indexGame(const Game &game, qint64 id, bool existing);
    void retireOpeningStats(qint64 id);
    void creditOpeningStats(qint64 id);
    bool collectPositionMatches(const QString &fen, const std::shared_ptr<std::atomic_bool> &canceled) const;
    // Aucun index ne répond à « ces pièces sur ces cases » : les parties sont rejouées
    // toutes, une fois par motif, et la liste est gardée tant que la base ne change pas.
    void collectPlacementMatches(const QString &pattern,
                                 const std::shared_ptr<std::atomic_bool> &canceled) const;
    static constexpr int sparseMatches = 20000;
    QSqlQuery &statement(const QString &text) const;
    mutable std::map<QString, QSqlQuery> statements_;
    mutable bool lateValid_ = false, lateSparse_ = false;
    mutable qint64 lateKey_ = 0, lateState_ = 0;
    // Calcule et enregistre le bilan chiffré d'une analyse menée à son terme.
    void recordGameReview(qint64 job, qint64 game, int depth);
    qint64 registerSource(const QVariantMap &source);
    void linkSource(qint64 source, qint64 game);
    void recordImport(qint64 source, const QString &file, const QString &digest, int processed, int added);
    QString connection_, path_;
    QSqlDatabase db_;
    void sql(const QString &query) const;
};
} // namespace t64
