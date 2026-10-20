# Adaptation locale par Trait64

Base : commit `53e6a841dcda7059a2af363d85f785ef1817304a`, fichier `include/chess.hpp`.

La branche `using enum underlying` de `Square` est activée à partir de GCC 13, au lieu de GCC 12. GCC 12 utilise donc les déclarations `static constexpr` équivalentes déjà présentes dans la branche de compatibilité amont.

Motif constaté : la compilation C++23 avec GCC 12.2 sur Debian 12 provoque un internal compiler error `tsubst_copy` lors de l’instanciation de `setFenCommon`, sur `Square::NO_SQ`. La compilation avec GCC 13 et GCC 14 et les tests de règles passent. Ce contournement local est marqué par un commentaire dans le fichier ; conserver cette note lors d’une mise à jour et retester la branche GCC 12.

Licence du composant et de cette adaptation : MIT, voir LICENSE dans ce dossier.

Audit du 20 octobre 2026 : compteurs `hfm_`, `State::half_moves` et `plies_` élargis à 32 bits pour empêcher leur remise à zéro silencieuse lors d’une longue suite. La recherche de répétitions borne explicitement le compteur à l’historique disponible avant la soustraction signée. Trait64 garde ses limites d’import FEN (255 demi-coups sans prise/pion et numéro de coup 32000) et refuse atomiquement une continuation qui les dépasserait. Les tests vérifient l’incrément, l’annulation du coup, les répétitions et l’absence d’étude partiellement modifiée. Aucun algorithme de génération des coups n’est modifié.
