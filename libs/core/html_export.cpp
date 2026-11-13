// SPDX-License-Identifier: AGPL-3.0-or-later
#include "html_export.h"
#include <cstdio>
namespace t64 {
namespace {
std::string html(const std::string &text) {
    std::string out;
    for (const char c : text)
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out += c;
        }
    return out;
}
// Une chaîne JSON qui peut vivre dans une balise <script> : « < » y est écrit en
// échappement, pour qu'un commentaire contenant « </script> » ne ferme pas la page.
std::string json(const std::string &text) {
    std::string out = "\"";
    for (const unsigned char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += char(c);
        } else if (c == '<' || c == '>' || c == '&' || c < 0x20) {
            char buffer[8];
            std::snprintf(buffer, sizeof buffer, "\\u%04x", unsigned(c));
            out += buffer;
        } else
            out += char(c);
    }
    return out + "\"";
}
std::string tag(const Game &game, const char *name) {
    const auto found = game.tags.find(name);
    return found == game.tags.end() || found->second == "?" ? std::string() : found->second;
}
const char *const page = R"HTML(<style>
:root{color-scheme:light dark;--light:#ece0cb;--dark:#70828f;--mark:#d9b44a}
body{font:16px/1.5 system-ui,sans-serif;margin:0 auto;max-width:980px;padding:16px}
h1{font-size:1.3rem;margin:0}.meta,.foot{opacity:.7;font-size:.85rem;margin:.2rem 0 1rem}
.wrap{display:flex;flex-wrap:wrap;gap:20px;align-items:flex-start}
#board{display:grid;grid-template:repeat(8,1fr)/repeat(8,1fr);width:min(92vw,440px);aspect-ratio:1;border:1px solid #8888;user-select:none}
#board div{display:flex;align-items:center;justify-content:center;font-size:min(9.5vw,46px);line-height:1}
#board .l{background:var(--light)}#board .d{background:var(--dark)}#board .m{box-shadow:inset 0 0 0 4px var(--mark)}
#board .w{color:#fff;text-shadow:0 0 2px #000,0 0 2px #000,0 0 1px #000}#board .b{color:#111}
#side{flex:1 1 300px;min-width:260px}#moves{line-height:1.9}
#moves span{cursor:pointer;padding:1px 4px;border-radius:4px}#moves span.on{background:var(--mark);color:#111}
#moves .v{opacity:.8;font-size:.92em}#note{min-height:3em;margin-top:.8rem;padding:.5rem .7rem;border-left:3px solid var(--mark)}
button{font:inherit;padding:.25rem .7rem;margin:.6rem .3rem 0 0}
</style>
<div class="wrap"><div><div id="board"></div>
<button id="first" aria-label="Début">⏮</button><button id="back" aria-label="Coup précédent">◀</button><button id="next" aria-label="Coup suivant">▶</button><button id="last" aria-label="Fin">⏭</button><button id="flip" aria-label="Retourner">⇅</button></div>
<div id="side"><div id="moves"></div><p id="note"></p></div></div>
<p class="foot">Étude exportée de Trait64 — flèches ← → pour parcourir, clic sur un coup pour y aller.</p>
<script>
"use strict";
const glyph={k:"♚",q:"♛",r:"♜",b:"♝",n:"♞",p:"♟"},nag={1:"!",2:"?",3:"!!",4:"??",5:"!?",6:"?!"};
let at=0,flipped=false;
const board=document.getElementById("board"),moves=document.getElementById("moves"),note=document.getElementById("note");
function squares(fen){const out=[];for(const c of fen.split(" ")[0]){if(c==="/")continue;if(c>="1"&&c<="8")for(let i=0;i<+c;i++)out.push("");else out.push(c)}return out}
function draw(){
  const cells=squares(nodes[at].f),uci=nodes[at].u||"",mark=[uci.slice(0,2),uci.slice(2,4)];
  board.textContent="";
  for(let i=0;i<64;i++){
    const n=flipped?63-i:i,file=n%8,rank=8-Math.floor(n/8),name="abcdefgh"[file]+rank,piece=cells[n],cell=document.createElement("div");
    cell.className=((file+rank)%2?"l":"d")+(mark.includes(name)?" m":"")+(piece?(piece===piece.toUpperCase()?" w":" b"):"");
    cell.textContent=piece?glyph[piece.toLowerCase()]:"";board.appendChild(cell)}
  for(const s of moves.querySelectorAll("span"))s.classList.toggle("on",+s.dataset.n===at);
  note.textContent=nodes[at].c||"";note.style.visibility=nodes[at].c?"visible":"hidden"}
function label(id,forced){
  const parent=nodes[nodes[id].p].f.split(" "),white=parent[1]==="w";
  return (white?parent[5]+". ":forced?parent[5]+"… ":"")+nodes[id].s+(nodes[id].g||[]).map(g=>nag[g]||"").join("")}
function line(id,into,forced){
  for(;;){
    const span=document.createElement("span");span.dataset.n=id;span.textContent=label(id,forced);into.append(span," ");
    forced=!!nodes[id].c;
    const parent=nodes[nodes[id].p];
    if(parent.k[0]===id)for(const other of parent.k.slice(1)){
      const side=document.createElement("span");side.className="v";side.style.cursor="auto";side.append("( ");line(other,side,true);side.append(") ");into.append(side);forced=true}
    if(!nodes[id].k.length)return;id=nodes[id].k[0]}}
function go(n){if(n>=0&&n<nodes.length){at=n;draw()}}
if(nodes[0].k.length)line(nodes[0].k[0],moves,true);
moves.addEventListener("click",e=>{const n=e.target.dataset.n;if(n!==undefined)go(+n)});
const step={first:()=>go(0),back:()=>go(Math.max(0,nodes[at].p)),next:()=>nodes[at].k.length&&go(nodes[at].k[0]),
  last:()=>{let n=at;while(nodes[n].k.length)n=nodes[n].k[0];go(n)},flip:()=>{flipped=!flipped;draw()}};
for(const id in step)document.getElementById(id).onclick=step[id];
document.addEventListener("keydown",e=>{const k={ArrowLeft:"back",ArrowRight:"next",Home:"first",End:"last"}[e.key];if(k){step[k]();e.preventDefault()}});
draw();
</script>
)HTML";
} // namespace
std::string studyHtml(const Game &game) {
    const auto white = tag(game, "White"), black = tag(game, "Black");
    const auto title = white.empty() && black.empty() ? std::string("Étude") : white + " — " + black;
    std::string meta;
    for (const char *name : {"Event", "Site", "Date", "Result", "ECO"})
        if (const auto value = tag(game, name); !value.empty() && value != "????.??.??" && value != "*")
            meta += (meta.empty() ? "" : " · ") + value;
    std::string out = "<!doctype html>\n<html lang=\"fr\">\n<meta charset=\"utf-8\">\n"
                      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n<title>" +
                      html(title) + "</title>\n<h1>" + html(title) + "</h1>\n<p class=\"meta\">" +
                      html(meta) + "</p>\n<script>const nodes=[";
    for (size_t i = 0; i < game.nodes.size(); ++i) {
        const auto &node = game.nodes[i];
        // Les directives graphiques ([%cal …], [%eval …]) sont du PGN, pas du texte à lire.
        std::string comment;
        for (size_t at = 0; at < node.comment.size(); ++at) {
            if (node.comment.compare(at, 2, "[%") == 0) {
                const auto end = node.comment.find(']', at);
                if (end == std::string::npos)
                    break;
                at = end;
                continue;
            }
            comment += node.comment[at];
        }
        while (!comment.empty() && comment.front() == ' ')
            comment.erase(comment.begin());
        out += (i ? ",\n" : "\n") + std::string("{p:") + std::to_string(node.parent) +
               ",s:" + json(node.san) + ",u:" + json(node.uci) + ",f:" + json(node.fen) +
               ",c:" + json(comment) + ",k:[";
        for (size_t k = 0; k < node.children.size(); ++k)
            out += (k ? "," : "") + std::to_string(node.children[k]);
        out += "],g:[";
        for (size_t k = 0; k < node.nags.size(); ++k)
            out += (k ? "," : "") + std::to_string(node.nags[k]);
        out += "]}";
    }
    return out + "\n];</script>\n" + page + "</html>\n";
}
} // namespace t64
