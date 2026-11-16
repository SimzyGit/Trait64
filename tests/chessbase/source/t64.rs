//! Writes a small classic ChessBase database for Trait64's tests from a plain
//! description on stdin, one field per line, a blank line between games:
//!   white Last|First, black Last|First, event, site, date y m d, result n,
//!   round r s, elo w b, eco n, mode n, twobyte 0|1, fen <FEN>, deleted,
//!   toks <uci|O-O|O-O-O|--|V|E ...>, text pos before nation text, nag pos a b c,
//!   squares pos colour sq ..., arrows pos colour from to ..., guide (a text record).
use std::io::Read;

use cbvault_fixtures::classic::{self, Builder, Tok};
use gigachess::{Board, Color, Role};

fn main() {
    let out = std::env::args().nth(1).expect("output directory");
    let mut input = String::new();
    std::io::stdin().read_to_string(&mut input).unwrap();
    let mut b = Builder::new();
    let mut id = 0u32;
    for block in input.split("\n\n").filter(|s| !s.trim().is_empty()) {
        let field = |k: &str| block.lines().find_map(|l| l.strip_prefix(&format!("{k} ")).map(str::to_owned));
        id += 1;
        if block.trim() == "guide" {
            b.text(&[(0, b"Guide".as_slice())]);
            continue;
        }
        let fen = field("fen");
        let board = match &fen {
            Some(f) => gigachess::fen::parse_fen(f).expect("FEN"),
            None => Board::startpos(),
        };
        let words: Vec<String> = field("toks").unwrap().split_whitespace().map(str::to_owned).collect();
        let toks: Vec<Tok> = words
            .iter()
            .map(|w| match w.as_str() {
                "V" => Tok::Var,
                "E" => Tok::End,
                m => Tok::Mv(m),
            })
            .collect();
        let mode: u8 = field("mode").map_or(0, |m| m.parse().unwrap());
        let stream = classic::encode(&board, &toks, mode, field("twobyte").as_deref() == Some("1"));
        let start = fen.as_ref().map(|f| {
            let parts: Vec<&str> = f.split(' ').collect();
            let mut pieces = Vec::new();
            for (r, row) in parts[0].split('/').enumerate() {
                let mut file = 0u8;
                for c in row.chars() {
                    if let Some(n) = c.to_digit(10) {
                        file += n as u8;
                        continue;
                    }
                    let role = match c.to_ascii_lowercase() {
                        'k' => Role::King,
                        'q' => Role::Queen,
                        'r' => Role::Rook,
                        'b' => Role::Bishop,
                        'n' => Role::Knight,
                        _ => Role::Pawn,
                    };
                    let colour = if c.is_ascii_uppercase() { Color::White } else { Color::Black };
                    let sq: &'static str =
                        Box::leak(format!("{}{}", (b'a' + file) as char, 8 - r).into_boxed_str());
                    pieces.push((sq, role, colour));
                    file += 1;
                }
            }
            let mut castling = 0u8;
            for (c, bit) in [('Q', 1), ('K', 2), ('q', 4), ('k', 8)] {
                if parts[2].contains(c) {
                    castling |= bit;
                }
            }
            let ep = parts[3].bytes().next().filter(|c| c.is_ascii_lowercase()).map_or(0, |c| c - b'a' + 1);
            let mut s = classic::start_position(&pieces, parts[1] == "b", castling, ep);
            s[3] = parts[5].parse().unwrap();
            s
        });
        let flags = mode | if start.is_some() { 0x40 } else { 0 };
        let record = classic::move_record(flags, start.as_ref().map(|s| s.as_slice()), None, &stream);
        let names = |k: &str| -> (String, String) {
            let v = field(k).unwrap();
            let (l, f) = v.split_once('|').unwrap_or((&v, ""));
            (l.to_owned(), f.to_owned())
        };
        let (wl, wf) = names("white");
        let (bl, bf) = names("black");
        let white = b.player(&wl, &wf);
        let black = b.player(&bl, &bf);
        let tournament = b.tournament(&field("event").unwrap_or_default(), &field("site").unwrap_or_default());
        let mut items = Vec::new();
        for line in block.lines() {
            let mut w = line.split(' ');
            let kind = w.next().unwrap();
            let nums = |n: usize, w: &mut std::str::Split<'_, char>| -> Vec<i32> {
                (0..n).map(|_| w.next().unwrap().parse().unwrap()).collect()
            };
            match kind {
                "text" => {
                    let v = nums(3, &mut w);
                    let text: Vec<&str> = w.collect();
                    items.push(classic::text_item(v[0], v[1] == 1, v[2] as u8, &text.join(" ")));
                }
                "nag" => {
                    let v = nums(4, &mut w);
                    items.push(classic::symbols_item(v[0], v[1] as u8, v[2] as u8, v[3] as u8));
                }
                "squares" | "arrows" => {
                    let rest: Vec<i32> = w.map(|x| x.parse().unwrap()).collect();
                    if kind == "squares" {
                        let pairs: Vec<(u8, u8)> = rest[1..].chunks(2).map(|c| (c[0] as u8, c[1] as u8)).collect();
                        items.push(classic::squares_item(rest[0], &pairs));
                    } else {
                        let t: Vec<(u8, u8, u8)> =
                            rest[1..].chunks(3).map(|c| (c[0] as u8, c[1] as u8, c[2] as u8)).collect();
                        items.push(classic::arrows_item(rest[0], &t));
                    }
                }
                _ => {}
            }
        }
        let h = b.game(&record);
        h[9..12].copy_from_slice(&white.to_be_bytes()[1..]);
        h[12..15].copy_from_slice(&black.to_be_bytes()[1..]);
        h[15..18].copy_from_slice(&tournament.to_be_bytes()[1..]);
        if let Some(d) = field("date") {
            let v: Vec<u32> = d.split(' ').map(|x| x.parse().unwrap()).collect();
            h[24..27].copy_from_slice(&(v[0] << 9 | v[1] << 5 | v[2]).to_be_bytes()[1..]);
        }
        h[27] = field("result").map_or(2, |r| r.parse().unwrap());
        if let Some(r) = field("round") {
            let v: Vec<u8> = r.split(' ').map(|x| x.parse().unwrap()).collect();
            h[29] = v[0];
            h[30] = v[1];
        }
        if let Some(e) = field("elo") {
            let v: Vec<u16> = e.split(' ').map(|x| x.parse().unwrap()).collect();
            h[31..33].copy_from_slice(&v[0].to_be_bytes());
            h[33..35].copy_from_slice(&v[1].to_be_bytes());
        }
        if let Some(e) = field("eco") {
            h[35..37].copy_from_slice(&e.parse::<u16>().unwrap().to_be_bytes());
        }
        if block.lines().any(|l| l == "deleted") {
            h[0] |= 0x80;
        }
        if !items.is_empty() {
            b.annotations(&classic::annotation_items(id, &items));
        }
    }
    let db = b.write("essai");
    for ext in [".cbh", ".cbg", ".cba", ".cbp", ".cbt", ".cbc", ".cbs"] {
        std::fs::copy(db.path(ext), format!("{out}/essai{ext}")).unwrap();
    }
}
