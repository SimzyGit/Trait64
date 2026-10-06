# Adaptation locale par Trait64

Base : commit `53e6a841dcda7059a2af363d85f785ef1817304a`, fichier `include/chess.hpp`.

Une seule adaptation : la branche `using enum underlying` de `Square` est activée à partir de GCC 13, au lieu de GCC 12. GCC 12 utilise donc les déclarations `static constexpr` équivalentes déjà présentes dans la branche de compatibilité amont. Aucun algorithme d’échecs n’est changé.

Motif constaté : la compilation C++23 avec GCC 12.2 sur Debian 12 provoque un internal compiler error `tsubst_copy` lors de l’instanciation de `setFenCommon`, sur `Square::NO_SQ`. La compilation avec GCC 13 et GCC 14 et les tests de règles passent. Ce contournement local est marqué par un commentaire dans le fichier ; conserver cette note lors d’une mise à jour et retester la branche GCC 12.

Licence du composant et de cette adaptation : MIT, voir LICENSE dans ce dossier.
