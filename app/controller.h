// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "comparison.h"
#include "content.h"
#include "database.h"
#include "fide.h"
#include "online.h"
#include "polyglot.h"
#include "queue.h"
#include "tournament.h"
#include "uci.h"
#include "ui_translator.h"
#include "updater.h"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
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
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(QVariantList squares READ squares NOTIFY changed)
    Q_PROPERTY(QVariantList notation READ notation NOTIFY changed)
    Q_PROPERTY(QVariantList moveRows READ moveRows NOTIFY notationChanged)
    Q_PROPERTY(QVariantList moveFlow READ moveFlow NOTIFY notationChanged)
    Q_PROPERTY(QVariantList games READ games NOTIFY libraryChanged)
    // Nombre de parties ajoutées par la dernière synchronisation ou le dernier import :
    // c'est le lot que le bouton de bilan propose de passer en revue. 0 : rien à proposer.
    Q_PROPERTY(int importedGames READ importedGames NOTIFY libraryChanged)
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList openings READ openings NOTIFY changed)
    // Dossier de la position courante : coups joués, scores, tranches de force et
    // parties modèles. Construit en arrière-plan, comme la préparation.
    Q_PROPERTY(QVariantMap openingReport READ openingReport NOTIFY reportChanged)
    Q_PROPERTY(QVariantList bookMoves READ bookMoves NOTIFY changed)
    Q_PROPERTY(QVariantMap puzzles READ puzzles NOTIFY changed)
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
    // Profils de joueur : un utilisateur rassemble ses comptes en ligne et son profil
    // FIDE sous un seul nom. Chaque profil porte la liste des comptes qui lui sont liés.
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
    // Téléchargement d'une liste FIDE en cours : {busy, speed, index, count, received,
    // total, status}. Les listes pèsent quelques dizaines de mégaoctets chacune.
    Q_PROPERTY(QVariantMap fideSync READ fideSync NOTIFY fideSyncChanged)
    Q_PROPERTY(int taskProgress READ taskProgress NOTIFY busyChanged)
    Q_PROPERTY(QString taskStatus READ taskStatus NOTIFY busyChanged)
    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY changed)
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
    void nextPendingStudy();
    static QString studyCollectionName(const QString &path);
    // Les recueils, et les parties de l'un d'eux. L'onglet Études les montre comme des
    // chapitres : un clic en ouvre une dans Analyse.
    Q_PROPERTY(QVariantList studyCollections READ studyCollections NOTIFY sourcesChanged)
    QVariantList studyCollections() const;
    Q_INVOKABLE QVariantList studyGames(qint64 sourceId) const;
    Q_INVOKABLE void importRepertoire(qint64 id, const QString &url, bool white = true);
    // Un dossier d'études, tel que l'exportent En Croissant et Pawn Appétit : un
    // répertoire par fichier PGN, sous-dossiers compris, nommés d'après leur chemin.
    Q_INVOKABLE void importRepertoireFolder(const QString &url, bool white = true);
    Q_INVOKABLE void exportRepertoire(qint64 id, const QString &url);
    Q_INVOKABLE void editRepertoireMove(qint64 id, const QString &fen, const QString &uci,
                                        const QString &note, bool principal);
    Q_INVOKABLE void removeRepertoireMove(qint64 id, const QString &fen, const QString &uci);
    Q_INVOKABLE void openRepertoirePosition(qint64 id, const QString &fen);
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
    QVariantList squares() const { return squares_; }
    QVariantList notation() const { return notation_; }
    QVariantList moveRows() const;
    QVariantList moveFlow() const;
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
    Q_INVOKABLE void analyseGame(int milliseconds = 500, bool twoPasses = true);
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
    // site: "lichess" or "chesscom". Lichess re-imports only games newer than the last import.
    Q_INVOKABLE void importOnline(const QString &site, const QString &user, int max);
    Q_INVOKABLE void cancelOnline();
    // Saved accounts ("lichess" / "chesscom"); adding one syncs it at once.
    QVariantList onlineAccounts() const;
    Q_INVOKABLE void addOnlineAccount(const QString &site, const QString &user);
    Q_INVOKABLE void removeOnlineAccount(int index);
    Q_INVOKABLE void syncOnlineAccount(int index);
    Q_INVOKABLE void syncAllAccounts();
    bool onlineFetching() const { return online_->busy(); }
    Q_INVOKABLE void exportFile(const QString &url, bool all = false);
    Q_INVOKABLE void backup(const QString &url);
    Q_INVOKABLE void openBackup(const QString &url);
    Q_INVOKABLE void clickSquare(const QString &square);
    Q_INVOKABLE void tryMove(const QString &from, const QString &to);
    Q_INVOKABLE void move(const QString &uci);
    Q_INVOKABLE void navigate(int node);
    Q_INVOKABLE void step(int direction);
    Q_INVOKABLE void flip();
    Q_INVOKABLE void setComment(const QString &comment);
    Q_INVOKABLE void setNag(int nag);
    Q_INVOKABLE void setTags(const QVariantMap &tags);
    Q_INVOKABLE void promote();
    Q_INVOKABLE void deleteVariation();
    Q_INVOKABLE void search(const QString &query, const QString &result, bool favorite, bool position,
                            bool trash);
    Q_INVOKABLE void favorite(qint64 id, bool value);
    Q_INVOKABLE void trash(qint64 id, bool value);
    Q_INVOKABLE void setCollection(qint64 id, const QString &collection);
    // Range toute la bibliothèque en collections par famille d'ouverture, sans rien
    // télécharger : la table ECO est embarquée.
    // Donne son code ECO à chaque partie qui n'en porte pas : la plupart des PGN n'en
    // portent aucun, et la recherche par ouverture restait alors vide en silence.
    Q_INVOKABLE void buildOpeningReport();
    QVariantMap openingReport() const { return report_; }
    Q_INVOKABLE void classifyLibraryOpenings();
    // Le même classement, sans message : il suit chaque import, et un import réussi ne
    // doit pas échouer parce que le classement a manqué.
    void classifyQuietly(bool announce);
    QVariantMap openingPositions() const;
    Q_INVOKABLE void groupLibraryByOpening();
    Q_INVOKABLE void copyText(const QString &what);
    Q_INVOKABLE void setPreference(const QString &key, const QVariant &value);
    Q_INVOKABLE void retranslateUi(QObject *window);
    Q_INVOKABLE QString uiText(const QString &text) const { return translator_->display(text); }
    Q_INVOKABLE void startEngine();
    Q_INVOKABLE void toggleAnalysis();
    Q_INVOKABLE void addEngineLine(int index);
    Q_INVOKABLE void playComputer(bool humanWhite);
    Q_INVOKABLE void stopPlaying();
    Q_INVOKABLE void createCard(const QString &title);
    Q_INVOKABLE void startCard(qint64 id);
    Q_INVOKABLE void revealCard();
    Q_INVOKABLE void finishTraining();
    Q_INVOKABLE void deleteCard(qint64 id);
    Q_INVOKABLE void loadExamples();
    Q_INVOKABLE void verifyDatabase();
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
    bool trainingActive() const { return cardId_ || puzzleId_ || !repertoireExercise_.isEmpty(); }
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
        int count = 0, duplicates = 0;
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
    int reviewIndex_ = 0, reviewMilliseconds_ = 500, reviewEpoch_ = 0, reviewPass_ = 0, reviewDone_ = 0;
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
    void importBytes(const QByteArray &bytes);
    void requestEngine();
    QString positionCommand() const;
    QString localPath(const QString &url) const;
    void fail(const std::exception &e);
};
