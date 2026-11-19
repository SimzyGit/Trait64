# Bases SCID d'essai

`essai.pgn` est la source. Les deux bases en sont tirées et doivent la redonner à l'identique.

- `essai.si4`, `.sn4`, `.sg4` : écrites par SCID 4.7 (format 4).
- `essai.si5`, `.sn5`, `.sg5` : écrites par le code de SCID 5 lui-même, avec `source/make5.cpp`. Ce programme est compilé avec les fichiers de `src/` de [benini/scid](https://github.com/benini/scid) (révision `fc26337`) : `codec_scid4.cpp scidbase.cpp sortcache.cpp stored.cpp game.cpp position.cpp textbuf.cpp misc.cpp`, en C++20.

```
make5 essai.pgn essai      # écrit essai.si5, essai.sn5, essai.sg5
```

Le lecteur de Trait64 suit la description du format donnée en tête de `src/codec_scid5.h` (licence MIT, Fulvio Benini). Aucun code de SCID n'est repris dans Trait64.
