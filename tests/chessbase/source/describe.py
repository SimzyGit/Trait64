import chess, chess.pgn, sys
special = {"Mode quatre": "mode 4", "Mode cinq": "mode 5", "Deux octets": "twobyte 1", "Effacee": "deleted"}
res = {"1-0": 2, "1/2-1/2": 1, "0-1": 0, "*": 3}
out = []
def name(n):
    l, _, f = n.partition(", ")
    return l + "|" + f
for path in sys.argv[1:]:
    f = open(path)
    while (g := chess.pgn.read_game(f)):
        h = g.headers
        lines = [f"white {name(h['White'])}", f"black {name(h['Black'])}", f"event {h['Event']}",
                 f"site {'' if h['Site']=='?' else h['Site']}", f"result {res[h['Result']]}"]
        y, m, d = (int(x) if x.isdigit() else 0 for x in h["Date"].split("."))
        lines.append(f"date {y} {m} {d}")
        r = h.get("Round", "?")
        lines.append(f"round {int(r) if r.isdigit() else 0} 0")
        lines.append(f"elo {h.get('WhiteElo', 0)} {h.get('BlackElo', 0)}")
        if "FEN" in h: lines.append("fen " + h["FEN"])
        if h["Event"] in special: lines.append(special[h["Event"]])
        if h["Event"] == "Test variantes":
            lines += ["eco 36992", "squares 0 2 29 4 37", "arrows 0 3 13 29"]
        toks, ann, ply = [], [], [0]
        if g.comment: ann.append(f"text -1 0 0 {g.comment}")
        def uci(board, m):
            if board.is_castling(m): return "O-O" if board.is_kingside_castling(m) else "O-O-O"
            return m.uci()
        def walk(node):
            kids = node.variations
            if not kids: toks.append("E"); return
            for i, c in enumerate(kids):
                if i < len(kids) - 1: toks.append("V")
                toks.append(uci(node.board(), c.move))
                p = ply[0]; ply[0] += 1
                if c.comment: ann.append(f"text {p} 0 0 {c.comment}")
                if c.nags: ann.append(f"nag {p} {min(c.nags)} 0 0")
                walk(c)
        walk(g)
        lines.append("toks " + " ".join(toks))
        out.append("\n".join(lines + ann))
        if h["Event"] == "Effacee": out.append("guide")
print("\n\n".join(out))
