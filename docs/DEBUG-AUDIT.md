# Audit de débogage global — 20 octobre 2026

Demande utilisateur : suspendre les ajouts et déboguer tout le logiciel.
Base exacte : `4e4cd3d36711c98a5952d26dca4b2531bc4c6264` (code de dev.46).

État de travail, pas une certification finale :

- Copie isolée vérifiée contre l’arbre GitHub ; anciennes copies locales préservées.
- Compilation Debug complète, GCC 13.3 / Qt 6.4.2, avec avertissements renforcés.
- Référence avant modification : **11/11 suites, 55,76 s**, Stockfish réel.
- Tests de publication et de nettoyage exécutés séparément.
- Régressions reproduites avant correction : session JSON sans structure restaurable écrasée sans copie ; dernière annulation de réindexation ignorée ; compteur sans prise/pion remis à zéro après 255 ; continuation au-delà du coup 32000 laissant un nœud illisible.
- Corrections en cours avec tests dédiés. Les résultats UCI réentrants et les positions à nulle réclamable sont encore en cours de vérification.

Périmètre de la revue : cœur et formats, SQLite/migrations/imports/répertoires, analyses et UCI, contrôleur/session/historique/entraînement/pendule, interface QML, téléchargements/mises à jour, scripts et distribution.

À terminer : stress reproductible, sanitizers sur tous les modules, tests complets après correction, CI Debian 12/13 et bilan exact des limites. Aucune nouvelle fonctionnalité ni nouvelle version publiée par cet audit à ce stade.
