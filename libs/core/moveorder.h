// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "chess.hpp"
#include <vector>

namespace t64 {
// Les bases d'En Croissant notent chaque coup sur un octet : le rang du coup dans la
// liste des coups légaux de la position, telle que la produit la bibliothèque Shakmaty.
// Relire ces bases demande donc de retrouver cet ordre exactement — un rang de travers
// ne donne pas une erreur, il donne un autre coup légal, et la partie devient fausse
// sans que rien ne le signale.
//
// L'ordre, repris de shakmaty 0.30 (Position::legal_moves pour les échecs classiques) :
//   1. les prises en passant, par case de départ croissante ;
//   2. hors échec, les pions, puis cavaliers, fous, tours, dames, le roi, enfin les
//      roques (petit puis grand) ; en échec, le roi passe avant les autres pièces et
//      les roques n'existent pas ;
//   3. les pions : prises vers l'ouest, puis vers l'est, puis poussées simples, puis
//      doubles ; dans chaque groupe les coups sans promotion d'abord, par case
//      d'arrivée croissante, puis les promotions, dame, tour, fou, cavalier ;
//   4. les pièces : par case de départ croissante, puis case d'arrivée croissante.
std::vector<chess::Move> movesInShakmatyOrder(const chess::Board &board);
} // namespace t64
