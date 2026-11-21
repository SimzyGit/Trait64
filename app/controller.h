// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "broadcasts.h"
#include "comparison.h"
#include "content.h"
#include "database.h"
#include "explorer.h"
#include "fide.h"
#include "lichess_login.h"
#include "online.h"
#include "polyglot.h"
#include "queue.h"
#include "syzygy_download.h"
#include "tournament.h"
#include "uci.h"
#include "ui_translator.h"
#include "updater.h"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QQmlPropertyMap>
#include <QSettings>
#include <QTimer>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>

class Controller : public QObject {
    Q_OBJECT
    Q_PROPERTY(t64::Content *content READ content CONSTANT)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantList importHistory READ importHistory NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantList importJobs READ importJobs NOTIFY importsChanged)
    Q_PROPERTY(QVariantMap importProgress READ importProgress NOTIFY busyChanged)
    Q_PROPERTY(bool resumableImport READ resumableImport NOTIFY busyChanged)
    Q_PROPERTY(QVariantList repertoires READ repertoires NOTIFY repertoiresChanged)
    Q_PROPERTY(QVariantMap repertoireState READ repertoireState NOTIFY repertoiresChanged)
    Q_PROPERTY(QVariantMap repertoireTask READ repertoireTask NOTIFY repertoireTaskChanged)
    Q_PROPERTY(QVariantMap preparation READ preparation NOTIFY preparationChanged)
    Q_PROPERTY(bool librarySearching READ librarySearching NOTIFY searchChanged)
    Q_PROPERTY(bool libraryHasNext READ libraryHasNext NOTIFY searchChanged)
    Q_PROPERTY(int libraryPage READ libraryPage NOTIFY searchChanged)
    Q_PROPERTY(QString librarySearchStatus READ librarySearchStatus NOTIFY searchChanged)
    Q_PROPERTY(t64::Updater *updater READ updater CONSTANT)
    Q_PROPERTY(t64::Comparison *comparison READ comparison CONSTANT)
    Q_PROPERTY(t64::Tournament *tournament READ tournament CONSTANT)
    Q_PROPERTY(t64::AnalysisQueue *analysisQueue READ analysisQueue CONSTANT)
    // L'état par clé : une liaison QML sur « state.fen » n'est réévaluée que si fen
    // change. Exposé d'un bloc, chaque coup — et chaque tic de pendule — faisait relire
    // toute la carte à des centaines de liaisons : trente millisecondes par coup.
    Q_PROPERTY(QQmlPropertyMap *state READ stateObject CONSTANT)
    Q_PROPERTY(QQmlPropertyMap *cells READ cells CONSTANT)
    Q_PROPERTY(QVariantList squares READ squares NOTIFY squaresChanged)
    Q_PROPERTY(QVariantList notation READ notation NOTIFY notationChanged)
    Q_PROPERTY(QVariantList moveRows READ moveRows NOTIFY notationChanged)
    Q_PROPERTY(QVariantList moveFlow READ moveFlow NOTIFY notationChanged)
    Q_PROPERTY(QVariantList games READ games NOTIFY libraryChanged)
    // Nombre de parties ajoutées par la dernière synchronisation ou le dernier import :
    // c'est le lot que le bouton de bilan propose de passer en revue. 0 : rien à proposer.
    Q_PROPERTY(int importedGames READ importedGames NOTIFY libraryChanged)
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList openings READ openings NOTIFY openingsChanged)
    // Dossier de la position courante : coups joués, scores, tranches de force et
    // parties modèles. Construit en arrière-plan, comme la préparation.
    Q_PROPERTY(QVariantMap openingReport READ openingReport NOTIFY reportChanged)
    Q_PROPERTY(QVariantMap onlineReport READ onlineReport NOTIFY onlineReportChanged)
    // Le téléchargement des tables Syzygy : running, pack, done, total, received, error.
    Q_PROPERTY(QVariantMap syzygyDownload READ syzygyDownload NOTIFY syzygyDownloadChanged)
    Q_PROPERTY(QVariantList bookMoves READ bookMoves NOTIFY bookMovesChanged)
    Q_PROPERTY(QVariantMap puzzles READ puzzles NOTIFY puzzlesChanged)
    Q_PROPERTY(QVariantList cards READ cards NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList engineOptions READ engineOptions NOTIFY engineChanged)
    Q_PROPERTY(QVariantList engineLines READ engineLines NOTIFY engineChanged)
    // La dernière évaluation reçue, gardée pendant que le moteur change de position : il
    // vide ses lignes à chaque nouvelle recherche, et la barre retombait à l'équilibre le
    // temps que la première évaluation du coup joué revienne.
    Q_PROPERTY(QVariantMap heldEvaluation READ heldEvaluation NOTIFY engineChanged)
    Q_PROPERTY(QString engineStatus READ engineStatus NOTIFY engineChanged)
    Q_PROPERTY(QVariantMap preferences READ preferences NOTIFY preferencesChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool onlineFetching READ onlineFetching NOTIFY busyChanged)
    Q_PROPERTY(QVariantList onlineAccounts READ onlineAccounts NOTIFY accountsChanged)
    // Recherches enregistrées : [{name, text, result, favorite, position, trash, filters}].
    Q_PROPERTY(QVariantList savedSearches READ savedSearches NOTIFY savedSearchesChanged)
    // Profils de joueur : un utilisateur rassemble ses comptes en ligne et son profil
    // FIDE sous un seul nom. Chaque profil porte la liste des comptes qui lui sont liés.
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
    // Téléchargement d'une liste FIDE en cours : {busy, speed, index, count, received,
    // total, status}. Les listes pèsent quelques dizaines de mégaoctets chacune.
    Q_PROPERTY(QVariantMap fideSync READ fideSync NOTIFY fideSyncChanged)
    Q_PROPERTY(int taskProgress READ taskProgress NOTIFY busyChanged)
    Q_PROPERTY(QString taskStatus READ taskStatus NOTIFY busyChanged)
    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)
    Q_PROPERTY(QVariantList review READ review NOTIFY reviewChanged)
    // Le point coloré par coup, rangé par numéro de nœud : le carnet de coups a des
    // centaines de lignes, et chacune parcourant la liste du bilan, l'affichage
    // deviendrait quadratique.
    Q_PROPERTY(QVariantMap reviewQuality READ reviewQuality NOTIFY reviewChanged)
    Q_PROPERTY(QVariantMap reviewState READ reviewState NOTIFY reviewChanged)
    // Précision par camp et par phase, à la manière de Lichess. Chaque valeur vaut -1
    // quand elle n'est pas mesurable, faute de coups évalués des deux côtés.
    Q_PROPERTY(QVariantMap reviewAccuracy READ reviewAccuracy NOTIFY reviewChanged)
    Q_PROPERTY(QVariantList players READ players NOTIFY libraryChanged)
    Q_PROPERTY(QVariantMap playerProfile READ playerProfile NOTIFY libraryChanged)
  public:
    explicit Controller(const QString &dataPath, QObject *parent = nullptr);
    ~Controller() override;
    t64::Content *content() const { return content_.get(); }
    QVariantList sources() const { return sources_; }
    QVariantList importHistory() const { return importHistory_; }
    QVariantList importJobs() const { return importJobs_; }
    QVariantMap importProgress() const;
    bool resumableImport() const { return busy_ && resumableImport_; }
    Q_INVOKABLE void importResumable(const QString &url);
    Q_INVOKABLE void resumeImport(qint64 id, const QString &replacement = {});
    Q_INVOKABLE void forgetImport(qint64 id);
    QVariantList repertoires() const { return repertoires_; }
    QVariantMap repertoireState() const { return repertoireState_; }
    QVariantMap repertoireTask() const;
    Q_INVOKABLE void cancelRepertoireOverview();
    Q_INVOKABLE void reloadRepertoires();
    Q_INVOKABLE void analyseRepertoire(qint64 id, int milliseconds = 500, int profile = -1);
    Q_INVOKABLE void selectRepertoire(qint64 id);
    Q_INVOKABLE void createRepertoire(const QString &name, bool white);
    Q_INVOKABLE void renameRepertoire(qint64 id, const QString &name);
    Q_INVOKABLE void deleteRepertoire(qint64 id);
    Q_INVOKABLE void addStudyToRepertoire(qint64 id);
    // `id` à 0 : le fichier crée une étude nommée d'après lui, plutôt que d'exiger
    // d'en avoir choisi une au préalable.
    // Un recueil : les parties du fichier restent séparées et parcourables, au lieu
    // d'être fondues dans un arbre de positions comme un répertoire.
    Q_INVOKABLE void importStudyCollection(const QString &url);
    Q_INVOKABLE void importStudyFolder(const QString &url);
    // L'explorateur de l'onglet Études : un dossier du disque, ses sous-dossiers et ses
    // fichiers de parties, avec la marque de ceux qui sont déjà dans la base.
    Q_INVOKABLE QVariantMap browseStudies(const QString &path = {}) const;
    // Le recueil du fichier, importé au premier clic. 0 si l'import vient de commencer
    // ou si le fichier n'est pas lisible.
    Q_INVOKABLE qint64 openStudyFile(const QString &url);
    Q_INVOKABLE void createStudyFolder(const QString &parent, const QString &name);
    // L'arborescence du dossier d'études, à plat : une ligne par entrée visible, avec sa
    // profondeur. `expanded` liste les dossiers ouverts ; `search` filtre et ouvre tout.
    Q_INVOKABLE QVariantList studyTree(const QString &root, const QStringList &expanded,
                                       const QString &search = {}) const;
    Q_INVOKABLE QStringList studyExpanded() const;
    Q_INVOKABLE void rememberStudyExpanded(const QStringList &folders);
    // Chacune rend le nouveau chemin, vide en cas de refus.
    Q_INVOKABLE QString createStudyFile(const QString &folder, const QString &name, const QString &pgn = {});
    Q_INVOKABLE QString renameStudyEntry(const QString &entry, const QString &name);
    Q_INVOKABLE QString moveStudyEntry(const QString &entry, const QString &folder);
    // À la corbeille du système, d'où l'on peut encore le reprendre.
    Q_INVOKABLE bool trashStudyEntry(const QString &entry);
    void nextPendingStudy();
    static QString studyCollectionName(const QString &path);
    // Les recueils, et les parties de l'un d'eux. L'onglet Études les montre comme des
    // chapitres : un clic en ouvre une dans Analyse.
    Q_PROPERTY(QVariantList studyCollections READ studyCollections NOTIFY sourcesChanged)
    QVariantList studyCollections() const;
    Q_INVOKABLE QVariantList studyGames(qint64 sourceId) const;
    // Un aperçu : les cases d'une position quelconque, et la ligne principale d'une
    // partie. Ni l'un ni l'autre ne touche à l'étude ouverte.
    Q_INVOKABLE QVariantList squaresForFen(const QString &fen, const QString &lastUci = {}) const;
    Q_INVOKABLE QVariantList gameLine(qint64 id) const;
    // delta = +1 ou -1 : la partie suivante ou précédente de la liste affichée par
    // la bibliothèque, chargée dans l’onglet courant.
    Q_INVOKABLE void stepGame(int delta);
    Q_INVOKABLE void importRepertoire(qint64 id, const QString &url, bool white = true);
    // Un dossier d'études, tel que l'exportent En Croissant et Pawn Appétit : un
    // répertoire par fichier PGN, sous-dossiers compris, nommés d'après leur chemin.
    Q_INVOKABLE void importRepertoireFolder(const QString &url, bool white = true);
    Q_INVOKABLE void exportRepertoire(qint64 id, const QString &url);
    Q_INVOKABLE void editRepertoireMove(qint64 id, const QString &fen, const QString &uci,
                                        const QString &note, bool principal);
    Q_INVOKABLE void removeRepertoireMove(qint64 id, const QString &fen, const QString &uci);
    Q_INVOKABLE void openRepertoirePosition(qint64 id, const QString &fen);
    Q_INVOKABLE QVariantMap compareRepertoires(qint64 first, qint64 second);
    Q_INVOKABLE void startRepertoireTraining(qint64 id, bool includeFuture = false);
    QVariantMap preparation() const { return preparation_; }
    Q_INVOKABLE void preparePosition(const QVariantMap &filters);
    Q_INVOKABLE void cancelPreparation();
    Q_INVOKABLE void playPreparedMove(const QString &uci);
    Q_INVOKABLE void exportPreparation(const QString &url);
    bool librarySearching() const { return librarySearching_; }
    bool libraryHasNext() const { return libraryHasNext_; }
    int libraryPage() const { return int(searchCursors_.size()) + 1; }
    QString librarySearchStatus() const { return librarySearchStatus_; }
    Q_INVOKABLE void cancelLibrarySearch();
    Q_INVOKABLE void nextLibraryPage();
    Q_INVOKABLE void previousLibraryPage();
    Q_INVOKABLE void exportSource(const QString &url, qint64 source);
    Q_INVOKABLE void searchLibrary(const QString &query, const QString &result, bool favorite, bool position,
                                   bool trash, const QVariantMap &filters);
    QVariantMap state() const { return state_; }
    QQmlPropertyMap *stateObject() const { return stateObject_; }
    QVariantList squares() const { return squares_; }
    QVariantList notation() const { return notation_; }
    QVariantList moveRows() const { return moveRows_; }
    QVariantList moveFlow() const { return moveFlow_; }
    QQmlPropertyMap *cells() const { return cells_; }
    QVariantList games() const { return games_; }
    int importedGames() const { return importedGames_; }
    QVariantMap stats() const { return stats_; }
    QVariantList openings() const { return openings_; }
    QVariantList bookMoves() const { return bookMoves_; }
    QVariantMap puzzles() const;
    // Puzzles: theme is a Lichess theme tag ("fork", "mateIn2"…), source "lichess", "mine" or "".
    Q_INVOKABLE void startPuzzle(const QString &theme = {}, const QString &source = {});
    Q_INVOKABLE void nextPuzzle();
    Q_INVOKABLE void puzzleHint();
    Q_INVOKABLE void importPuzzles(const QString &url, int max, int minRating, int maxRating);
    Q_INVOKABLE void createMistakePuzzles();
    // Des mats en un, deux ou trois coups tirés des fins de parties de la bibliothèque.
    Q_INVOKABLE void harvestLibraryMates();
    QVariantList cards() const { return cards_; }
    QVariantList engineOptions() const {
        return settings_.value("enginePath").toString() == engine_.executable() ? engine_.options()
                                                                                : QVariantList();
    }
    Q_INVOKABLE void setEngineOption(const QString &name, const QVariant &value);
    Q_INVOKABLE void pressEngineOption(const QString &name);
    QVariantList engineLines() const { return engineLines_; }
    QVariantMap heldEvaluation() const { return heldEval_; }
    // Les lignes du moteur et la dernière évaluation gardée vont ensemble : les oublier
    // séparément laisserait la barre afficher une valeur d'une autre position.
    void forgetEngineLines();
    QString engineStatus() const { return engineStatus_; }
    QVariantMap preferences() const;
    bool busy() const { return busy_ || (online_ && online_->busy()); }
    int taskProgress() const { return taskProgress_->load(); }
    QString taskStatus() const {
        return verifying_ ? QString("Vérification de la bibliothèque…")
                          : QString("Traitement en cours · %1 parties lues").arg(taskProgress());
    }
    Q_INVOKABLE void cancelTask() { cancelTask_->store(true); }
    t64::Updater *updater() const { return updater_.get(); }
    t64::Comparison *comparison() { return &comparison_; }
    t64::Tournament *tournament() { return &tournament_; }
    // Round-robin between saved engine profiles; finished games go to the library.
    Q_INVOKABLE void startTournament(const QVariantList &indices, int gamesPerPairing, int moveTime,
                                     int maxPlies, bool bookOpenings);
    t64::AnalysisQueue *analysisQueue() { return &analysisQueue_; }
    Q_INVOKABLE void enqueueAnalysis(bool variations = true, int milliseconds = 500, int profile = -1);
    // Every game matching the current library filters (main lines), up to `maxGames`.
    Q_INVOKABLE void enqueueLibrary(int milliseconds = 500, int profile = -1, int maxGames = 200);
    // Bilan du lot que la dernière synchronisation ou le dernier import vient d'ajouter.
    // Les parties déjà parcourues au moins aussi loin sont laissées de côté.
    Q_INVOKABLE void reviewImported(int depth, int profile = -1);
    Q_INVOKABLE int reviewTimeCeiling(int depth) const;
    // Jumps to the first move of the study that no other library game played.
    Q_INVOKABLE void findNovelty();
    Q_INVOKABLE void compareEngines(const QVariantList &indices);
    Q_INVOKABLE void addComparisonLine(int engine, int variation);
    void startBackgroundServices();
    Q_INVOKABLE void automaticBackup(bool force = true);
    bool smokeTest(QString *error);
    QVariantList tabs() const;
    QVariantList review() const { return review_; }
    QVariantList profiles() const;
    Q_INVOKABLE void addProfile(const QString &name);
    Q_INVOKABLE void renameProfile(int index, const QString &name);
    Q_INVOKABLE void removeProfile(int index);
    // profileName vide détache le compte sans rien supprimer.
    Q_INVOKABLE void setAccountProfile(int accountIndex, const QString &profileName);
    // Ajoute un compte en ligne à un profil, autant qu'on veut : un compte déjà connu est
    // simplement rattaché, un nouveau est enregistré puis synchronisé.
    Q_INVOKABLE void addProfileAccount(int profileIndex, const QString &site, const QString &user);
    // Un classement à 0 se lit « pas classé dans cette cadence », ce qui est courant.
    // Range une copie réduite de l'image dans le dossier de données : le profil ne
    // dépend pas d'un fichier que l'on peut déplacer ou effacer. Une URL vide efface.
    Q_INVOKABLE void setProfilePhoto(int index, const QString &url);
    Q_INVOKABLE void setFideDetails(int index, const QString &fideId, const QString &fideName, int standard,
                                    int rapid, int blitz, const QString &title = {},
                                    const QString &federation = {});
    // Télécharge les listes publiées et reporte les classements sur le profil. `speeds`
    // ne contient que les cadences voulues : chacune est un téléchargement séparé.
    Q_INVOKABLE void fetchFideRatings(int index, const QStringList &speeds);
    Q_INVOKABLE void cancelFideRatings();
    QVariantMap fideSync() const { return fideSync_; }
    // Tableau de bord d'un profil : notes d'ensemble et par phase, fautes, tactique, et
    // l'évolution mois par mois. Un profil sans compte rattaché ne mesure rien.
    Q_INVOKABLE QVariantMap skillDashboard(int profileIndex) const;
    // L'écart entre un répertoire et les parties du profil : où le joueur quitte sa
    // propre préparation. Il faut les deux au même endroit pour pouvoir le dire.
    Q_INVOKABLE QVariantMap repertoireGap(qint64 repertoireId, int profileIndex) const;
    // Passe en revue les parties synchronisées de ce profil qui n'ont pas encore de
    // bilan, afin d'alimenter son tableau de bord.
    Q_INVOKABLE void reviewProfile(int profileIndex, int depth, int enginePreset = -1);
    // Traduit un nom d'ouverture en codes ECO. Fonction pure, exposée pour que ses deux
    // règles — une famille en début de nom, un fragment de variante n'importe où —
    // soient vérifiables sans monter une recherche complète.
    Q_INVOKABLE QVariantMap openingFilters(const QVariantMap &filters) const;
    // Reporte sur un profil ce qu'une recherche a rapporté. `requested` nomme les
    // cadences effectivement cherchées : celles-là seules peuvent devenir douteuses.
    // Tout passe par les arguments, ce qui la rend vérifiable sans réseau.
    Q_INVOKABLE void applyFideRatings(int index, const QStringList &requested, const QVariantMap &player);
    QVariantMap reviewState() const;
    QVariantMap reviewAccuracy() const { return reviewAccuracy_; }
    QVariantMap reviewQuality() const { return reviewQuality_; }
    QVariantList players() const { return players_; }
    QVariantMap playerProfile() const { return playerProfile_; }
    Q_INVOKABLE void switchTab(int index);
    Q_INVOKABLE void closeTab(int index);
    Q_INVOKABLE void duplicateStudy();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE QVariantList boardPresets() const;
    Q_INVOKABLE void applyBoardPreset(const QString &name);
    Q_INVOKABLE void resetAppearance();
    Q_INVOKABLE void importPieceSet(const QString &folder);
    void adoptLoneCustomPieceSet();
    Q_INVOKABLE void useCustomPieceSet(int index);
    Q_INVOKABLE void removeCustomPieceSet(int index);
    Q_INVOKABLE void setPosition(const QString &fen, bool chess960);
    Q_INVOKABLE void newChess960(int number);
    Q_INVOKABLE void toggleAnnotation(const QString &from, const QString &to, const QString &color = "G");
    Q_INVOKABLE void clearAnnotations();
    Q_INVOKABLE void mergeGame(qint64 id);
    // Two passes by default: a quick one from the first move to the last, then a full one from
    // the last move back to the first, the engine keeping its hash table between positions.
    // depth > 0 : la profondeur décide, milliseconds n'est plus qu'un plafond par position.
    Q_INVOKABLE void analyseGame(int milliseconds = 500, bool twoPasses = true, int depth = 0);
    Q_INVOKABLE void cancelReview();
    Q_INVOKABLE void annotateReview();
    Q_INVOKABLE QVariantMap explainReview(int index, bool alternative = false) const;
    Q_INVOKABLE bool openReviewLine(int index, bool alternative = false);
    Q_INVOKABLE void createMistakeCards();
    Q_INVOKABLE void exportStudyPdf(const QString &url);
    Q_INVOKABLE void exportDiagram(const QString &url);
    Q_INVOKABLE void exportPolyglot(const QString &url, int minGames);
    QByteArray diagramSvg() const;
    Q_INVOKABLE void identifyOpening();
    Q_INVOKABLE void searchAdvanced(const QVariantMap &filters);
    Q_INVOKABLE void inspectPlayer(const QString &name);
    Q_INVOKABLE void rebuildIndexes();
    Q_INVOKABLE void findDuplicates();
    // Les parties de la page affichée (au plus 200, celles d'une recherche) fondues en une
    // étude avec variantes, dans un nouvel onglet.
    Q_INVOKABLE void mergeShownGames();
    // Les repères du bilan sur la position d'après un coup fautif : coup joué (rouge),
    // meilleur coup (vert), réfutation du moteur (orange), et une phrase qui les nomme.
    // Vide si le coup n'est pas une faute ou si le bilan porte sur une autre partie.
    Q_INVOKABLE QVariantMap reviewMarks(int node) const;
    Q_INVOKABLE void nextDueCard();
    Q_INVOKABLE void saveEngineProfile(const QString &name);
    Q_INVOKABLE void loadEngineProfile(int index);
    Q_INVOKABLE void removeEngineProfile(int index);
    Q_INVOKABLE void startLocalGame(int minutes, int increment, int opponent, bool humanWhite);
    Q_INVOKABLE void resignGame();
    Q_INVOKABLE void claimDraw();
    Q_INVOKABLE void newGame();
    Q_INVOKABLE void loadGame(qint64 id);
    Q_INVOKABLE void saveGame(bool copy = false);
    Q_INVOKABLE void setFen(const QString &fen);
    Q_INVOKABLE void importText(const QString &text);
    Q_INVOKABLE void importFile(const QString &url);
    // Un PGN importé sous une étiquette : elle devient la collection de ses parties et un
    // recueil de la bibliothèque. `study` : des études ou des cours, dont les noms
    // n'entrent pas dans l'onglet Joueurs.
    Q_INVOKABLE void importLabeled(const QString &url, const QString &label, bool study);
    // Un recueil compté comme études (true) ou comme parties jouées.
    Q_INVOKABLE void setStudyPurpose(qint64 source, bool study);
    // Retire un recueil : ses parties vont à la corbeille, le fichier reste sur le disque.
    Q_INVOKABLE void removeStudyCollection(qint64 source);
    // Supprime une base de la bibliothèque (un PGN importé, une base ChessBase ou SCID, un
    // compte en ligne, un téléchargement) : ses parties vont à la corbeille, sauf celles
    // qu'une autre base contient aussi.
    Q_INVOKABLE void removeBase(qint64 source);
    // site: "lichess" or "chesscom". Lichess re-imports only games newer than the last import.
    Q_INVOKABLE void importOnline(const QString &site, const QString &user, int max);
    Q_INVOKABLE void cancelOnline();
    // Saved accounts ("lichess" / "chesscom"); adding one syncs it at once.
    QVariantList onlineAccounts() const;
    Q_INVOKABLE void addOnlineAccount(const QString &site, const QString &user);
    Q_INVOKABLE void removeOnlineAccount(int index);
    Q_INVOKABLE void syncOnlineAccount(int index);
    // Jeton personnel Lichess pour l'explorateur d'ouvertures (vide : le retirer). Il ne
    // repart jamais vers l'interface : on dit seulement s'il y en a un.
    Q_INVOKABLE void setLichessToken(const QString &token);
    Q_INVOKABLE bool hasLichessToken() const;
    // Lie un jeton collé à un compte Lichess déjà enregistré (vide : le délier).
    Q_INVOKABLE void setAccountToken(int index, const QString &token);
    // Se connecter à Lichess dans le navigateur, comme En Croissant : le compte connecté
    // est ajouté s'il n'existe pas, et reçoit son jeton s'il existe.
    Q_INVOKABLE void connectLichess();
    Q_INVOKABLE void cancelLichessLogin();
    Q_INVOKABLE bool lichessLoginRunning() const { return lichessLogin_ && lichessLogin_->running(); }
    // Fait entendre un jeu de sons (coup, prise, échec), pour le choisir à l'oreille.
    Q_INVOKABLE void previewSound(const QString &theme);
    Q_INVOKABLE void syncAllAccounts();
    bool onlineFetching() const { return online_->busy(); }
    Q_INVOKABLE void exportFile(const QString &url, bool all = false);
    Q_INVOKABLE void backup(const QString &url);
    Q_INVOKABLE void openBackup(const QString &url);
    // Les sauvegardes que Trait64 a faites lui-même, de la plus récente à la plus ancienne :
    // automatiques, avant une mise à jour, avant une restauration, avant une migration.
    Q_INVOKABLE QVariantList backups() const;
    // Remplace la bibliothèque par une sauvegarde de la liste. La bibliothèque actuelle est
    // d'abord sauvegardée à son tour : une restauration se défait comme elle se fait.
    Q_INVOKABLE void restoreBackup(const QString &file);
    Q_INVOKABLE void clickSquare(const QString &square);
    Q_INVOKABLE void tryMove(const QString &from, const QString &to);
    Q_INVOKABLE void move(const QString &uci);
    // Saisie au clavier : les coups qui répondent à ce qui est tapé, en notation affichée,
    // et le coup joué quand un seul y répond.
    Q_INVOKABLE QVariantList typedMoves(const QString &text) const;
    Q_INVOKABLE bool playTyped(const QString &text);
    // Tableau croisé du tournoi de cette partie ; vide, avec un message, s'il ne se fait pas.
    Q_INVOKABLE QVariantMap tournamentTable(qint64 gameId);
    // Correction des noms : les groupes d'écritures arrivent par nameVariantsChanged.
    Q_PROPERTY(QVariantMap nameVariants READ nameVariants NOTIFY nameVariantsChanged)
    QVariantMap nameVariants() const { return nameVariants_; }
    Q_INVOKABLE void findNameVariants(const QString &field);
    // Recherche de tournois : {text, running, rows}.
    Q_PROPERTY(QVariantMap tournamentList READ tournamentList NOTIFY tournamentListChanged)
    QVariantMap tournamentList() const { return tournamentList_; }
    Q_INVOKABLE void findTournaments(const QString &text);
    // La sélection de la bibliothèque en chiffres : parties par année, résultats, Elo.
    Q_PROPERTY(QVariantMap librarySummary READ librarySummary NOTIFY librarySummaryChanged)
    QVariantMap librarySummary() const { return librarySummary_; }
    Q_INVOKABLE void summarizeLibrary();
    QVariantList savedSearches() const { return settings_.value("savedSearches").toList(); }
    // Un nom déjà pris remplace la recherche du même nom.
    Q_INVOKABLE void saveSearch(const QString &name, const QVariantMap &search);
    Q_INVOKABLE void removeSearch(int index);
    // Retransmissions en direct (Lichess) : list, round, following, error, loading.
    Q_PROPERTY(QVariantMap live READ live NOTIFY liveChanged)
    QVariantMap live() const { return live_; }
    Q_INVOKABLE void liveList();
    Q_INVOKABLE void liveOpenRound(const QVariantMap &entry);
    // Ouvre la partie dans un onglet et la tient à jour tant qu'on la suit.
    Q_INVOKABLE void liveFollow(int gameIndex);
    // Le panneau se ferme : on cesse d'interroger Lichess, sauf pour la partie suivie.
    Q_INVOKABLE void liveClose();
    Q_INVOKABLE void liveStopFollowing();
    // Affiche une ronde reçue de Lichess et lance l'évaluation de ses échiquiers.
    void liveShowRound(const QVariantMap &round);
    Q_INVOKABLE void unifyNames(const QString &field, const QStringList &spellings, const QString &kept);
    Q_INVOKABLE void navigate(int node);
    Q_INVOKABLE void step(int direction);
    Q_INVOKABLE void stepVariation(int delta);
    Q_INVOKABLE void flip();
    Q_INVOKABLE void setComment(const QString &comment);
    Q_INVOKABLE void setNag(int nag);
    Q_INVOKABLE void setTags(const QVariantMap &tags);
    Q_INVOKABLE void promote();
    Q_INVOKABLE void deleteVariation();
    Q_INVOKABLE void search(const QString &query, const QString &result, bool favorite, bool position,
                            bool trash);
    Q_INVOKABLE void favorite(qint64 id, bool value);
    Q_INVOKABLE void setMedals(qint64 id, int medals);
    Q_INVOKABLE void trash(qint64 id, bool value);
    Q_INVOKABLE void setCollection(qint64 id, const QString &collection);
    // Range toute la bibliothèque en collections par famille d'ouverture, sans rien
    // télécharger : la table ECO est embarquée.
    // Donne son code ECO à chaque partie qui n'en porte pas : la plupart des PGN n'en
    // portent aucun, et la recherche par ouverture restait alors vide en silence.
    Q_INVOKABLE void buildOpeningReport();
    QVariantMap openingReport() const { return report_; }
    // Le même dossier, demandé à Lichess : « lichess » pour les parties classées du site,
    // « masters » pour celles jouées sur l'échiquier. Rien n'est téléchargé.
    Q_INVOKABLE void buildOnlineReport(const QString &source, const QVariantMap &filters = {});
    Q_INVOKABLE void cancelOnlineReport();
    QVariantMap onlineReport() const { return onlineReport_; }
    QVariantMap syzygyDownload() const { return syzygyState_; }
    // « 3-4-5 » ou « 6 » : taille du paquet, place libre là où il irait, déjà présent.
    Q_INVOKABLE QVariantMap syzygyPack(const QString &pack) const;
    Q_INVOKABLE void downloadSyzygy(const QString &pack);
    Q_INVOKABLE void cancelSyzygyDownload();
    // Rapatrie une partie de Lichess dans la bibliothèque, par son identifiant.
    // Les moteurs d'échecs installés sur la machine, Stockfish en tête.
    Q_INVOKABLE QStringList detectEngines() const;
    // Règle le premier trouvé comme moteur et rend son chemin, vide s'il n'y en a aucun.
    Q_INVOKABLE QString useDetectedEngine();
    Q_INVOKABLE void importOnlineGame(const QString &id);
    // L'identifiant d'une partie dans un lien Lichess, vide si le texte n'en est pas un.
    static QString lichessGameId(const QString &text);
    // { étude, chapitre } d'un lien d'étude Lichess ; chapitre vide pour l'étude entière.
    static QStringList lichessStudy(const QString &text);
    Q_INVOKABLE void importOnlineStudy(const QString &study, const QString &chapter);
    Q_INVOKABLE void classifyLibraryOpenings();
    // Le même classement, sans message : il suit chaque import, et un import réussi ne
    // doit pas échouer parce que le classement a manqué.
    void classifyQuietly(bool announce);
    QVariantMap openingPositions() const;
    Q_INVOKABLE void groupLibraryByOpening();
    Q_INVOKABLE void copyText(const QString &what);
    Q_INVOKABLE void setPreference(const QString &key, const QVariant &value);
    Q_INVOKABLE void retranslateUi(QObject *window);
    // Ce que contient le dossier des tables Syzygy, dit en une phrase pour les réglages.
    Q_INVOKABLE QString syzygyStatus(const QString &folder) const;
    Q_INVOKABLE QString uiText(const QString &text) const { return translator_->display(text); }
    // Le guide de l'utilisateur, en Markdown, dans la langue de l'interface.
    Q_INVOKABLE QString userGuide() const;
    // Vrai une seule fois, au tout premier lancement : l'interface propose alors le guide.
    Q_INVOKABLE bool firstLaunch();
    // Raccourcis clavier : le nom portable d'une touche pressée (« Ctrl+Shift+F »), vide pour
    // une touche de modification seule ; puis l'attribution, refusée si la combinaison sert
    // déjà ailleurs. Une combinaison vide rend celle d'origine.
    Q_INVOKABLE QString keyName(int key, int modifiers) const;
    Q_INVOKABLE QString setShortcut(const QString &action, const QString &sequence);
    Q_INVOKABLE void resetShortcuts();
    static const QList<QPair<QString, QString>> &defaultShortcuts();
    Q_INVOKABLE void startEngine();
    Q_INVOKABLE void toggleAnalysis();
    Q_INVOKABLE void toggleThreat();
    Q_INVOKABLE void addEngineLine(int index);
    Q_INVOKABLE void playComputer(bool humanWhite);
    Q_INVOKABLE void stopPlaying();
    Q_INVOKABLE void createCard(const QString &title);
    Q_INVOKABLE void startCard(qint64 id);
    // Deviner le coup : la partie ouverte est rejouée depuis le début, et l'on cherche
    // chaque coup du camp choisi. Un essai par coup ; le coup de la partie est ensuite joué.
    Q_INVOKABLE void startGuessing(bool white);
    // Questions d'entraînement, comme dans ChessBase : un coup de la ligne principale marqué
    // « [%tqu …] » est à trouver, en trois essais (3, 2 puis 1 point) ; les autres coups
    // se jouent d'eux-mêmes.
    Q_INVOKABLE void toggleQuestion();
    // Un diagramme dans la notation après le coup courant (NAG 220).
    Q_INVOKABLE void toggleDiagram();
    // Un titre de rubrique avant le coup courant ([%title …] dans son commentaire) ; un
    // texte vide le retire.
    Q_INVOKABLE void setMoveTitle(const QString &title);
    // La position courante sur Lichess : son adresse et son QR code, en image SVG.
    Q_INVOKABLE QVariantMap positionQr() const;
    // L'exercice de Stoyko : les variantes écrites depuis la position courante, la fin de
    // chacune évaluée par le moteur, et ce que le moteur joue lui-même au départ.
    Q_PROPERTY(QVariantMap stoyko READ stoyko NOTIFY stoykoChanged)
    QVariantMap stoyko() const { return stoyko_; }
    Q_INVOKABLE void startStoyko();
    // L'explorateur de plans pour la structure de pions de l'échiquier, calculé à part.
    Q_PROPERTY(QVariantMap plans READ plans NOTIFY plansChanged)
    QVariantMap plans() const { return plans_; }
    // Les positions semblables à celle de l'échiquier, cherchées à part.
    Q_PROPERTY(QVariantMap similar READ similar NOTIFY similarChanged)
    QVariantMap similar() const { return similar_; }
    Q_INVOKABLE void findSimilar();
    // Ouvre la partie et se place sur la position trouvée.
    Q_INVOKABLE void openSimilar(qint64 id, int node);
    Q_INVOKABLE void explorePlans();
    // Parties de la bibliothèque pas encore classées par clés stratégiques (à réindexer).
    Q_INVOKABLE int gamesWithoutThemes() const;
    Q_INVOKABLE void stopStoyko();
    Q_INVOKABLE void startQuestions();
    Q_INVOKABLE void revealCard();
    Q_INVOKABLE void finishTraining();
    Q_INVOKABLE void deleteCard(qint64 id);
    Q_INVOKABLE void loadExamples();
    Q_INVOKABLE void verifyDatabase();
    // Un journal à joindre à un signalement de bug : version, système, base, moteur,
    // réglages et derniers messages. Sans jeton ni mot de passe.
    Q_INVOKABLE QString diagnosticReport() const;
    Q_INVOKABLE void exportDiagnostic(const QString &url);
    Q_INVOKABLE void runPerft(int depth);
    Q_INVOKABLE void openDataFolder();
  signals:
    void changed();
    void libraryChanged();
    void sourcesChanged();
    void notationChanged();
    void importsChanged();
    void repertoiresChanged();
    void repertoireTaskChanged();
    void showAnalysisQueue();
    void preparationChanged();
    void reportChanged();
    void squaresChanged();
    void openingsChanged();
    void bookMovesChanged();
    void puzzlesChanged();
    void tabsChanged();
    void onlineReportChanged();
    void syzygyDownloadChanged();
    void nameVariantsChanged();
    void liveChanged();
    void savedSearchesChanged();
    void tournamentListChanged();
    void librarySummaryChanged();
    void stoykoChanged();
    void plansChanged();
    void similarChanged();
    void searchChanged();
    void libraryOpened();
    void engineChanged();
    void preferencesChanged();
    void accountsChanged();
    void profilesChanged();
    void fideSyncChanged();
    void busyChanged();
    void message(const QString &text, bool error = false);
    void promotionNeeded(const QString &prefix);
    void showAnalysis();
    void reviewChanged();

  private:
    int frenchNotation() const; // t64::Notation used to display SAN
    struct PreparationResult {
        QVariantList rows;
        QString error, fen;
        quint64 generation = 0;
    };
    QFutureWatcher<PreparationResult> preparationWorker_;
    std::shared_ptr<std::atomic_bool> cancelPreparation_;
    quint64 preparationGeneration_ = 0;
    bool preparationActive_ = false, preparationPending_ = false;
    QVariantMap preparation_, preparationFilters_;
    QString preparationFen_;
    void initPreparation();
    void startPreparation();
    struct RepertoireResult {
        QVariantMap overview, config;
        QString error, message, pgn;
        quint64 generation = 0;
        qint64 selected = 0;
        bool canceled = false;
    };
    QFutureWatcher<RepertoireResult> repertoireReader_, repertoireWriter_;
    std::shared_ptr<std::atomic_bool> cancelRepertoireRead_;
    std::shared_ptr<std::atomic<qint64>> repertoireProgress_ = std::make_shared<std::atomic<qint64>>(0);
    quint64 repertoireGeneration_ = 0;
    bool repertoiresEnabled_ = false, repertoireReadActive_ = false, repertoireReadPending_ = false;
    QVariantMap repertoireTask_;
    QTimer repertoirePoll_;
    void initRepertoires();
    void startRepertoireRead();
    void startRepertoireTask(
        const QString &operation,
        std::function<RepertoireResult(t64::Database &, const t64::RepertoireProgress &)> work);
    // depth 0 : la durée par position décide seule, comme pour une analyse ponctuelle.
    // Au-delà, c'est la profondeur qui décide et la durée devient un plafond.
    QVariantMap analysisConfig(int milliseconds, int profile, int depth = 0) const;
    // Les pseudos sous lesquels ce profil joue. Partagé par le tableau de bord et par la
    // mise en revue : deux résolutions distinctes finiraient par désigner deux lots.
    QStringList profileAccountNames(int profileIndex) const;
    qint64 selectedRepertoire_ = 0;
    QVariantList repertoires_;
    QVariantMap repertoireState_, repertoireExercise_;
    bool trainingActive() const {
        return cardId_ || puzzleId_ || guessing_ || !repertoireExercise_.isEmpty();
    }
    void refreshRepertoires(bool overview = true);
    bool playRepertoireAnswer(const QString &uci);
    struct SearchResult {
        QVariantList rows;
        QString error;
        quint64 generation = 0;
    };
    static constexpr int libraryPageSize = 200;
    QFutureWatcher<SearchResult> searchWorker_;
    std::shared_ptr<std::atomic_bool> cancelSearch_;
    quint64 searchGeneration_ = 0;
    bool searchActive_ = false, searchPending_ = false, searchEnabled_ = false;
    bool librarySearching_ = false, libraryHasNext_ = false;
    QString librarySearchStatus_;
    QVariantMap searchCursor_;
    QList<QVariantMap> searchCursors_;
    void initLibrarySearch();
    void requestLibrarySearch(bool resetPage = true);
    void startLibrarySearch();
    std::unique_ptr<t64::Content> content_;
    QVariantList sources_, importHistory_;
    void initContent();
    void importSourceFile(const QString &url, const QVariantMap &source);
    struct ImportResult {
        int count = 0, duplicates = 0, skipped = 0;
        QString error, detail, operation = "import";
        bool canceled = false;
        QVariantMap job;
    };
    struct ImportView {
        std::mutex mutex;
        t64::ImportProgress progress;
        qint64 elapsed = 0, startBytes = 0;
    };
    bool resumableImport_ = false, verifying_ = false;
    QVariantList importJobs_;
    std::shared_ptr<ImportView> importView_ = std::make_shared<ImportView>();
    std::unique_ptr<t64::Updater> updater_;
    QTimer backupTimer_;
    QFutureWatcher<QString> backupWorker_;
    void initReliability();
    QString dataPath_, sessionPath_;
    std::unique_ptr<t64::Database> db_;
    t64::Game game_, beforeTraining_;
    int node_ = 0, selected_ = -1, beforeNode_ = 0, importedGames_ = 0;
    qint64 gameId_ = 0, beforeId_ = 0, cardId_ = 0;
    QStringList solution_;
    int solutionIndex_ = 0;
    bool guessing_ = false, questions_ = false;
    int questionTries_ = 0;
    void advanceQuestions();
    int guessFound_ = 0, guessAsked_ = 0;
    bool trainingFailed_ = false, trainingComplete_ = false, beforeDirty_ = false;
    bool flipped_ = false, dirty_ = false, busy_ = false, analysis_ = false, playing_ = false,
         humanWhite_ = true;
    QString query_, resultFilter_;
    bool favoritesOnly_ = false, positionFilter_ = false, trashFilter_ = false;
    QVariantMap state_, stats_;
    QVariantList squares_, notation_, games_, openings_, bookMoves_, cards_, engineLines_;
    QVariantMap heldEval_;
    t64::PolyglotBook book_;
    qint64 puzzleId_ = 0, puzzleStarted_ = 0;
    QVariantMap puzzle_, puzzleStats_;
    QString puzzleTheme_, puzzleSource_;
    QElapsedTimer puzzleClock_;
    void refreshPuzzleStats();
    void finishPuzzle();
    bool acceptPuzzleMate(const QString &uci);
    t64::OnlineGames *online_ = nullptr;
    t64::LichessLogin *lichessLogin_ = nullptr;
    // Le jeton d'un compte Lichess enregistré (vide s'il n'est pas connecté), et celui
    // que l'explorateur utilise : le jeton collé à part, sinon celui d'un compte.
    QString accountToken(const QString &user) const;
    QString lichessToken() const;
    t64::OpeningExplorer *explorer_ = nullptr;
    QVariantMap onlineReport_;
    t64::SyzygyDownload *syzygyLoader_ = nullptr;
    QVariantMap syzygyState_;
    QVariantMap nameVariants_; // field, running, groups, error
    t64::Broadcasts *broadcasts_ = nullptr;
    QVariantMap live_;
    QVariantMap tournamentList_;
    QVariantMap librarySummary_;
    int summaryGeneration_ = 0;
    QTimer assistRefresh_;
    QTimer *liveTimer_ = nullptr;
    bool liveWatching_ = false; // le panneau est ouvert
    int livePendingGame_ = -1;  // partie à ouvrir à la réception du PGN
    QStringList liveFollowed_;  // White, Black, Event de la partie suivie
    void ensureBroadcasts();
    void liveTick();
    t64::Uci *liveEngine_ = nullptr; // évalue les échiquiers de la ronde, un par un
    t64::Uci *stoykoEngine_ = nullptr;
    QVariantMap stoyko_;
    QVariantMap plans_;
    int plansGeneration_ = 0;
    QVariantMap similar_;
    int similarGeneration_ = 0;
    QStringList stoykoQueue_; // positions à évaluer, la première étant le départ
    QString stoykoEvaluating_;
    void stoykoNext();
    QStringList liveQueue_;
    QString liveEvaluating_;
    QVariantMap liveEvals_; // FEN → {score, eval}
    void liveNextEval();
    void liveReceivedPgn(const QString &roundId, const QByteArray &pgn);
    QString syzygyFolder() const;
    QString onlineSource_, onlineKey_;
    QHash<QString, QVariantMap> onlineCache_;
    t64::FideSync *fide_ = nullptr;
    QVariantMap fideSync_;
    int fideProfile_ = -1;
    QStringList fideSpeeds_;
    QString onlineSyncKey_, onlineFile_;
    qint64 onlineSyncTime_ = 0;
    QList<int> syncQueue_;
    int syncingAccount_ = -1;
    void nextAccountSync();
    void accountSynced(int added);
    void openBook();
    QString engineStatus_ = "Moteur déconnecté";
    QSettings settings_;
    std::unique_ptr<t64::UiTranslator> translator_;
    QTimer autosave_, analysisTimer_;
    t64::Uci engine_;
    t64::Comparison comparison_;
    t64::Tournament tournament_;
    QString tournamentFile_, tournamentTitle_;
    QStringList bookOpenings(int count, int plies);
    t64::AnalysisQueue analysisQueue_;
    void initAnalyses();
    QFutureWatcher<ImportResult> importer_;
    std::shared_ptr<std::atomic_bool> cancelTask_ = std::make_shared<std::atomic_bool>(false);
    std::shared_ptr<std::atomic_int> taskProgress_ = std::make_shared<std::atomic_int>(0);
    QTimer taskPoll_;
    struct Snapshot {
        t64::Game game;
        int node = 0;
        qint64 id = 0;
        bool dirty = false;
        size_t bytes = 0;
    };
    struct Study {
        Snapshot position;
        std::deque<Snapshot> undo, redo;
        QVariantList review;
    };
    std::vector<Study> studies_;
    int activeTab_ = 0;
    std::deque<Snapshot> undo_, redo_;
    QVariantList review_, players_;
    QVariantMap reviewAccuracy_, reviewQuality_;
    QVariantMap report_;
    // Dossiers déjà calculés, tant que le fichier de la bibliothèque n'a pas bougé : sur
    // une grosse base, revenir à une position courante ne relit pas un million de lignes.
    QHash<QString, QVariantMap> reportCache_;
    QString reportStamp() const;
    std::shared_ptr<QTemporaryFile> unpackedStudy_;
    QStringList pendingStudies_;
    QFutureWatcher<int> classifier_;
    bool classifyAnnounces_ = false;
    QFutureWatcher<QVariantMap> reportWorker_;
    std::shared_ptr<std::atomic_bool> cancelReport_;
    qint64 reportGeneration_ = 0;
    bool reportPending_ = false;
    QVariantMap advancedFilters_, playerProfile_;
    t64::Uci reviewer_;
    t64::Game reviewGame_;
    std::vector<int> reviewNodes_;
    QVariantList reviewScores_;
    bool reviewing_ = false;
    int reviewIndex_ = 0, reviewMilliseconds_ = 500, reviewDepth_ = 0, reviewEpoch_ = 0, reviewPass_ = 0,
        reviewDone_ = 0;
    bool reviewTwoPasses_ = true;
    void rebuildReview();
    QString reviewError_;
    QTimer clock_;
    QElapsedTimer clockElapsed_, commentEdit_;
    int commentNode_ = -1;
    bool beforeFlipped_ = false;
    qint64 whiteMs_ = 0, blackMs_ = 0;
    int incrementMs_ = 0;
    bool clockRunning_ = false, localGame_ = false;
    QString clockResult_;
    void remember();
    Snapshot snapshot() const;
    void restore(const Snapshot &snapshot);
    void storeTab(bool history = true);
    void initFeatures();
    QVariantList annotations() const;
    void requestReviewPosition();
    void finishReviewPosition(const QString &fen, const QVariantList &lines);
    void tickClock();
    void afterClockMove(bool white);
    void detectOpening();
    void refresh(bool changedContent = false);
    void refreshLibrary();
    void checkpoint();
    void writeSession();
    void replaceGame(t64::Game game, qint64 id = 0);
    void adoptGame(t64::Game game, qint64 id);
    // Publie ce qui a réellement changé depuis la dernière fois, clé par clé et liste
    // par liste, puis « changed » pour ceux qui écoutent tout.
    void publish();
    void refreshNotation();
    QQmlPropertyMap *stateObject_ = nullptr, *cells_ = nullptr;
    QVariantList moveRows_, moveFlow_;
    QVariantList shownSquares_, shownOpenings_, shownBook_, shownTabs_;
    int heardNode_ = 0;
    // La menace se demande pour une position : la quitter y met fin d'elle-même.
    int threatNode_ = -1;
    bool threatActive() const { return analysis_ && threatNode_ == node_; }
    QVariantMap shownPuzzles_;
    // Relève une seule fois les anciens réglages par défaut qui bridaient le moteur.
    void unleashEngineOnce();
    // Rang de la partie ouverte dans la liste de la bibliothèque, -1 si absente.
    int gameIndex() const;
    void importBytes(const QByteArray &bytes);
    void requestEngine();
    QString positionCommand() const;
    std::deque<QString> recentMessages_;
    void importOnlineDownload(const QString &downloaded);
    void orientForOwnGame();
    // Fils et table de hachage des moteurs : le réglage s'il existe, sinon ce que la
    // machine peut donner (tous les cœurs moins un), bornés à ses cœurs et à sa mémoire.
    int engineThreads() const;
    int engineHashMb() const;
    QString localPath(const QString &url) const;
    void fail(const std::exception &e);
};
