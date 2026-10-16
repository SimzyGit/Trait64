# Stockage GitHub Actions

Les paquets distribués et leurs sources correspondantes sont conservés dans **GitHub Releases**. Le workflow de compilation n’utilise plus d’archives Actions : Debian 12 et Debian 13 sont compilés dans deux conteneurs propres sur un même runner, avec un dossier local commun pour les paquets. Les sources du projet sont montées en lecture seule ; aucun jeton GitHub n’est transmis aux conteneurs.

## Politique de publication

- Les PR exécutent tous les tests, les paquets DEB installés et l’AppImage, sans publier ni stocker d’archive.
- Les commits de code sur main publient exclusivement des versions de développement. Stable et Bêta demandent un tag ou une publication manuelle explicite. Les changements de documentation seuls ne déclenchent pas de compilation.
- Après les tests des deux distributions et les sanitizers, `scripts/publish-release.py` exige exactement les deux DEB, l’AppImage et les sources du runtime. Il produit SHA256SUMS, envoie les cinq fichiers dans un brouillon Releases et vérifie les tailles et SHA-256 annoncés par GitHub avant publication.
- Une version publiée n’est jamais réécrite. Un brouillon ne peut être repris que s’il a été créé par ce processus pour le même commit, avec des fichiers attendus. Tout fichier supplémentaire est conservé et bloque la reprise.
- Les publications sont sérialisées. Les trois canaux de l’application restent inchangés.
- La purge quotidienne à 03:23 UTC et la purge après succès restent actives pour les anciennes archives connues. Elles ne touchent pas aux sources, commits, branches, logs ni fichiers Releases.

## Nettoyage manuel

Dans Actions, ouvrir `Reclaim Trait64 Actions storage` puis `Run workflow` :

- `all_completed` retire toutes les archives de compilation terminées, sans
  limite d'âge ;
- `clear_caches` retire également les caches reproductibles, uniquement en
  l'absence d'une autre exécution active.

Le script `scripts/cleanup-actions.py` fonctionne par défaut en simulation.
L'option `--apply` autorise réellement les suppressions. Il n'accepte que le
dépôt Trait64 et les deux noms d'archives de compilation connus. Il vérifie
l'état de chaque exécution juste avant la suppression et contrôle que les
versions déjà publiées conservent leurs paquets, sommes de contrôle et sources.
Le code exécuté par un événement `workflow_run` provient toujours de la branche
par défaut, jamais du code d'une demande de modification externe.

Le bilan détaillé et les octets effectivement supprimés apparaissent dans les
journaux et le résumé de l'exécution. Aucune archive supplémentaire n'est créée
pour ce rapport. Le compteur de facturation GitHub peut prendre 6 à 12 heures
pour refléter une suppression ; l'utilisation déjà comptabilisée reste acquise.

## Audit initial — 15 octobre 2026

Avant nettoyage : **58 archives**, soit **8 371 207 728 octets**, issues de
33 exécutions terminées. L'ancienne configuration conservait les archives
pendant 30 jours et les produisait aussi sur les branches et les demandes de
modification. Les dix versions déjà publiées disposaient de leurs cinq fichiers,
dont les sources nécessaires aux licences : elles doivent rester disponibles.

Nettoyage réellement effectué à 05:59 UTC dans l'exécution
[38689692486](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/38689692486) :
les 58 archives et leurs 8 371 207 728 octets ont été supprimés. L'inventaire final
ne contient aucune archive Actions ni cache. Les dix releases et leurs cinquante
fichiers sont identiques avant/après. La [PR #4](https://github.com/feuvrierbernardinisimhon-png/Trait64/pull/4)
est fusionnée après validation Debian 12/13, paquets, AppImage et sanitizers.
La purge déclenchée automatiquement à la fin de la CI de la PR #5 a également
réussi ([38692633667](https://github.com/feuvrierbernardinisimhon-png/Trait64/actions/runs/38692633667)).

À 06:35 UTC, la publication dev.36 (exécution 38692749119) a encore rencontré
`Artifact storage quota has been hit` au téléversement, malgré un inventaire
vide. Les tests et les paquets ont réussi ; le service annonce un recalcul toutes
les 6 à 12 heures. Attendre ce recalcul puis relancer uniquement les tâches
échouées. Voir `WORK-IN-PROGRESS.md` pour l'état exact ; aucun achat ni changement
du budget du compte n'a été effectué.

Références :

- <https://docs.github.com/en/billing/concepts/product-billing/github-actions>
- <https://docs.github.com/en/actions/managing-workflow-runs-and-deployments/managing-workflow-runs/removing-workflow-artifacts>

Validation locale :

```sh
python3 -m unittest discover -s tests -p 'test_cleanup_actions.py' -v
```
