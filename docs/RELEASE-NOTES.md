## Nouveautés

- Vérification GitHub Releases en arrière-plan, canaux Stable/Bêta/Développement, notes de version, Mettre à jour/Plus tard/Ignorer.
- Dépôt privé : accès par GitHub CLI activable, variable T64_GITHUB_TOKEN ou jeton de session non enregistré.
- Téléchargement borné, SHA-256 obligatoire, filtrage des redirections, refus des rétrogradations et sauvegarde avant installation.
- Installation Debian avec autorisation système ; remplacement atomique AppImage avec ancien exécutable conservé.
- Sauvegardes automatiques toutes les six heures, conservation de 14 instantanés ; sauvegardes avant mise à jour conservées séparément.
- Tests automatiques Debian 12/13, contrôles mémoire du cœur, paquets .deb et AppImage Linux x86_64.

Les fonctions 0.2 sont conservées. Cette préversion ne réalise pas encore l’ensemble du catalogue initial. Le module de mise à jour ne peut pas être ajouté rétroactivement à 0.2 : cette première version doit être installée manuellement.
