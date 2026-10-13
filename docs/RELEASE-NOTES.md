## Moteurs, bases et recherches

- Téléchargement de Stockfish 19, Arasan 26 et PlentyChess 8, avec SHA-256 épinglé, progression, annulation et contrôle de compatibilité. PlentyChess nécessite glibc 2.38 ou plus (Debian 13).
- Profils moteurs indépendants, options UCI découvertes et validées : listes, bornes numériques, texte et commandes.
- 42 collections PGN à télécharger : 18 champions classiques, quatre champions FIDE de la période scindée, huit championnes disponibles, matchs historiques et ouvertures. PGN Mentor reste le fournisseur ; les conditions de téléchargement sont affichées.
- Bases nommées, historique, rattachement d’une même partie à plusieurs bases sans duplication et export d’une base choisie.
- Recherche combinant base, joueur/couleur, ouverture par nom ou ECO, date, Elo, tournoi, matériel et position.
- Recherche en arrière-plan réellement annulable ; pages de 200 parties par curseur, retour à la page précédente, résultats anciens écartés.
- Migration SQLite 4 avec sauvegarde. L’interface et les fonctions existantes sont conservées.

## Fonctions de la série 0.3 conservées

- Vérification GitHub Releases en arrière-plan, canaux Stable/Bêta/Développement, notes de version, Mettre à jour/Plus tard/Ignorer.
- Dépôt privé : accès par GitHub CLI activable, variable T64_GITHUB_TOKEN ou jeton de session non enregistré.
- Téléchargement borné, SHA-256 obligatoire, filtrage des redirections, refus des rétrogradations et sauvegarde avant installation.
- Installation Debian avec autorisation système ; remplacement atomique AppImage avec ancien exécutable conservé.
- Sauvegardes automatiques toutes les six heures, conservation de 14 instantanés ; sauvegardes avant mise à jour conservées séparément.
- Tests automatiques Debian 12/13, contrôles mémoire du cœur, paquets .deb et AppImage Linux x86_64.

- Comparaison de 2 à 4 moteurs UCI, MultiPV, insertion de variantes et budgets CPU/hash partagés.
- Bibliothèque : index FTS5, filtres exacts et statistiques d’ouvertures/joueurs incrémentales ; sauvegarde avant migration.

Les fonctions 0.2 sont conservées. Cette préversion ne réalise pas encore l’ensemble du catalogue initial. Le module de mise à jour ne peut pas être ajouté rétroactivement à 0.2 : cette première version doit être installée manuellement.

Les répertoires complets, l’import reprenable et la qualification sur 1/2/5/10 millions de PGN réels restent à développer. Les limites et la suite précise figurent dans `docs/NEXT-STEPS.md` et `docs/PERFORMANCE.md`.
