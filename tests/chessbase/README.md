# Base ChessBase d'essai

`essai.cbh` et ses fichiers ont été écrits par l'encodeur de fixtures de
[cbvault](https://github.com/itshak/cbvault) (MIT), qui suit la description du format
sans passer par le lecteur de Trait64 : les deux lectures se contrôlent l'une l'autre.

- `source/describe.py` décrit les parties de `../scid/essai.pgn` et de `source/extra.pgn`
  (python-chess) ;
- `source/t64.rs` est l'exemple ajouté à `crates/cbvault-fixtures/examples/` qui écrit la
  base à partir de cette description ;
- `essai.pgn` est l'export de cette base par la commande `cbvault pgn`, la référence du test.

On y trouve les modes d'encodage 0, 4 et 5, des coups sur deux octets, des positions de
départ, un troisième cavalier, des variantes imbriquées, des commentaires, des NAG, des
cases et des flèches, une partie effacée et un texte, que la lecture doit passer.

Pour la refaire :

    python3 source/describe.py ../scid/essai.pgn source/extra.pgn > /tmp/essai.txt
    cargo run --release -p cbvault-fixtures --example t64 -- "$PWD" < /tmp/essai.txt
