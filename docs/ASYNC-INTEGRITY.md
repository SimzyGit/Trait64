# Vérification d'intégrité en arrière-plan

Le bouton **Vérifier la base** exécutait `PRAGMA integrity_check` sur le fil de
l'interface. Le contrôle CLI sur 100 000 parties a pris 110,028 s sur l'hôte local :
cette durée n'est pas une mesure directe d'un gel Qt, mais l'appel synchrone pouvait
retenir l'interface pendant toute l'opération.

Le contrôle manuel s'exécute maintenant dans un worker, avec sa propre connexion
SQLite. La barre d'état indique « Vérification de la bibliothèque… » et le bouton
**Interrompre** demande l'annulation. Une opération déjà en cours empêche d'en
lancer une autre. L'étude non enregistrée reste en place.

L'annulation utilise le mécanisme SQLite déjà employé par les recherches :
un drapeau atomique partagé et un gestionnaire de progression sur la connexion du
worker. Elle ne modifie pas les lignes et retire son gestionnaire après la requête.
Une interruption ou une erreur d'itération ne peut pas produire un faux résultat
« ok ». Le contrôleur annule et attend son worker à la fermeture.

Le résultat terminé est copié avant l'émission de `busyChanged` : un observateur
peut lancer immédiatement une nouvelle tâche sans remplacer le résultat encore
à présenter.

## Vérification

- Test SQL : annulation préalable, puis annulation déclenchée par l'autoriseur
  SQLite lors de la préparation de la vraie requête. Vérification de l'interruption
  du moteur SQL, des données inchangées et d'une nouvelle requête réussie.
- Test Qt : worker retenu dans la file, événements UI traités pendant l'attente,
  refus du chevauchement, annulation, étude conservée, reprise réussie,
  relance depuis le signal de fin et destruction avec une tâche active.
- 12/12 suites locales réussies en 62,73 s, Stockfish réel ; aucune suite ignorée.
  La CI de la PR complète les contrôles locaux avant fusion.

Le schéma reste 7. La sauvegarde manuelle, la sauvegarde avant mise à jour et le
contrôle préalable à l'ouverture d'une bibliothèque sont des chemins distincts,
encore synchrones. La qualification des 100k de dev.55 reste distincte de ces
tests fonctionnels ; aucun nouveau résultat de débit ni essai Voyager n'est déduit
du passage en arrière-plan.
