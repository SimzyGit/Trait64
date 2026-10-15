# Stockage GitHub Actions

Les paquets distribués à l'utilisateur et leurs sources correspondantes sont
conservés dans **GitHub Releases**. Les archives Actions servent uniquement au
transfert des paquets entre les tâches de compilation et de publication.

## Politique de conservation

- Une demande de modification lance les tests et la compilation Debian 12/13,
  les vérifications des paquets installés et de l'AppImage, sans téléverser
  d'archive Actions.
- Un commit de code sur `main` publie uniquement une version de développement.
  Les versions stables et bêta demandent toujours un tag ou une publication
  manuelle. Les changements de documentation seuls ne lancent pas de compilation.
- Les publications conservent temporairement leurs archives Actions pendant un
  jour au maximum. Le workflow `Reclaim Trait64 Actions storage` les supprime
  aussitôt que le workflow de publication s'est terminé avec succès.
- Une purge quotidienne à 03:23 UTC retire les archives de compilation terminées
  qui ont plus de 24 heures, y compris celles d'une publication interrompue.
- Les sources, commits, branches, journaux d'exécution et fichiers des versions
  publiées ne sont jamais supprimés par cette purge.

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

Références :

- <https://docs.github.com/en/billing/concepts/product-billing/github-actions>
- <https://docs.github.com/en/actions/managing-workflow-runs-and-deployments/managing-workflow-runs/removing-workflow-artifacts>

Validation locale :

```sh
python3 -m unittest discover -s tests -p 'test_cleanup_actions.py' -v
```
