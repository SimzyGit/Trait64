## Nouvelle interface Trait64

- Navigation horizontale numérotée, accueil composé comme un carnet et aperçu réel de l’étude à reprendre.
- Palette encre/papier/cuivre, titres à empattements, notation à chasse fixe, panneaux et boutons redessinés.
- Plus de place pour l’échiquier ; commandes de position réunies sous le plateau ; carnet de coups et notes numérotés.
- Neuvième palette « Trait64 » et accent adapté au thème clair/sombre ; couleurs et pièces personnelles conservées.
- Navigation, échiquier et accès aux outils vérifiés à 960 × 640, avec tests de clics sur les sept sections.

## Fonctions de la série 0.3 conservées

- Vérification GitHub Releases en arrière-plan, canaux Stable/Bêta/Développement, notes de version, Mettre à jour/Plus tard/Ignorer.
- Dépôt privé : accès par GitHub CLI activable, variable T64_GITHUB_TOKEN ou jeton de session non enregistré.
- Téléchargement borné, SHA-256 obligatoire, filtrage des redirections, refus des rétrogradations et sauvegarde avant installation.
- Installation Debian avec autorisation système ; remplacement atomique AppImage avec ancien exécutable conservé.
- Sauvegardes automatiques toutes les six heures, conservation de 14 instantanés ; sauvegardes avant mise à jour conservées séparément.
- Tests automatiques Debian 12/13, contrôles mémoire du cœur, paquets .deb et AppImage Linux x86_64.

- Comparaison de 2 à 4 moteurs UCI, MultiPV, insertion de variantes et budgets CPU/hash partagés.
- Bibliothèque schéma 3 : index FTS5, filtres exacts et statistiques d’ouvertures/joueurs incrémentales ; sauvegarde avant migration.

Les fonctions 0.2 sont conservées. Cette préversion ne réalise pas encore l’ensemble du catalogue initial. Le module de mise à jour ne peut pas être ajouté rétroactivement à 0.2 : cette première version doit être installée manuellement.
