# Mises à jour et distribution

Le module est disponible à partir de 0.3. Les anciennes versions 0.1/0.2 nécessitent une première installation manuelle de 0.3.

## Utilisation

Ouvrir **Mises à jour**, choisir Stable, Bêta ou Développement. La vérification au démarrage est activée par défaut et désactivable. Aucune requête n’est nécessaire pour utiliser les fonctions d’échecs. Le dépôt étant privé, activer **Utiliser ma connexion GitHub CLI** après `gh auth login`, fournir `T64_GITHUB_TOKEN` dans l’environnement, ou saisir un jeton de session limité au dépôt, permission Contents en lecture. Le jeton de session n’est pas écrit dans les paramètres. Le logiciel ne contient aucune clé d’accès.

Une version compatible plus récente affiche son numéro et ses notes. **Plus tard** reporte les vérifications automatiques de 24 heures ; **Ignorer** mémorise cette version sur le canal choisi. La vérification manuelle permet de retrouver une version ignorée. Les versions sont comparées selon SemVer, sans rétrogradation automatique.

Après **Mettre à jour**, Trait64 télécharge l’asset GitHub adapté à l’architecture et au mode d’installation, vérifie sa taille et son SHA-256 fourni par l’API GitHub, puis crée une sauvegarde de la bibliothèque active, de la session et des préférences. Une sauvegarde échouée bloque l’installation. Les jetons ne sont envoyés qu’à api.github.com et jamais aux hôtes de redirection. Seules des URL HTTPS GitHub/ressources GitHub autorisées sont suivies. Le contrôle SHA-256 repose sur la confiance dans le dépôt GitHub et HTTPS ; ce n’est pas une signature indépendante du mainteneur.

Pour un .deb installé dans /usr/bin, pkexec ouvre l’autorisation système et apt-get installe le paquet vérifié avec interdiction de supprimer des paquets. Pour une AppImage, le remplacement est atomique dans le même dossier, avec conservation du précédent exécutable `.previous`. Une installation utilisateur issue des sources peut être remplacée dans `~/.local/bin/trait64` par l’AppImage. Un chemin non accessible en écriture est refusé. Le bouton **Redémarrer** relance le nouveau programme après libération du verrou de session.

## Publication

Le workflow `.github/workflows/linux.yml` teste Debian 12 et 13 et le cœur avec sanitizers avant publication. Un commit sur main produit uniquement `X.Y.Z-dev.N`, marqué prerelease. Les branches et PR sont testées sans publication. Un tag `vX.Y.Z-beta.N` ou `vX.Y.Z-rc.N` produit une préversion ; `vX.Y.Z` produit une stable. **Actions → Build, test and release Trait64 → Run workflow** accepte aussi une version explicite. Une modification ordinaire du code ne publie jamais de stable.

Les assets sont d’abord chargés dans une Release brouillon. La publication intervient après chargement complet. SHA256SUMS et les sources des bibliothèques AppImage sont joints. Une publication interrompue laisse un brouillon, jamais une notification de version partiellement livrée. Le digest des assets est fourni par GitHub Releases ; les versions sans digest sont refusées par l’updater.

## Sauvegardes et retour arrière

Les sauvegardes sont sous `backups/` dans le dossier de données affiché par les paramètres. Les 14 dernières `auto-*` sont conservées ; les dossiers `pre-update-*` et sauvegardes de migration ne sont pas supprimés par cette rotation. Une base peut être ouverte séparément avec **Outils → Ouvrir une base**. Conserver la session JSON et les préférences avec la copie SQLite si un retour à une ancienne version est nécessaire. Une ancienne version ne doit pas ouvrir une base migrée vers un schéma plus récent.

## Sources techniques

- https://docs.github.com/en/rest/releases/releases
- https://docs.github.com/en/rest/releases/assets
- https://docs.appimage.org/packaging-guide/from-source/linuxdeploy-user-guide.html
- https://github.com/linuxdeploy/linuxdeploy-plugin-qt
