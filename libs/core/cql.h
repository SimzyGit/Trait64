// SPDX-License-Identifier: AGPL-3.0-or-later
// Un sous-ensemble du Chess Query Language (CQL 6, de Costeff et Gutman) : de quoi décrire
// une position par ses pièces, ses cases et quelques faits (échec, mat, trait), et la
// retrouver dans toutes les positions d'une bibliothèque, variantes comprises.
//
//   Ra7 Kg8 mate          une tour en a7, le roi noir en g8, et mat
//   [Qq] d4               une dame, de l'un ou l'autre camp, en d4
//   Pa-h7 wtm              un pion blanc sur la septième, les Blancs au trait
//   N attacks [qr]        un cavalier blanc attaque une dame ou une tour noire
//   move from K to g1     le dernier coup : le roi blanc est allé en g1
//   not Q not q           plus aucune dame
//   { Ra1 or Rh1 } check  les accolades groupent, « or » sépare deux possibilités
//
// Pièces : K Q R B N P (Blancs), k q r b n p (Noirs), A et a (n'importe quelle pièce
// d'un camp), _ (case vide), [..] pour un choix. Cases : e4, a-h7, [a-d1-4], [e4,d5].
// Le reste de CQL (séquences, comptes, rayons…) n'est pas lu, et le dit.
#pragma once
#include "chess.hpp"
#include <memory>
#include <string>
#include <string_view>

namespace t64::cql {
struct Filter;
class Query {
  public:
    // Lance std::invalid_argument avec un message en français si le texte n'est pas compris.
    explicit Query(std::string_view text);
    ~Query();
    Query(Query &&) noexcept;
    // `lastMove` : le coup qui a mené à la position, en UCI (vide pour la position de départ).
    bool matches(const chess::Board &board, std::string_view lastMove = {}) const;

  private:
    std::unique_ptr<Filter> root_;
};
} // namespace t64::cql
