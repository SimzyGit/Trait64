import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Native

ApplicationWindow {
    id: win
    width: Math.max(minimumWidth,Math.min(1440,Screen.desktopAvailableWidth)); height: Math.max(minimumHeight,Math.min(940,Screen.desktopAvailableHeight))
    minimumWidth: 960; minimumHeight: 640
    visible: true
    title: "Trait64 — " + controller.state.title + (controller.state.dirty ? " •" : "")
    onClosing: function(close) { if(controller.updater.state.status==="installing") close.accepted=false }
    // Drop a PGN, FEN, EPD or T64 file anywhere in the window to import it.
    DropArea {
        id: fileDrop; objectName: "fileDrop"; anchors.fill: parent; z: 1000
        readonly property var accepted: /\.(pgn|fen|epd|t64)$/i
        function firstFile(urls) { for (var i = 0; i < urls.length; ++i) if (accepted.test(urls[i].toString())) return urls[i].toString(); return "" }
        onEntered: function(drag) { drag.accepted = drag.hasUrls && firstFile(drag.urls) !== "" && !controller.busy }
        onDropped: function(drop) {
            var file = firstFile(drop.urls)
            if (file === "") return
            drop.acceptProposedAction()
            labelImport.ask(file)
        }
        Rectangle {
            anchors.fill: parent; visible: fileDrop.containsDrag; color: Qt.rgba(0, 0, 0, .35)
            Label { anchors.centerIn: parent; text: qsTr("Déposer pour importer"); color: "white"; font.pixelSize: 22; font.bold: true }
        }
    }
    Updates {id:updatesDialog;objectName:"updatesDialog";parent:Overlay.overlay;backend:controller.updater;app:controller}
    property int page: 0
    property bool light: controller.preferences.lightTheme
    property color bg: controller.preferences.background
    property color panel: controller.preferences.panel
    property color raised: controller.preferences.raised
    property color line: controller.preferences.line
    property color fg: controller.preferences.foreground
    property color muted: controller.preferences.muted
    property color accent: controller.preferences.accentColor
    property bool exportAll: false
    property string promotionPrefix: ""
    property int selectedGame: 0
    property int selectedMedals: 0
    // Les clés stratégiques (themes.h), dans l'ordre de leurs bits.
    readonly property var themeKeys: ["isolatedQueenPawn","hangingPawns","advancedPassedPawn","doubledPawns","oppositeBishops","bishopPair","carlsbad","maroczy","stonewall","queenlessMiddlegame"]
    readonly property var themeNames: [qsTr("Pion isolé en d"), qsTr("Pions pendants"), qsTr("Pion passé avancé"), qsTr("Pions doublés"), qsTr("Fous de couleurs opposées"), qsTr("Paire de fous"), qsTr("Structure Carlsbad"), qsTr("Étau de Maróczy"), qsTr("Stonewall"), qsTr("Milieu de jeu sans dames")]
    // Les médailles de ChessBase, un bit chacune, dans son ordre.
    readonly property var medalNames: [qsTr("Meilleure partie"), qsTr("Décisive pour le tournoi"), qsTr("Modèle d’ouverture"),
        qsTr("Nouveauté"), qsTr("Structure de pions"), qsTr("Stratégie"), qsTr("Tactique"), qsTr("Attaque"),
        qsTr("Sacrifice"), qsTr("Défense"), qsTr("Matériel"), qsTr("Jeu des pièces"), qsTr("Finale"),
        qsTr("Gaffe tactique"), qsTr("Erreur stratégique"), qsTr("Personnelle")]
    function medalList(mask) { return medalNames.filter(function(name, i) { return (mask & (1 << i)) !== 0 }).join(", ") }
    property var nav: [qsTr("Accueil"), qsTr("Bibliothèque"), qsTr("Analyse"), qsTr("Ouvertures"), qsTr("Entraînement"), qsTr("Outils"), qsTr("Joueurs"), qsTr("Profils"), qsTr("Études")]
    palette.window: bg; palette.windowText: fg; palette.base: panel; palette.text: fg
    palette.button: panel; palette.buttonText: fg; palette.mid: line
    palette.alternateBase: raised; palette.highlight: accent; palette.highlightedText: contrast(accent)
    palette.placeholderText: muted
    // Le groupe « désactivé » ne suit pas les lignes ci-dessus : en thème sombre, un bouton
    // grisé (pendant un import, par exemple) se peignait en blanc, son texte illisible.
    palette.disabled.window: bg; palette.disabled.windowText: muted; palette.disabled.base: panel; palette.disabled.text: muted
    palette.disabled.button: panel; palette.disabled.buttonText: fg; palette.disabled.mid: line; palette.disabled.alternateBase: raised
    // « light » et « dark » sont des nuances de surface, pas des couleurs de texte : le
    // thème Basic s'en sert comme fond du survol des menus. Affectés au texte, ils
    // peignaient l'entrée survolée en noir sur noir — ou en blanc sur blanc en thème
    // sombre —, rendant illisible ce que l'on désignait.
    palette.dark: Qt.darker(panel, light ? 1.12 : 1.4)
    palette.light: Qt.rgba(accent.r, accent.g, accent.b, .16)
    palette.midlight: raised
    palette.shadow: line
    palette.brightText: light ? "#ffffff" : "#101010"
    color: bg
    font.family: controller.preferences.fontFamily
    font.pixelSize: Math.round(12*textScale)
    function contrast(c){function linear(v){return v<=.04045?v/12.92:Math.pow((v+.055)/1.055,2.4)}return .2126*linear(c.r)+.7152*linear(c.g)+.0722*linear(c.b)>.179?"#171b21":"#ffffff"}
    property double selectedBaseId:0
    property var advanced:({})
    function applyAdvanced(){var f=Object.assign({},advanced);f.sourceId=selectedBaseId;f.pawnStructure=positionCheck.checked&&pawnCheck.checked;controller.searchLibrary(searchField.text,resultBox.currentIndex===0?"":resultBox.currentText,favoriteCheck.checked,positionCheck.checked,trashCheck.checked,f)}
    function filterLibrary() {applyAdvanced()}
    // L'échiquier seul : la notation et le moteur s'effacent, l'échiquier prend la place.
    property bool boardOnly:false

    // ---- Panneaux ancrés -------------------------------------------------------------
    // La disposition : trois zones (gauche, droite, sous l'échiquier), chacune une liste de
    // piles {panels, current, size}. Elle se garde dans les préférences, en JSON.
    readonly property var dockDefault: ({left: [], bottom: [],
        right: [{panels: ["notation", "tree", "book", "online"], current: 0},
                {panels: ["notes"], current: 0}, {panels: ["engine", "graph"], current: 0}]})
    // Lue une fois : liée aux préférences, elle recréerait tous les panneaux au moindre
    // réglage changé (la langue, une couleur), et chacun perdrait son défilement.
    property var dockLayout: dockDefault
    Component.onCompleted: dockLayout = parseDock(controller.preferences.dockLayout)
    property bool dockDragging: false
    property string dockDragged: ""
    property var dockHandler: null
    property var dockPending: null
    property rect dockArea: Qt.rect(0, 0, 0, 0)
    signal dockSelect(string panel)
    function parseDock(text) {
        try {
            var d = JSON.parse(text)
            if (d && (d.left || d.right || d.bottom))
                return {left: d.left || [], right: d.right || [], bottom: d.bottom || []}
        } catch (e) {}
        return JSON.parse(JSON.stringify(dockDefault))
    }
    function saveDock(d) { dockLayout = d; controller.setPreference("dockLayout", JSON.stringify(d)) }
    function copyDock() { return JSON.parse(JSON.stringify(dockLayout)) }
    function panelTitle(id) {
        return ({notation: qsTr("Notation"), tree: qsTr("Arbre"), book: qsTr("Livre"), online: qsTr("Lichess"),
                 notes: qsTr("Notes"), graph: qsTr("Courbe"), engine: qsTr("Moteur")})[id] || id
    }
    function panelComponent(id) {
        return ({notation: panelNotation, tree: panelTree, book: panelBook, online: panelOnline,
                 notes: panelNotes, graph: panelGraph, engine: panelEngine})[id] || null
    }
    function panelHeight(id) { return Math.round((({notes: 104, graph: 140, engine: 200})[id] || 280) * win.textScale) }
    function detachable(id) { return id === "notation" || id === "graph" }
    function detachName(id) { return id }
    function locate(d, id) {
        for (var zone of ["left", "right", "bottom"])
            for (var i = 0; i < d[zone].length; ++i) {
                var at = d[zone][i].panels.indexOf(id)
                if (at >= 0) return {zone: zone, stack: i, index: at}
            }
        return null
    }
    function removePanel(d, id) {
        var at = locate(d, id)
        if (!at) return
        var s = d[at.zone][at.stack]
        s.panels.splice(at.index, 1)
        s.current = Math.max(0, Math.min(s.current || 0, s.panels.length - 1))
        if (!s.panels.length) d[at.zone].splice(at.stack, 1)
    }
    // L'onglet choisi et la taille d'une pile changent sans tout redessiner : l'objet est
    // modifié en place, puis seulement enregistré.
    function rememberCurrent(zone, position, index) {
        var s = dockLayout[zone] && dockLayout[zone][position]
        if (!s) return
        s.current = index
        controller.setPreference("dockLayout", JSON.stringify(dockLayout))
    }
    function rememberSizes(zone, column) {
        var list = dockLayout[zone] || []
        for (var i = 0; i < list.length && i < column.count; ++i) {
            var item = column.itemAt(i)
            if (item) list[i].size = Math.round(column.orientation === Qt.Vertical ? item.height : item.width)
        }
        controller.setPreference("dockLayout", JSON.stringify(dockLayout))
    }
    function selectPanel(id) {
        var at = locate(dockLayout, id)
        if (!at) return
        rememberCurrent(at.zone, at.stack, at.index)
        dockSelect(id)
    }
    function closePanel(id) { var d = copyDock(); removePanel(d, id); saveDock(d) }
    function showPanel(id) {
        if (locate(dockLayout, id)) { win.page = 2; selectPanel(id); return }
        var d = copyDock()
        d.right.push({panels: [id], current: 0})
        saveDock(d)
        win.page = 2
    }
    function togglePanel(id) { if (locate(dockLayout, id)) closePanel(id); else showPanel(id) }
    function resetDock() { saveDock(JSON.parse(JSON.stringify(dockDefault))) }
    function beginDockDrag(id, handler) {
        var r = analysisSplit.mapToItem(dockOverlay, 0, 0)
        dockArea = Qt.rect(r.x, r.y, analysisSplit.width, analysisSplit.height)
        dockDragged = id; dockHandler = handler; dockPending = null; dockDragging = true
    }
    function endDockDrag() {
        if (!dockDragging) return
        dockGhost.Drag.drop()
        dockDragging = false; dockHandler = null
        // Appliqué après coup : l'onglet qu'on lâche appartient à une pile que le
        // déplacement va recréer, et elle ne doit pas disparaître dans son propre signal.
        Qt.callLater(commitDock)
    }
    function dropPanel(zone, position, where) { dockPending = {id: dockDragged, zone: zone, position: position, where: where} }
    function commitDock() {
        var p = dockPending
        dockPending = null; dockDragged = ""
        if (!p) return
        var d = copyDock(), list = d[p.zone], target = p.position >= 0 ? list[p.position] : null
        if (target && target.panels.indexOf(p.id) >= 0 && (p.where === "tab" || target.panels.length === 1)) return
        if (target) target.mark = true
        removePanel(d, p.id)
        var index = -1
        for (var i = 0; i < list.length; ++i) if (list[i].mark) { index = i; delete list[i].mark }
        if (p.where === "tab" && index >= 0) {
            list[index].panels.push(p.id)
            list[index].current = list[index].panels.length - 1
        } else {
            if (index < 0) index = p.where === "start" ? 0 : list.length
            else if (p.where === "after") index += 1
            for (var j = 0; j < list.length; ++j) delete list[j].size
            list.splice(index, 0, {panels: [p.id], current: 0})
        }
        saveDock(d)
    }

    // Les panneaux de l'analyse. Chacun vit dans une pile (DockStack.qml) que l'on place
    // à la souris ; ces composants en sont le contenu, créé là où le panneau est rangé.
    Component {id:panelNotation
        Item {
                                    MoveNotebook {anchors.fill:parent;anchors.margins:6;
                                        backend: controller; textScale: win.textScale; outerViewport: analysisViewport
                                        foreground: win.fg; muted: win.muted; accent: win.accent
                                        raised: win.raised; lineColor: win.line
                                    }
        }
    }
    Component {id:panelTree
        Item {
                                    // Opening tree: move, games, result bar (White / draws / Black), score.
                                    ListView {id:miniExplorer;anchors.fill:parent;anchors.margins:6;objectName:"openingTree";model:controller.openings;clip:true;spacing:0
                                        // Les parties qui passent par ici, les mieux classées d'abord : la
                                        // bibliothèque filtrée sur la position, triée par Elo.
                                        // En finale, le score de ce matériel dans la bibliothèque, quelle
                                        // que soit la place des pièces.
                                        footer:Column {width:miniExplorer.width
                                            Label {objectName:"endgameOdds";width:parent.width;padding:6;wrapMode:Text.WordWrap;color:win.muted
                                                readonly property var odds:controller.state.endgame||({})
                                                visible:(odds.games||0)>0
                                                text:qsTr("Ce matériel dans la bibliothèque : %1 partie(s) · Blancs %2 % · nulles %3 % · Noirs %4 %").arg(odds.games||0)
                                                    .arg(Math.round(100*(odds.white||0)/Math.max(1,odds.games||0))).arg(Math.round(100*(odds.draws||0)/Math.max(1,odds.games||0)))
                                                    .arg(Math.round(100*(odds.black||0)/Math.max(1,odds.games||0)))}
                                            // Les clés stratégiques de la position.
                                            Label {objectName:"positionThemes";width:parent.width;leftPadding:6;rightPadding:6;bottomPadding:6;wrapMode:Text.WordWrap;color:win.muted
                                                readonly property var keys:controller.state.themes||[]
                                                visible:keys.length>0
                                                text:qsTr("Clés stratégiques : %1").arg(keys.map(function(k){return win.themeNames[win.themeKeys.indexOf(k)]||k}).join(" · "))}
                                            // La tendance : le score des Blancs selon l'écart d'Elo, quand il est connu.
                                            Label {objectName:"endgameTrend";width:parent.width;leftPadding:6;rightPadding:6;bottomPadding:6;wrapMode:Text.WordWrap;color:win.muted
                                                readonly property var rows:(controller.state.endgame||{}).byElo||[]
                                                readonly property var names:[qsTr("Blancs plus forts"),qsTr("Elo proches"),qsTr("Noirs plus forts")]
                                                visible:rows.some(function(r){return r.games>0})
                                                text:qsTr("Score des Blancs selon l’Elo : %1").arg(rows.map(function(r,i){return r.games>0?names[i]+" "+r.score+" % ("+r.games+")":""}).filter(function(t){return t}).join(" · "))
                                                ToolTip.visible:trendHover.hovered;ToolTip.delay:500
                                                ToolTip.text:qsTr("« Plus forts » : au moins 100 points d’Elo d’avance. Les parties sans Elo ne comptent pas ici.")
                                                HoverHandler {id:trendHover}}
                                            TButton {objectName:"bestGamesHere";width:parent.width;flat:true;text:qsTr("Meilleures parties de cette position")+"  →";onClicked:win.bestGamesHere()}
                                            TButton {objectName:"plansHere";width:parent.width;flat:true;text:qsTr("Plans de cette structure")+"  →";onClicked:plansDialog.open()}
                                            TButton {objectName:"similarHere";width:parent.width;flat:true;text:qsTr("Positions semblables")+"  →";onClicked:similarDialog.open()}
                                        }
                                        header:RowLayout {width:miniExplorer.width;spacing:8
                                            Label {text:qsTr("Coup");Layout.preferredWidth:58;color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                                            Label {text:qsTr("Parties");Layout.preferredWidth:58;horizontalAlignment:Text.AlignRight;color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                                            Label {text:qsTr("Blancs · nulles · Noirs");Layout.fillWidth:true;color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                                            Label {text:qsTr("Score");Layout.preferredWidth:44;horizontalAlignment:Text.AlignRight;color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                                        }
                                        delegate:ItemDelegate {
                                            id:treeRow
                                            required property var modelData
                                            readonly property real total:Math.max(1,modelData.white+modelData.draw+modelData.black)
                                            width:miniExplorer.width;padding:3;onClicked:controller.move(modelData.uci)
                                            Accessible.name:modelData.san+", "+modelData.count+qsTr(" partie(s)")
                                            contentItem:RowLayout {spacing:8
                                                Label {text:modelData.san;Layout.preferredWidth:58;font.weight:Font.DemiBold}
                                                Label {text:modelData.count;Layout.preferredWidth:58;horizontalAlignment:Text.AlignRight}
                                                Row {Layout.fillWidth:true;height:10;Layout.alignment:Qt.AlignVCenter
                                                    Rectangle {width:parent.width*treeRow.modelData.white/treeRow.total;height:10;color:"#f4f4f1";border.color:win.line}
                                                    Rectangle {width:parent.width*treeRow.modelData.draw/treeRow.total;height:10;color:"#a3a8ad"}
                                                    Rectangle {width:parent.width*treeRow.modelData.black/treeRow.total;height:10;color:"#2c3035"}
                                                }
                                                Label {text:Math.round(100*(treeRow.modelData.white+treeRow.modelData.draw/2)/treeRow.total)+" %";Layout.preferredWidth:44;horizontalAlignment:Text.AlignRight;color:win.muted}
                                            }
                                        }
                                        Label {anchors.centerIn:parent;width:parent.width-16;wrapMode:Text.WordWrap;horizontalAlignment:Text.AlignHCenter;visible:controller.openings.length===0;text:win.pastOpeningIndex()?qsTr("L’arbre couvre les 20 premiers coups. Au-delà, la recherche de position retrouve les parties."):qsTr("Aucune suite dans votre base.");color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                                    }
        }
    }
    Component {id:panelBook
        Item {
                                    ListView {id:bookList;anchors.fill:parent;anchors.margins:6;objectName:"bookMoves";model:controller.bookMoves;clip:true;spacing:0
                                        delegate:ItemDelegate {
                                            required property var modelData
                                            width:bookList.width;padding:3;onClicked:controller.move(modelData.uci)
                                            Accessible.name:modelData.san+", "+modelData.percent+qsTr(" % du livre")
                                            contentItem:RowLayout {spacing:8
                                                Label {text:modelData.san;Layout.preferredWidth:58;font.weight:Font.DemiBold}
                                                Rectangle {Layout.preferredWidth:Math.max(2,140*modelData.percent/100);height:8;color:win.accent;opacity:.7}
                                                Label {text:modelData.percent+" %";color:win.muted;Layout.fillWidth:true}
                                            }
                                        }
                                        Label {anchors.centerIn:parent;width:parent.width-24;horizontalAlignment:Text.AlignHCenter;wrapMode:Text.Wrap;visible:controller.bookMoves.length===0
                                            text:controller.preferences.bookPath?qsTr("Position absente du livre."):qsTr("Aucun livre Polyglot (.bin) : indiquez-le dans Paramètres.")
                                            color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                                    }
        }
    }
    Component {id:panelOnline
        Item {
            OnlineExplorer {objectName:"onlineExplorer";backend:controller;anchors.fill:parent;anchors.margins:6
                            active:!!parent.parent&&!!parent.parent.panelActive&&win.page===2}
        }
    }
    Component {id:panelNotes
        Item {
            ColumnLayout {anchors.fill:parent;spacing:0
                                RowLayout {Layout.fillWidth:true;Layout.margins:4;spacing:4;visible:!controller.state.training
                                    TComboBox {id:nagBox;model:["Annotation","! Bon coup","? Erreur","!! Excellent","?? Gaffe","!? Intéressant","?! Douteux"];currentIndex:Math.min(controller.state.nag,6);onActivated:controller.setNag(currentIndex);Layout.fillWidth:true;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                                    TButton {text:"↑";enabled:controller.state.node>0;onClicked:controller.promote();ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Promouvoir en ligne principale")}
                                }
                                ScrollView {Layout.fillWidth:true;Layout.fillHeight:true;Layout.leftMargin:4;Layout.rightMargin:4;Layout.bottomMargin:4
                                    TextArea {id:note;objectName:"positionNote";text:controller.state.comment;readOnly:controller.state.training;placeholderText:qsTr("Une idée, un plan, une question…");wrapMode:TextEdit.Wrap;font.pixelSize:Math.round(12*controller.preferences.fontScale*controller.preferences.uiScale);selectByMouse:true
                                        onTextChanged:if(activeFocus&&text!==controller.state.comment)controller.setComment(text)
                                    }
                                }
            }
        }
    }
    Component {id:panelGraph
        Item {
                            ColumnLayout {anchors.fill:parent;anchors.margins:6;spacing:4;visible:controller.review.length>0
                                RowLayout {Layout.fillWidth:true;spacing:6
                                    Label {text:qsTr("Évaluations du point de vue des Blancs");elide:Text.ElideRight;Layout.fillWidth:true;color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                                    TButton {text:qsTr("Détails");flat:true;onClicked:reviewDialog.open();ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Ouvrir le bilan complet : précision par phase, fautes expliquées, exercices.")}
                                }
                                EvalGraph {Layout.fillWidth:true;Layout.fillHeight:true;backend:controller}
                            }
            Label {anchors.centerIn:parent;width:parent.width-24;visible:controller.review.length===0;wrapMode:Text.Wrap
                   horizontalAlignment:Text.AlignHCenter;color:win.muted
                   text:qsTr("La courbe paraît après le bilan de la partie (bouton Bilan).")}
        }
    }
    Component {id:panelEngine
        Item {
                            ColumnLayout {anchors.fill:parent;anchors.margins:6;spacing:4
                                RowLayout {Layout.fillWidth:true;spacing:6
                                    Label {text:win.systemText(controller.engineStatus)+(controller.state.opening?" · "+controller.state.opening:"");elide:Text.ElideRight;Layout.fillWidth:true;color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                                    // Sans moteur configuré, « Analyser » ne mène nulle part et rien
                                    // ne dit où en choisir un : premier mur venu pour qui découvre.
                                    TButton {visible:!controller.preferences.enginePath;text:qsTr("Choisir un moteur…");accent:true
                                             // D'abord chercher : la plupart du temps Stockfish est déjà là.
                                             onClicked:{if(controller.useDetectedEngine()==="")settingsDialog.open()}
                                             ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Stockfish depuis votre distribution, ou un moteur du catalogue dans Bibliothèque → Téléchargements.")}
                                    TButton {objectName:"toggleThreat";visible:!!controller.preferences.enginePath;text:qsTr("Menace");accent:!!controller.state.threat;enabled:!controller.state.training&&!controller.state.playing;onClicked:controller.toggleThreat();ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Ce que jouerait l’adversaire si vous passiez votre tour (touche M).")}
                                    TButton {objectName:"toggleEngineAnalysis";visible:!!controller.preferences.enginePath;text:controller.state.analysis?qsTr("Arrêter"):qsTr("Analyser");accent:!controller.state.analysis;enabled:!controller.state.training;onClicked:controller.toggleAnalysis()}
                                }
                                ListView {
                                    Layout.fillWidth:true;Layout.fillHeight:true;visible:controller.state.analysis;clip:true;model:controller.engineLines;spacing:2
                                    delegate:RowLayout {
                                        required property var modelData
                                        required property int index
                                        width:ListView.view.width;spacing:8
                                        // L'évaluation en pastille, claire si les Blancs sont mieux, sombre sinon, comme
                                        // sur Lichess : le signe se lit avant le chiffre.
                                        Rectangle {Layout.preferredWidth:Math.round(56*win.textScale);Layout.preferredHeight:Math.round(20*win.textScale);radius:3
                                            readonly property bool black:String(modelData.eval).indexOf("-")===0||String(modelData.eval).indexOf("#-")===0
                                            color:black?"#2a2926":"#f4f1ea";border.color:win.line
                                            Label {anchors.centerIn:parent;text:modelData.eval;font.weight:Font.DemiBold;color:parent.black?"#f4f1ea":"#2a2926"
                                                   font.pixelSize:Math.round(12*win.textScale)}}
                                        // Gain, nulle, perte selon le moteur (vus des Blancs), comme dans
                                        // Nibbler : une évaluation de +1 ne dit pas si la nulle reste probable.
                                        Item {objectName:"engineWdl";visible:!!modelData.wdl;Layout.preferredWidth:44;Layout.preferredHeight:8
                                            readonly property var w:modelData.wdl||[0,1000,0]
                                            Row {anchors.fill:parent
                                                Rectangle {width:parent.width*parent.parent.w[0]/1000;height:parent.height;color:"#f4f1ea"}
                                                Rectangle {width:parent.width*parent.parent.w[1]/1000;height:parent.height;color:"#9a958b"}
                                                Rectangle {width:parent.width*parent.parent.w[2]/1000;height:parent.height;color:"#2a2926"}}
                                            Rectangle {anchors.fill:parent;color:"transparent";border.color:win.line;radius:1}
                                            HoverHandler {id:wdlHover}
                                            ToolTip.visible:wdlHover.hovered;ToolTip.delay:300
                                            ToolTip.text:qsTr("Blancs %1 % · nulle %2 % · Noirs %3 % (estimation du moteur)").arg(Math.round(w[0]/10)).arg(Math.round(w[1]/10)).arg(Math.round(w[2]/10))}
                                        Label {text:modelData.san;elide:Text.ElideRight;Layout.fillWidth:true}
                                        Label {text:"d"+modelData.depth+" · "+Math.round(Number(modelData.nps)/1000)+" kN/s";color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                                        TButton {text:"+";flat:true;implicitWidth:24;implicitHeight:22;onClicked:controller.addEngineLine(index);ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Ajouter cette ligne à l’étude")}
                                    }
                                }
                                Label {Layout.fillWidth:true;Layout.fillHeight:true;visible:!controller.state.analysis&&!!controller.preferences.enginePath
                                       verticalAlignment:Text.AlignTop;topPadding:6;wrapMode:Text.Wrap;color:win.muted;font.pixelSize:Math.round(11*win.textScale)
                                       text:qsTr("« Analyser » lance le moteur sur la position affichée ; il suit chaque coup joué ou parcouru.")}
                            }
        }
    }
    // Les panneaux détachés : une fenêtre par demande, détruite à sa fermeture.
    function detach(panel) {
        var component = Qt.createComponent("PanelWindow.qml")
        var w = component.createObject(win, {backend: controller, panel: panel})
        if (w) w.show()
        return w
    }
    // Une disposition : la place de l'échiquier, les hauteurs du panneau de droite,
    // l'échiquier seul ou non, et l'onglet de notation ouvert.
    function currentLayout(name) {
        return {name: name, ratio: boardPanel.width / Math.max(1, analysisSplit.width),
                dock: JSON.stringify(dockLayout), boardOnly: win.boardOnly}
    }
    function applyLayout(layout) {
        win.page = 2
        win.boardOnly = !!layout.boardOnly
        controller.setPreference("analysisPanelRatio", layout.ratio)
        if (layout.dock) saveDock(parseDock(layout.dock))
    }
    // Le coup fautif sous les yeux, d'après le bilan : relu à chaque coup et à chaque bilan.
    readonly property var reviewSpot:(controller.review,controller.reviewMarks(controller.state.node))
    function toggleFullScreen(){visibility=visibility===Window.FullScreen?Window.Windowed:Window.FullScreen}
    function bestGamesHere(){page=1;positionCheck.checked=true;pawnCheck.checked=false;trashCheck.checked=false;var f=Object.assign({},advanced);f.sort="elo";f.reverse=false;advanced=f;applyAdvanced()}
    // Une recherche enregistrée : tout ce que la bibliothèque filtre, sous un nom.
    function currentSearch(){return {text:searchField.text,result:resultBox.currentIndex,favorite:favoriteCheck.checked,position:positionCheck.checked,pawns:pawnCheck.checked,trash:trashCheck.checked,filters:Object.assign({},advanced)}}
    function applySearch(s){page=1;searchField.text=s.text||"";resultBox.currentIndex=s.result||0;favoriteCheck.checked=!!s.favorite;positionCheck.checked=!!s.position;pawnCheck.checked=!!s.pawns;trashCheck.checked=!!s.trash;advanced=Object.assign({},s.filters||{});searchTimer.stop();applyAdvanced()}
    function sortLibrary(key){var f=Object.assign({},advanced);f.reverse=(f.sort||"updated")===key?!f.reverse:false;f.sort=key;advanced=f;applyAdvanced()}
    Connections {target:controller;function onLibraryOpened(){win.selectedBaseId=0;win.advanced=({});baseBox.currentIndex=0;win.filterLibrary()}}
    function openExport(all) {exportAll=all;exportDialog.open()}

    property real uiScale: controller.preferences.uiScale
    property real textScale: controller.preferences.fontScale * uiScale
    property string uiLanguage: controller.preferences.language
    onUiLanguageChanged: controller.retranslateUi(win)
    function systemText(text) { var language=controller.preferences.language; return controller.uiText(text) }
    Rectangle {anchors.fill: parent; color: win.bg}
    // Classic desktop structure: menu bar, a slim row of workspace tabs, then dense panes.
    menuBar: MenuBar {
        objectName: "mainMenuBar"
        font.pixelSize: Math.round(12*win.textScale)
        // La barre de menus prend le ton du fond, avec un filet dessous : en blanc pur, elle
        // tranchait sur le papier du thème clair comme une bande rapportée.
        background: Rectangle {
            implicitHeight: 28*win.uiScale
            color: win.bg
            Rectangle {anchors.bottom:parent.bottom;width:parent.width;height:1;color:win.line;opacity:.6}
        }
        Menu { title: qsTr("&Fichier")
            MenuItem {text: qsTr("Nouvelle étude"); onTriggered: controller.newGame()}
            MenuItem {text: qsTr("Importer des parties…"); onTriggered: importDialog.open()}
            MenuItem {text: qsTr("Comptes Lichess et Chess.com…"); onTriggered: accountsDialog.open()}
            MenuItem {text: qsTr("Bilan de vos compétences…"); onTriggered: skillDialog.open()}
            MenuItem {text: qsTr("Imports PGN reprenables…"); onTriggered: importsDialog.open()}
            MenuSeparator {}
            MenuItem {text: qsTr("Enregistrer l’étude"); enabled: !controller.state.training; onTriggered: controller.saveGame()}
            MenuItem {text: qsTr("Exporter la bibliothèque…"); onTriggered: win.openExport(true)}
            MenuItem {text: qsTr("Exporter un livre Polyglot…"); onTriggered: polyglotDialog.open()}
            MenuSeparator {}
            MenuItem {text: qsTr("Ouvrir une bibliothèque…"); onTriggered: openDatabaseDialog.open()}
            MenuItem {text: qsTr("Sauvegarde complète…"); onTriggered: backupDialog.open()}
            MenuItem {text: qsTr("Restaurer une sauvegarde…"); onTriggered: backupsDialog.open()}
            MenuSeparator {}
            MenuItem {text: qsTr("Quitter"); onTriggered: win.close()}
        }
        Menu { title: qsTr("É&dition")
            MenuItem {text: qsTr("Annuler"); enabled: controller.state.canUndo; onTriggered: controller.undo()}
            MenuItem {text: qsTr("Rétablir"); enabled: controller.state.canRedo; onTriggered: controller.redo()}
            MenuSeparator {}
            MenuItem {text: qsTr("Copier la FEN"); onTriggered: controller.copyText("fen")}
            MenuItem {text: qsTr("Ouvrir sur un téléphone (QR)…"); onTriggered: positionQr.open()}
            MenuItem {text: qsTr("Copier le PGN"); onTriggered: controller.copyText("pgn")}
            MenuItem {text: qsTr("Charger une FEN…"); onTriggered: fenDialog.open()}
            MenuItem {text: qsTr("Éditer la position…"); onTriggered: positionEditor.open()}
        }
        Menu { title: qsTr("&Partie")
            MenuItem {text: qsTr("Informations de la partie…"); onTriggered: tagsDialog.open()}
            MenuItem {text: controller.state.playing ? qsTr("Arrêter la partie") : qsTr("Jouer une partie…"); onTriggered: controller.state.playing ? controller.stopPlaying() : playDialog.open()}
            MenuItem {text: qsTr("Nouvelle partie Chess960…"); onTriggered: chess960Dialog.open()}
            MenuSeparator {}
            MenuItem {text: qsTr("Bilan de la partie…"); enabled: !controller.state.training; onTriggered: reviewDialog.open()}
            MenuItem {text: qsTr("Chercher la nouveauté"); onTriggered: controller.findNovelty()}
            MenuItem {text: qsTr("Identifier l’ouverture"); onTriggered: controller.identifyOpening()}
            MenuItem {text: qsTr("Tableau du tournoi"); enabled: controller.state.savedId > 0; onTriggered: crosstableDialog.show(controller.state.savedId)}
            MenuSeparator {}
            MenuItem {text: qsTr("Exporter le diagramme…"); onTriggered: diagramDialog.open()}
            MenuItem {text: qsTr("Exporter l’étude en PDF…"); onTriggered: pdfOptions.open()}
        }
        Menu { title: qsTr("&Moteur")
            MenuItem {text: controller.state.analysis ? qsTr("Arrêter l’analyse") : qsTr("Analyser la position"); enabled: !controller.state.training; onTriggered: controller.toggleAnalysis()}
            MenuItem {text: qsTr("Analyses en arrière-plan…"); onTriggered: analysisQueueDialog.open()}
            MenuItem {text: qsTr("Comparer plusieurs moteurs…"); onTriggered: compareDialog.open()}
            MenuItem {text: qsTr("Tournoi de moteurs…"); onTriggered: tournamentDialog.open()}
            MenuSeparator {}
            MenuItem {text: qsTr("Moteurs et bases à télécharger…"); onTriggered: downloadsDialog.open()}
            MenuItem {text: qsTr("Paramètres du moteur…"); onTriggered: settingsDialog.open()}
        }
        Menu { title: qsTr("&Base")
            MenuItem {text: qsTr("Bibliothèque"); onTriggered: win.page = 1}
            MenuItem {text: qsTr("Recherche avancée…"); onTriggered: {win.page = 1; advancedDialog.open()}}
            MenuItem {text: qsTr("Doublons de coups"); onTriggered: {win.page = 1; controller.findDuplicates()}}
            MenuItem {text: qsTr("Fusionner les doublons"); enabled: !controller.busy; onTriggered: {win.page = 1; controller.mergeDuplicates()}}
            MenuItem {text: qsTr("Comparer avec une autre base…"); onTriggered: compareLibraryDialog.open()}
            MenuItem {text: qsTr("Tournois de la bibliothèque…"); onTriggered: tournamentFinder.open()}
            MenuItem {text: qsTr("Graphiques de la sélection…"); onTriggered: libraryCharts.open()}
            MenuItem {text: qsTr("Supprimer la base…"); enabled: win.selectedBaseId > 0; onTriggered: removeBaseDialog.ask(win.selectedBaseId)}
            MenuItem {text: qsTr("Fusionner les parties affichées en une étude"); enabled: controller.games.length > 1; onTriggered: controller.mergeShownGames()}
            MenuItem {text: qsTr("Tournois en direct…"); onTriggered: liveDialog.open()}
            MenuSeparator {}
            MenuItem {text: qsTr("Dossier de la position…"); onTriggered: openingReportDialog.open()}
            MenuItem {text: qsTr("Préparation par position…"); onTriggered: preparationDialog.open()}
            MenuItem {text: qsTr("Mes répertoires…"); onTriggered: repertoiresDialog.open()}
            MenuItem {text: qsTr("Joueurs"); onTriggered: win.page = 6}
        }
        Menu { title: qsTr("&Entraînement")
            MenuItem {text: qsTr("Puzzles tactiques…"); onTriggered: puzzlesDialog.open()}
            MenuItem {text: qsTr("Coordonnées…"); onTriggered: coordinatesDialog.open()}
            MenuItem {text: qsTr("Finales à jouer…"); onTriggered: endgamesDialog.open()}
            MenuItem {text: qsTr("Deviner les coups des Blancs"); enabled: !controller.busy; onTriggered: controller.startGuessing(true)}
            MenuItem {text: qsTr("Deviner les coups des Noirs"); enabled: !controller.busy; onTriggered: controller.startGuessing(false)}
            MenuItem {text: qsTr("Coup à trouver (question d’entraînement)"); checkable: true; checked: !!controller.state.question; enabled: !controller.state.training && controller.state.node > 0; onTriggered: controller.toggleQuestion()}
            MenuItem {text: qsTr("Répondre aux questions de l’étude"); enabled: !controller.busy; onTriggered: controller.startQuestions()}
            MenuItem {text: qsTr("Vérifier mes variantes (exercice de Stoyko)…"); onTriggered: stoykoDialog.open()}
            MenuItem {text: qsTr("Réviser la prochaine carte"); enabled: (controller.stats.due||0) > 0; onTriggered: controller.nextDueCard()}
            MenuItem {text: qsTr("Réviser un répertoire…"); onTriggered: repertoiresDialog.open()}
            MenuItem {text: qsTr("Tableau d’entraînement"); onTriggered: win.page = 4}
        }
        Menu { title: qsTr("&Affichage")
            MenuItem {text: win.light ? qsTr("Thème sombre") : qsTr("Thème clair"); onTriggered: {controller.setPreference("lightTheme", !win.light); controller.setPreference("backgroundColor", "")}}
            MenuItem {text: qsTr("Pièces, échiquier et couleurs…"); onTriggered: appearanceDialog.open()}
            MenuItem {text: qsTr("Échiquier seul"); checkable: true; checked: win.boardOnly; onTriggered: {win.boardOnly = checked; win.page = 2}}
            Menu { title: qsTr("Détacher dans une fenêtre")
                MenuItem {objectName: "detachBoard"; text: qsTr("Échiquier"); onTriggered: win.detach("board")}
                MenuItem {text: qsTr("Notation"); onTriggered: win.detach("notation")}
                MenuItem {text: qsTr("Courbe du bilan"); onTriggered: win.detach("graph")}
            }
            Menu { title: qsTr("Panneaux")
                Repeater {
                    model: ["notation", "tree", "book", "online", "notes", "engine", "graph"]
                    MenuItem {required property string modelData
                        text: win.panelTitle(modelData); checkable: true
                        checked: !!win.locate(win.dockLayout, modelData); onTriggered: win.togglePanel(modelData)}
                }
                MenuSeparator {}
                MenuItem {objectName: "resetDock"; text: qsTr("Disposition d’origine"); onTriggered: win.resetDock()}
            }
            MenuItem {text: qsTr("Dispositions de travail…"); onTriggered: layoutsDialog.open()}
            MenuItem {text: qsTr("Plein écran"); checkable: true; checked: win.visibility === Window.FullScreen; onTriggered: win.toggleFullScreen()}
            MenuItem {text: qsTr("Analyse assistée"); checkable: true; checked: !!controller.preferences.assistedAnalysis; onTriggered: controller.setPreference("assistedAnalysis", checked)}
            MenuItem {text: qsTr("Pièces en prise"); checkable: true; checked: !!controller.preferences.showHanging; onTriggered: controller.setPreference("showHanging", checked)}
            MenuItem {text: qsTr("Relations d’attaque"); checkable: true; checked: !!controller.preferences.showAttacks; onTriggered: controller.setPreference("showAttacks", checked)}
            MenuItem {objectName: "muteSounds"; text: qsTr("Couper le son"); checkable: true; checked: !controller.preferences.sounds; onTriggered: controller.setPreference("sounds", !checked)}
            MenuItem {text: qsTr("Raccourcis clavier…"); onTriggered: shortcutsDialog.open()}
            MenuItem {text: qsTr("Rechercher une commande…"); onTriggered: commands.open()}
            MenuSeparator {}
            MenuItem {text: qsTr("Paramètres…"); onTriggered: settingsDialog.open()}
        }
        Menu { objectName:"helpMenu"; title: qsTr("&Aide")
            MenuItem {text: qsTr("Guide de l’utilisateur"); onTriggered: guideDialog.openAt(0)}
            MenuItem {text: qsTr("Aide et raccourcis"); onTriggered: helpDialog.open()}
            MenuItem {text: qsTr("Mises à jour…"); onTriggered: updatesDialog.open()}
        }
    }
    // Une seule navigation, à gauche, comme ChessBase 18 et En Croissant : la rangée
    // d'onglets du haut répétait la barre de menus. Repliée en icônes si la fenêtre est
    // étroite ou si on le demande.
    RowLayout {
        anchors.fill: parent; spacing: 0
        T64Sidebar {
            id: sidebar; objectName: "workspaceTabs"
            Layout.fillHeight: true; Layout.preferredWidth: implicitWidth
            currentPage: win.page; preferences: controller.preferences
            collapsed: !!controller.preferences.sidebarCompact || win.width < 1280
            onPageRequested: function(index) { win.page = index }
            onActionRequested: function(action) {
                if (action === "preparationDialog") preparationDialog.open()
                else if (action === "engineSettings" || action === "settingsDialog") settingsDialog.open()
                else if (action === "collapse") controller.setPreference("sidebarCompact", !controller.preferences.sidebarCompact)
            }
        }
    ColumnLayout {
        Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 34 * controller.preferences.uiScale; color: win.bg
                Rectangle {anchors.bottom: parent.bottom; width: parent.width; height: 1; color: win.line}
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 14*win.uiScale; anchors.rightMargin: 8*win.uiScale; spacing: 6
                    Label {text: win.nav[win.page] || ""; font.weight: Font.DemiBold; Layout.alignment: Qt.AlignVCenter}
                    Label {text: "·  " + controller.state.title; elide: Text.ElideRight; color: win.muted; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.alignment: Qt.AlignVCenter}
                    TButton {objectName: "commandPaletteButton"; text: "Ctrl K"; flat: true; Layout.alignment: Qt.AlignVCenter; Accessible.name: qsTr("Rechercher une commande"); onClicked: commands.open()}
                    TButton {id: appMenuButton; objectName: "appMenuButton"; text: "⚙"; flat: true; Layout.alignment: Qt.AlignVCenter; Accessible.name: qsTr("Apparence et langue"); onClicked: appearanceDialog.open()}
                }
            }
        StackLayout {
            id: pages
            Layout.fillWidth: true; Layout.fillHeight: true; currentIndex: win.page
            // Une marge franche autour des pages : à 6 px, le contenu collait au filet de la
            // barre latérale et la fenêtre paraissait trop pleine.
            Layout.leftMargin:14*win.uiScale;Layout.rightMargin:14*win.uiScale;Layout.topMargin:10*win.uiScale;Layout.bottomMargin:6*win.uiScale

            Home {
                backend: controller
                onNewStudy: controller.newGame()
                onImportStudy: importDialog.open()
                onResumeStudy: win.page = 2
                onShowLibrary: win.page = 1
                onShowTraining: win.page = 4
                onShowRepertoires: repertoiresDialog.open()
                onShowPreparation: preparationDialog.open()
                onShowEngines: settingsDialog.open()
                onShowAccounts: accountsDialog.open()
            }

            // LIBRARY
            ScrollView {id:libraryViewport;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
            ColumnLayout {width:libraryViewport.availableWidth;height:Math.max(libraryViewport.availableHeight,implicitHeight);spacing:12
                // Une seule rangée : la base, la recherche, Importer. Le reste du travail sur
                // la base (exports, classements, comptes, téléchargements) attend dans le menu
                // « Base » : onze boutons sur deux rangées écrasaient la recherche.
                RowLayout {Layout.fillWidth:true;spacing:8
                    TComboBox {id:baseBox;onlyFirstOption:true;objectName:"baseSelector";Layout.preferredWidth:200*win.textScale;Layout.minimumWidth:120*win.textScale;textRole:"title";valueRole:"id";model:[{id:0,title:"Toutes les bases"}].concat(controller.sources);onActivated:{win.selectedBaseId=currentValue;win.filterLibrary()} onModelChanged:currentIndex=Math.max(0,indexOfValue(win.selectedBaseId))}
                    // Supprimer la base choisie : visible seulement quand une base l'est.
                    TButton {objectName:"removeBase";visible:win.selectedBaseId>0;text:qsTr("Supprimer…");onClicked:removeBaseDialog.ask(win.selectedBaseId)
                        ToolTip.visible:hovered;ToolTip.delay:400;ToolTip.text:qsTr("Supprimer cette base et mettre ses parties à la corbeille")}
                    TTextField {id:searchField;objectName:"librarySearch";placeholderText:qsTr("Rechercher un joueur, un code ECO, un tournoi…");Layout.fillWidth:true;Layout.minimumWidth:0;onTextChanged:searchTimer.restart();onAccepted:{searchTimer.stop();win.filterLibrary()}}
                    TComboBox {id:resultBox;model:["Tous résultats","1-0","0-1","1/2-1/2","*"];onActivated:win.filterLibrary();Layout.preferredWidth:140*win.textScale}
                    TButton {text:qsTr("Recherche avancée");onClicked:advancedDialog.open()}
                    // Les recherches enregistrées, et de quoi garder celle qui est à l'écran.
                    TButton {id:savedButton;objectName:"savedSearches";text:qsTr("Recherches  ▾");onClicked:savedMenu.popup(savedButton,0,savedButton.height)
                        Menu {id:savedMenu
                            MenuItem {text:qsTr("Enregistrer cette recherche…");onTriggered:saveSearchDialog.open()}
                            MenuSeparator {visible:controller.savedSearches.length>0}
                            Instantiator {model:controller.savedSearches
                                delegate:MenuItem {required property var modelData;text:modelData.name;onTriggered:win.applySearch(modelData)}
                                onObjectAdded:function(index,object){savedMenu.insertItem(index+2,object)}
                                onObjectRemoved:function(index,object){savedMenu.removeItem(object)}}
                            MenuSeparator {visible:controller.savedSearches.length>0}
                            Menu {id:deleteSearchMenu;title:qsTr("Supprimer");enabled:controller.savedSearches.length>0
                                Instantiator {model:controller.savedSearches
                                    delegate:MenuItem {required property var modelData;required property int index;text:modelData.name;onTriggered:controller.removeSearch(index)}
                                    onObjectAdded:function(index,object){deleteSearchMenu.insertItem(index,object)}
                                    onObjectRemoved:function(index,object){deleteSearchMenu.removeItem(object)}}}
                        }
                    }
                    TButton {text:qsTr("+  Importer");accent:true;enabled:!controller.busy;onClicked:importDialog.open()}
                    TButton {id:libraryMenuButton;objectName:"libraryMenuButton";text:qsTr("Base  ▾");onClicked:libraryMenu.popup(libraryMenuButton,0,libraryMenuButton.height)
                        Menu {id:libraryMenu
                            MenuItem {text:qsTr("Téléchargements…");onTriggered:downloadsDialog.open()}
                            MenuItem {text:qsTr("Comptes en ligne…");objectName:"showAccounts";onTriggered:accountsDialog.open()}
                            MenuItem {text:qsTr("Imports reprenables…");objectName:"showImports";onTriggered:importsDialog.open()}
                            MenuSeparator {}
                            MenuItem {text:qsTr("Exporter la bibliothèque…");onTriggered:win.openExport(true)}
                            MenuItem {text:qsTr("Exporter cette base…");enabled:baseBox.currentIndex>0;onTriggered:sourceExportDialog.open()}
                            MenuItem {objectName:"exportPolyglot";text:qsTr("Exporter un livre Polyglot…");onTriggered:polyglotDialog.open()}
                            MenuSeparator {}
                            MenuItem {objectName:"classifyOpenings";text:qsTr("Classer les ouvertures");enabled:!controller.busy;onTriggered:controller.classifyLibraryOpenings()}
                            MenuItem {objectName:"groupByOpening";text:qsTr("Ranger par ouverture");enabled:!controller.busy;onTriggered:controller.groupLibraryByOpening()}
                            MenuItem {text:qsTr("Doublons de coups");onTriggered:controller.findDuplicates()}
                            MenuItem {text:qsTr("Fusionner les doublons");enabled:!controller.busy;onTriggered:controller.mergeDuplicates()}
                            MenuItem {objectName:"mergeShown";text:qsTr("Fusionner les parties affichées en une étude");enabled:controller.games.length>1;onTriggered:controller.mergeShownGames()}
                            MenuItem {text:qsTr("Corriger les noms…");objectName:"showNames";onTriggered:namesDialog.open()}
                            MenuSeparator {}
                            MenuItem {text:qsTr("Mes compétences…");objectName:"showSkills";onTriggered:skillDialog.open()}
                        }
                    }
                }
                Flow {Layout.fillWidth:true;spacing:6
                    TChip {id:favoriteCheck;text:qsTr("Favoris");onToggled:win.filterLibrary()}
                    TChip {id:positionCheck;objectName:"positionFilter";text:qsTr("Position de l’échiquier");onToggled:win.filterLibrary()}
                    TChip {id:pawnCheck;objectName:"pawnStructureFilter";text:qsTr("Même structure de pions");visible:positionCheck.checked
                        ToolTip.visible:hovered;ToolTip.text:qsTr("Parties qui atteignent la structure de pions de l’échiquier, quelles que soient les autres pièces.")
                        onToggled:win.filterLibrary()}
                    TChip {id:trashCheck;text:qsTr("Corbeille");onToggled:win.filterLibrary()}
                    TButton {text:qsTr("Annuler la recherche");visible:controller.librarySearching;onClicked:controller.cancelLibrarySearch()}
                }
                Label {text:controller.busy?qsTr("Traitement en cours…"):controller.librarySearchStatus;color:win.muted;font.pixelSize:Math.round(11*win.textScale);Layout.fillWidth:true;wrapMode:Text.WordWrap}
                // Dense game table, one row per game, like a ChessBase or SCID list.
                // Le tableau et, à côté, un aperçu : on y fait défiler les coups de la partie
                // choisie sans quitter l’onglet ni perdre l’étude en cours d’analyse.
                RowLayout {
                    Layout.fillWidth:true;Layout.fillHeight:true;spacing:10
                    ColumnLayout {
                        Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumWidth:360;spacing:0
                        Rectangle {Layout.fillWidth:true;implicitHeight:24*win.textScale;color:win.raised;border.color:win.line
                            RowLayout {anchors.fill:parent;anchors.leftMargin:6;anchors.rightMargin:6;spacing:8
                                // Un clic sur un titre trie sur sa colonne, un second l'inverse ; la
                                // flèche dit l'ordre en cours. N° revient à l'ordre de modification.
                                // Le modèle ne dépend pas de la taille du texte : il en dépendait, et
                                // chaque cran de Ctrl + molette recréait les titres pendant que la rangée
                                // les plaçait. Qt 6.4 gardait alors un pointeur mort (plantage vu sous
                                // ASan, une fois sur trois). Les largeurs se calculent dans chaque titre.
                                Repeater {
                                    model:[{t:qsTr("N°"),c:"number",k:"updated"},{t:qsTr("Blancs"),c:"",k:"white"},{t:"Elo",c:"elo",k:"whiteElo"},
                                           {t:qsTr("Noirs"),c:"",k:"black"},{t:"Elo",c:"elo",k:"blackElo"},{t:qsTr("Résultat"),c:"result",k:"result"},
                                           {t:qsTr("Coups"),c:"moves",k:"length"},{t:"ECO",c:"eco",k:"eco"},
                                           {t:qsTr("Événement"),c:"event",k:"event"},{t:qsTr("Date"),c:"date",k:"date"},
                                           {t:"",c:"actions",k:""}]
                                    Label {required property var modelData
                                        readonly property real w:modelData.c===""?-1:modelData.c==="event"&&!libraryList.detailed?0:libraryList.columns[modelData.c]
                                        readonly property bool active:modelData.k!==""&&((win.advanced.sort||"updated")===modelData.k||(win.advanced.sort==="elo"&&modelData.k.indexOf("Elo")>0))
                                        // Flèche vers le haut : de A à Z, du plus petit au plus grand.
                                        readonly property bool rising:["updated","whiteElo","blackElo","date"].indexOf(modelData.k)<0!==!!win.advanced.reverse
                                        text:modelData.t+(active?(rising?" ▲":" ▼"):"");visible:w!==0;elide:Text.ElideRight
                                        color:active?win.fg:win.muted;font.weight:active?Font.DemiBold:Font.Normal
                                        Layout.fillWidth:w<0;Layout.preferredWidth:w<0?80:w;Layout.maximumWidth:w<0?100000:w;Layout.minimumWidth:0
                                        Accessible.role:modelData.k?Accessible.ColumnHeader:Accessible.StaticText;Accessible.name:modelData.k?qsTr("Trier par %1").arg(modelData.t):text
                                        HoverHandler {enabled:modelData.k!=="";cursorShape:Qt.PointingHandCursor}
                                        TapHandler {enabled:modelData.k!=="";onTapped:win.sortLibrary(modelData.k)}
                                    }
                                }
                            }
                        }
                        ListView {id:libraryList;objectName:"libraryResults";TextZoom {}Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:180;clip:true;spacing:0;model:controller.games;enabled:!controller.librarySearching;opacity:enabled?1:.5
                            // Pas de recyclage des lignes : avec Qt 6.4, une ligne recyclée pendant
                            // que la taille du texte change (Ctrl + molette) laissait un pointeur
                            // mort dans sa mise en page, et le programme tombait (vu sous ASan,
                            // un passage sur quatre environ). Une page compte au plus 200 lignes.
                            reuseItems:false
                            readonly property bool detailed:width>900*win.textScale
                            readonly property var columns:({number:40*win.textScale,elo:42*win.textScale,result:56*win.textScale,moves:44*win.textScale,
                                                            eco:38*win.textScale,event:200*win.textScale,date:82*win.textScale,actions:132*win.textScale})
                            ScrollBar.vertical:ScrollBar {}
                            delegate:Rectangle {required property var modelData;required property int index
                                // Des lignes de 24 pixels sans état de survol : on ne savait pas
                                // laquelle on visait avant de cliquer.
                                width:libraryList.width;height:24*win.textScale
                                color:rowHover.hovered?Qt.rgba(win.accent.r,win.accent.g,win.accent.b,controller.preferences.highContrast?.3:.15)
                                                      :(index%2?win.bg:win.panel)
                                HoverHandler {id:rowHover}
                                Rectangle {anchors.bottom:parent.bottom;width:parent.width;height:1;color:win.line;opacity:.5}
                                RowLayout {anchors.fill:parent;anchors.leftMargin:6;anchors.rightMargin:6;spacing:8
                                    // Mêmes contraintes que l'en-tête, colonne par colonne. Sans la
                                    // largeur préférée commune, les deux colonnes extensibles se
                                    // partageaient l'espace restant différemment des titres — « Noirs »
                                    // démarrait vingt-cinq pixels plus loin que son en-tête. Et sans
                                    // plafond, une valeur un peu large élargit sa colonne et pousse
                                    // toutes les suivantes.
                                    Label {text:modelData.id;Layout.preferredWidth:libraryList.columns.number;Layout.maximumWidth:libraryList.columns.number;Layout.minimumWidth:0;color:win.muted;elide:Text.ElideRight}
                                    Label {text:modelData.white;Layout.fillWidth:true;Layout.preferredWidth:80;Layout.minimumWidth:0;elide:Text.ElideRight;font.weight:modelData.result==="1-0"?Font.DemiBold:Font.Normal}
                                    Label {text:modelData.whiteElo>0?modelData.whiteElo:"";Layout.preferredWidth:libraryList.columns.elo;Layout.maximumWidth:libraryList.columns.elo;Layout.minimumWidth:0;color:win.muted}
                                    Label {text:modelData.black;Layout.fillWidth:true;Layout.preferredWidth:80;Layout.minimumWidth:0;elide:Text.ElideRight;font.weight:modelData.result==="0-1"?Font.DemiBold:Font.Normal}
                                    Label {text:modelData.blackElo>0?modelData.blackElo:"";Layout.preferredWidth:libraryList.columns.elo;Layout.maximumWidth:libraryList.columns.elo;Layout.minimumWidth:0;color:win.muted}
                                    Label {text:modelData.result==="1/2-1/2"?"½-½":modelData.result;Layout.preferredWidth:libraryList.columns.result;Layout.maximumWidth:libraryList.columns.result;Layout.minimumWidth:0;elide:Text.ElideRight}
                                    Label {text:modelData.plies>0?Math.ceil(modelData.plies/2):"";Layout.preferredWidth:libraryList.columns.moves;Layout.maximumWidth:libraryList.columns.moves;Layout.minimumWidth:0;color:win.muted}
                                    Label {text:modelData.eco||"";Layout.preferredWidth:libraryList.columns.eco;Layout.maximumWidth:libraryList.columns.eco;Layout.minimumWidth:0;elide:Text.ElideRight}
                                    Label {visible:libraryList.detailed;text:[modelData.event,modelData.collection].filter(function(s){return !!s}).join(" · ");Layout.preferredWidth:libraryList.columns.event;Layout.maximumWidth:libraryList.columns.event;Layout.minimumWidth:0;elide:Text.ElideRight;color:win.muted}
                                    // Le PGN note une date inconnue « ????.??.?? » : un tiret se
                                    // lit mieux dans une colonne, et dit la même chose.
                                    Label {text:modelData.date && modelData.date.indexOf("?")<0 ? modelData.date : "—";Layout.preferredWidth:libraryList.columns.date;Layout.maximumWidth:libraryList.columns.date;Layout.minimumWidth:0;color:win.muted;elide:Text.ElideRight}
                                    RowLayout {Layout.fillWidth:false;Layout.preferredWidth:libraryList.columns.actions;Layout.maximumWidth:libraryList.columns.actions;spacing:2
                                        TButton {text:modelData.favorite?"★":"☆";implicitHeight:22;flat:true;Layout.preferredWidth:24;Accessible.name:qsTr("Favori");onClicked:controller.favorite(modelData.id,!modelData.favorite)}
                                        Label {objectName:"medalMark";visible:(modelData.medals||0)>0;text:"◆";color:controller.preferences.accentColor;Layout.preferredWidth:10
                                            HoverHandler {id:medalHover} ToolTip.visible:medalHover.hovered;ToolTip.text:win.medalList(modelData.medals||0)}
                                        // « Ouvrir » ne paraît que sur la ligne visée : répété sur deux cents lignes,
                                        // il faisait une colonne de boutons plus voyante que les parties. Un double
                                        // clic sur la ligne ouvre toujours la partie.
                                        TButton {text:qsTr("Ouvrir");implicitHeight:22;Layout.fillWidth:true;opacity:rowHover.hovered||activeFocus?1:0;onClicked:controller.loadGame(modelData.id)}
                                        TButton {text:"⋮";implicitHeight:22;flat:true;Layout.preferredWidth:24;Accessible.name:qsTr("Actions de la partie");onClicked:{win.selectedGame=modelData.id;win.selectedMedals=modelData.medals||0;collectionField.text=modelData.collection||"";gameMenu.open()}}
                                    }
                                }
                                // Un clic choisit la partie pour l'aperçu, un double-clic l'ouvre
                                // dans Analyse : regarder ne doit pas coûter l'étude en cours.
                                TapHandler {onSingleTapped:libraryPreview.gameId=modelData.id
                                            onDoubleTapped:controller.loadGame(modelData.id)}
                            }
                        }
                    }
                    GamePreview {
                        id:libraryPreview;objectName:"libraryPreview"
                        // Largeur fixe : sans plafond, l'aperçu réclame la place de ses
                        // boutons et le tableau se réduit à rien.
                        Layout.preferredWidth:Math.round(300*win.textScale)
                        Layout.minimumWidth:Layout.preferredWidth;Layout.maximumWidth:Layout.preferredWidth
                        Layout.alignment:Qt.AlignTop
                        // En dessous, l'aperçu mangerait les colonnes du tableau.
                        visible:win.width>Math.round(1040*win.textScale)
                        backend:controller
                        boardSide:Math.round(260*win.textScale)
                    }
                }
                RowLayout {Layout.fillWidth:true;spacing:6
                    Label {text:(controller.stats.games||0)===1?qsTr("Une partie dans la bibliothèque"):qsTr("%1 parties dans la bibliothèque").arg(controller.stats.games||0);color:win.muted;Layout.fillWidth:true;elide:Text.ElideRight}
                    TButton {text:"‹";flat:true;objectName:"previousLibraryPage";Accessible.name:qsTr("Page précédente");enabled:!controller.librarySearching&&controller.libraryPage>1;onClicked:controller.previousLibraryPage()}
                    Label {text:qsTr("Page %1").arg(controller.libraryPage);color:win.muted}
                    TButton {text:"›";flat:true;objectName:"nextLibraryPage";Accessible.name:qsTr("Page suivante");enabled:!controller.librarySearching&&controller.libraryHasNext;onClicked:controller.nextLibraryPage()}
                }
            }}

            // ANALYSIS
            ScrollView {id:analysisViewport;objectName:"analysisViewport";contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
ColumnLayout {width:analysisViewport.availableWidth;height:Math.max(analysisViewport.availableHeight,implicitHeight);
                Layout.margins:0;spacing:win.height<820?8:14
                // Toolbar: open studies, training status, study actions.
                RowLayout {Layout.fillWidth:true;spacing:4
                    ListView {Layout.fillWidth:true;Layout.preferredHeight:28*win.uiScale;orientation:ListView.Horizontal;model:controller.tabs;spacing:4;clip:true
                        // Un onglet de document : titre et croix dans le même cadre, l'onglet ouvert
                        // marqué d'un trait d'accent. Deux boutons accolés, l'un vert plein, se
                        // lisaient comme deux commandes et non comme une étude ouverte.
                        delegate:Rectangle {id:studyTab;required property var modelData;required property int index
                            width:Math.min(240*win.textScale,Math.max(110*win.textScale,tabTitle.implicitWidth+tabClose.width+20*win.uiScale));height:28*win.uiScale
                            radius:3;color:modelData.active?win.panel:tabArea.containsMouse?win.raised:"transparent"
                            border.color:modelData.active?win.line:"transparent"
                            Rectangle {visible:studyTab.modelData.active;anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.margins:1;height:2;color:win.accent}
                            MouseArea {id:tabArea;anchors.fill:parent;hoverEnabled:true;onClicked:controller.switchTab(studyTab.index)}
                            Label {id:tabTitle;anchors.left:parent.left;anchors.leftMargin:10*win.uiScale;anchors.right:tabClose.left;anchors.verticalCenter:parent.verticalCenter
                                   text:(studyTab.modelData.dirty?"• ":"")+studyTab.modelData.title;elide:Text.ElideRight
                                   font.weight:studyTab.modelData.active?Font.DemiBold:Font.Normal;color:studyTab.modelData.active?win.fg:win.muted}
                            TButton {id:tabClose;flat:true;text:"×";anchors.right:parent.right;anchors.rightMargin:3;anchors.verticalCenter:parent.verticalCenter
                                     implicitWidth:22*win.uiScale;implicitHeight:22*win.uiScale;Accessible.name:qsTr("Fermer l’étude");onClicked:controller.closeTab(studyTab.index)}
                            Accessible.role:Accessible.PageTab;Accessible.name:studyTab.modelData.title;Accessible.selected:studyTab.modelData.active
                        }
                    }
                    // Parcourir la base ou le recueil depuis l'analyse : la partie
                    // voisine de la liste de la bibliothèque remplace celle-ci, sans
                    // aller-retour par l'onglet ni onglet d'étude en plus.
                    RowLayout {spacing:2;visible:!controller.state.training&&controller.state.gameCount>0
                        TButton {objectName:"previousGame";text:"◀";flat:true;implicitWidth:26*win.textScale;implicitHeight:26*win.textScale
                                 enabled:controller.state.gameIndex!==0;onClicked:controller.stepGame(-1)
                                 Accessible.name:qsTr("Partie précédente");ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Partie précédente de la liste (Alt+←)")}
                        Label {color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)
                               text:controller.state.gameIndex>=0?qsTr("%1 / %2").arg(controller.state.gameIndex+1).arg(controller.state.gameCount)
                                                                 :qsTr("hors liste")}
                        TButton {objectName:"nextGame";text:"▶";flat:true;implicitWidth:26*win.textScale;implicitHeight:26*win.textScale
                                 enabled:controller.state.gameIndex<controller.state.gameCount-1;onClicked:controller.stepGame(1)
                                 Accessible.name:qsTr("Partie suivante");ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Partie suivante de la liste (Alt+→)")}
                    }
                    Label {visible:controller.state.training;Layout.maximumWidth:420;elide:Text.ElideRight;color:win.fg;font.weight:Font.DemiBold
                        text:controller.puzzles.active?(controller.puzzles.current.review?qsTr("Révision d’un puzzle manqué · "):"")+qsTr("Puzzle %1 · votre classement %2").arg(controller.puzzles.current.rating).arg(controller.puzzles.rating):(controller.state.questions?qsTr("Questions · %1 point(s) sur %2").arg(controller.state.guessFound).arg(controller.state.guessAsked):controller.state.guessing?qsTr("Deviner le coup · %1 trouvé(s) sur %2").arg(controller.state.guessFound).arg(controller.state.guessAsked):(controller.state.repertoireTraining>0?qsTr("Retrouver le coup"):qsTr("Retrouver la ligne"))+" · "+controller.state.trainingProgress)}
                    TButton {objectName:"guessReveal";text:qsTr("Montrer le coup");visible:!!controller.state.guessing&&!controller.state.trainingComplete;onClicked:controller.revealCard()}
                    // Après une erreur : effacer ses flèches et réessayer, ou jouer le bon coup et
                    // continuer l'exercice (il reste compté comme manqué).
                    TButton {objectName:"retryTraining";text:qsTr("Réessayer");visible:controller.state.training&&!!(controller.state.mistake||{}).move;onClicked:controller.retryTraining()}
                    TButton {objectName:"continueTraining";text:qsTr("Continuer");visible:controller.state.training&&!controller.state.trainingComplete&&!!(controller.state.mistake||{}).move&&!controller.state.guessing
                             ToolTip.visible:hovered;ToolTip.text:qsTr("Joue le bon coup et poursuit l’exercice ; il reste compté comme manqué.");onClicked:controller.continueTraining()}
                    TButton {objectName:"puzzleHint";text:qsTr("Indice");visible:controller.puzzles.active&&!controller.state.trainingComplete;onClicked:controller.puzzleHint();ToolTip.visible:hovered;ToolTip.text:qsTr("Montre la pièce à jouer ; compte comme un échec.")}
                    TButton {objectName:"nextPuzzle";text:qsTr("Puzzle suivant");accent:true;visible:controller.puzzles.active&&controller.state.trainingComplete;onClicked:controller.nextPuzzle()}
                    TButton {text:qsTr("Position suivante");visible:controller.state.repertoireTraining>0;enabled:controller.state.trainingComplete && controller.state.repertoireDue>0;onClicked:controller.startRepertoireTraining(controller.state.repertoireTraining)}
                    TButton {text:controller.state.training?qsTr("Quitter l’exercice"):qsTr("Nouvelle étude");onClicked:controller.state.training?controller.finishTraining():controller.newGame()}
                    TButton {objectName:"saveStudy";text:controller.state.training?qsTr("Voir la solution"):qsTr("Enregistrer");accent:true;onClicked:controller.state.training?controller.revealCard():controller.saveGame()}
                    TButton {text:"⋮";Accessible.name:qsTr("Actions de l’étude");visible:!controller.state.training;onClicked:analysisMenu.open()}
                    TButton {text:"↶";Accessible.name:qsTr("Annuler le coup");enabled:controller.state.canUndo;onClicked:controller.undo();ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Annuler le coup")}
                    TButton {text:"↷";Accessible.name:qsTr("Rétablir");enabled:controller.state.canRedo;onClicked:controller.redo();ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Rétablir")}
                    TButton {text:qsTr("Bilan");enabled:!controller.state.training;onClicked:reviewDialog.open()}
                }
                SplitView {
                    id:analysisSplit;Layout.minimumHeight:365*win.textScale;
                    property real savedRatio:controller.preferences.analysisPanelRatio
                    readonly property real fittedBoardWidth:Math.max(290,Math.min(width*savedRatio,width-312))
                    onFittedBoardWidthChanged:if(boardPanel&&!resizing)boardPanel.SplitView.preferredWidth=fittedBoardWidth
objectName:"analysisSplit"
                    Layout.fillWidth:true;Layout.fillHeight:true;orientation:Qt.Horizontal
                    onResizingChanged:if(!resizing&&width>0)controller.setPreference("analysisPanelRatio",boardPanel.width/width)
                    handle:Rectangle {implicitWidth:5;color:SplitHandle.hovered||SplitHandle.pressed?win.line:"transparent";Rectangle {anchors.horizontalCenter:parent.horizontalCenter;width:1;height:parent.height;color:win.line}}
                    DockColumn {
                        objectName:"leftDock";dock:win;zone:"left"
                        visible:!win.boardOnly&&stacks.length>0
                        SplitView.preferredWidth:Math.round(300*win.uiScale);SplitView.minimumWidth:220
                    }
                    ColumnLayout {
                        id:boardPanel;objectName:"boardPanel";SplitView.preferredWidth:analysisSplit.fittedBoardWidth;SplitView.minimumWidth:290;spacing:win.height<820?5:10
                        RowLayout {Layout.fillWidth:true;Layout.leftMargin:Math.max(0,(parent.width-chessboard.width)/2);Layout.rightMargin:Layout.leftMargin
                            Rectangle {Layout.preferredWidth:10;Layout.preferredHeight:10;Layout.alignment:Qt.AlignVCenter;radius:2;color:controller.state.flipped?"#ffffff":"#1d2125";border.color:"#1d2125"}
                            Label {Layout.fillWidth:true;elide:Text.ElideRight;text:controller.state.flipped?(controller.state.tags.White||qsTr("Blancs")):(controller.state.tags.Black||qsTr("Noirs"));font.weight:Font.DemiBold}
                            Label {objectName:"takenTop";text:win.takenLine(!!controller.state.flipped);color:win.muted;font.pixelSize:Math.round(13*controller.preferences.fontScale*controller.preferences.uiScale)}
                            Label {text:controller.state.clockRunning?(controller.state.flipped?controller.state.whiteClock:controller.state.blackClock):((controller.state.flipped?controller.state.tags.WhiteElo:controller.state.tags.BlackElo)||"");color:win.muted}
                        }
                        Item {
                            id:boardArea
                            Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:200
                            // La barre prend sa place d'abord : si l'échiquier se dimensionnait
                            // sur la largeur totale, les deux se poursuivraient sans se poser.
                            readonly property real barSpace:controller.preferences.evalBar?26:0
                            readonly property real side:Math.max(0,Math.min(width-12-barSpace,height-12))*controller.preferences.boardScale
                            Row {
                                anchors.centerIn:parent;spacing:boardArea.barSpace>0?5:0
                                // Hauteur tenue de l'échiquier lui-même, et non recalculée à
                                // côté : deux calculs parallèles finissent toujours par
                                // diverger d'un arrondi, et la barre ne tombe plus juste.
                                EvalBar {visible:boardArea.barSpace>0;width:18;height:chessboard.height;anchors.verticalCenter:parent.verticalCenter;backend:controller;flipped:controller.state.flipped}
                                Item {
                                    width:boardArea.side+12;height:width;anchors.verticalCenter:parent.verticalCenter
                                    Rectangle {anchors.fill:parent;color:"transparent";border.color:win.line}
                                    Board {id:chessboard;objectName:"chessboard";backend:controller;hintThreat:!!controller.state.threat;hintMoves:controller.state.analysis&&controller.preferences.engineArrows&&!controller.state.training&&!controller.state.playing?controller.engineLines.slice(0,3).map(l=>(l.uci||"").split(" ")[0]).filter(m=>m.length>=4).join(" "):"";coordinates:controller.preferences.coordinates;width:boardArea.side;height:width;anchors.centerIn:parent
                                        reviewMarks:(controller.state.mistake||{}).marks||win.reviewSpot.marks||[]}
                                }
                            }
                        }
                        RowLayout {Layout.fillWidth:true;Layout.leftMargin:Math.max(0,(parent.width-chessboard.width)/2);Layout.rightMargin:Layout.leftMargin
                            Rectangle {Layout.preferredWidth:10;Layout.preferredHeight:10;Layout.alignment:Qt.AlignVCenter;radius:2;color:controller.state.flipped?"#1d2125":"#ffffff";border.color:"#1d2125"}
                            Label {Layout.fillWidth:true;elide:Text.ElideRight;text:controller.state.flipped?(controller.state.tags.Black||qsTr("Noirs")):(controller.state.tags.White||qsTr("Blancs"));font.weight:Font.DemiBold}
                            Label {objectName:"takenBottom";text:win.takenLine(!controller.state.flipped);color:win.muted;font.pixelSize:Math.round(13*controller.preferences.fontScale*controller.preferences.uiScale)}
                            Label {text:controller.state.clockRunning?(controller.state.flipped?controller.state.blackClock:controller.state.whiteClock):((controller.state.flipped?controller.state.tags.BlackElo:controller.state.tags.WhiteElo)||"");color:win.muted}
                        }
                        RowLayout {Layout.fillWidth:true;spacing:6;Layout.leftMargin:Math.max(0,(parent.width-Math.max(chessboard.width,476))/2);Layout.rightMargin:Layout.leftMargin
                            TButton {text:"|◀";implicitWidth:38;padding:6;enabled:controller.state.canBack&&!controller.state.training;onClicked:controller.step(-2);Accessible.name:qsTr("Début de la partie");ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Début")}
                            TButton {text:"◀";implicitWidth:38;padding:6;enabled:controller.state.canBack&&!controller.state.training;onClicked:controller.step(-1);Accessible.name:qsTr("Coup précédent");ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Coup précédent")}
                            TButton {text:"▶";implicitWidth:38;padding:6;enabled:controller.state.canForward&&!controller.state.training;onClicked:controller.step(1);Accessible.name:qsTr("Coup suivant");ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Coup suivant")}
                            TButton {objectName:"autoplay";visible:boardPanel.width>=450;text:autoplay.running?"❚❚":"▶▶";implicitWidth:38;padding:6;enabled:autoplay.running||(controller.state.canForward&&!controller.state.training);onClicked:autoplay.running=!autoplay.running;Accessible.name:autoplay.running?qsTr("Arrêter la lecture"):qsTr("Lire la partie");ToolTip.visible:hovered||visualFocus;ToolTip.text:(autoplay.running?qsTr("Arrêter la lecture"):qsTr("Lire la partie"))+" ("+qsTr("Espace")+")"}
                            TButton {text:"▶|";implicitWidth:38;padding:6;enabled:controller.state.canForward&&!controller.state.training;onClicked:controller.step(2);Accessible.name:qsTr("Fin de la variante");ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Fin de la variante")}
                            TButton {text:"⇅";implicitWidth:38;onClicked:controller.flip();Accessible.name:qsTr("Retourner l’échiquier");ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Retourner l’échiquier · F")}
                            // Taper un coup plutôt que le jouer à la souris : « Cf3 », « e4 », « O-O ».
                            // Entrée le joue dès qu'un seul coup répond ; les candidats s'affichent
                            // pendant la frappe.
                            TTextField {id:moveEntry;objectName:"moveEntry";visible:!controller.state.training&&(boardPanel.width>=600||activeFocus);Layout.preferredWidth:84*controller.preferences.uiScale;Layout.preferredHeight:26*controller.preferences.uiScale
                                placeholderText:qsTr("Coup…");selectByMouse:true;Accessible.name:qsTr("Taper un coup")
                                readonly property var matches:text.length?controller.typedMoves(text):[]
                                color:text.length&&!matches.length?"#c0392b":win.fg
                                onAccepted:{if(controller.playTyped(text))text=""}
                                Keys.onEscapePressed:{text="";focus=false}
                                ToolTip.visible:activeFocus&&text.length>0
                                ToolTip.text:matches.length===0?qsTr("Aucun coup légal ne s’écrit ainsi"):matches.length===1?qsTr("Entrée : %1").arg(matches[0].san)
                                            :matches.slice(0,8).map(function(m){return m.san}).join(" · ")+(matches.length>8?" …":"")}
                            Item {Layout.fillWidth:true}
                            TButton {id:positionActions;objectName:"positionActions";text:analysisSplit.width>1000?qsTr("Position  ⋯"):"⋯";implicitWidth:analysisSplit.width>1000?110:38;visible:!controller.state.training;onClicked:positionMenu.open();Accessible.name:qsTr("Outils de la position")
                                Menu {id:positionMenu;y:-height
                                    MenuItem {text:qsTr("Éditer la position");onTriggered:positionEditor.open()}
                                    MenuItem {text:qsTr("Copier la FEN");onTriggered:controller.copyText("fen")}
                                    MenuItem {text:qsTr("Ouvrir sur un téléphone (QR)…");onTriggered:positionQr.open()}
                                    MenuItem {text:qsTr("Charger une FEN");onTriggered:fenDialog.open()}
                                    MenuItem {text:qsTr("Exporter le diagramme");onTriggered:diagramDialog.open()}
                                    MenuSeparator {}
                                    MenuItem {text:controller.state.playing?qsTr("Arrêter la partie"):qsTr("Jouer une partie");onTriggered:controller.state.playing?controller.stopPlaying():playDialog.open()}
                                }
                            }
                            TButton {text:controller.state.playing?qsTr("Arrêter"):qsTr("Jouer");visible:!controller.state.training&&analysisSplit.width>1000;onClicked:controller.state.playing?controller.stopPlaying():playDialog.open()}
                        }
                        Label {objectName:"reviewNote";visible:!!((controller.state.mistake||{}).note||win.reviewSpot.note);text:(controller.state.mistake||{}).note||win.reviewSpot.note||"";color:"#c0523f";Layout.alignment:Qt.AlignHCenter;wrapMode:Text.WordWrap;Layout.maximumWidth:boardArea.width;horizontalAlignment:Text.AlignHCenter;font.pixelSize:Math.round(12*controller.preferences.fontScale*controller.preferences.uiScale)}
                        Label {text:win.systemText(controller.state.clockResult||controller.state.status);color:win.accent;Layout.alignment:Qt.AlignHCenter;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);Layout.topMargin:2}
                        DockColumn {
                            objectName:"bottomDock";dock:win;zone:"bottom"
                            visible:!win.boardOnly&&stacks.length>0
                            Layout.fillWidth:true;Layout.preferredHeight:Math.round(190*win.textScale)
                        }
                    }
                    // Right column, ChessBase style: notation, tree and book above, the engine pane below.
                    // Les colonnes de panneaux, à la manière de Scid vs. PC : chaque panneau se
                    // glisse par son onglet d'une pile à l'autre ou sur un bord (DockStack.qml).
                    DockColumn {
                        id:analysisPanel;objectName:"analysisPanel";dock:win;zone:"right"
                        visible:!win.boardOnly&&stacks.length>0
                        SplitView.fillWidth:true;SplitView.minimumWidth:300
                    }
                }
            }
}

            // OPENINGS
            ScrollView {id:openingsViewport;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
ColumnLayout {width:openingsViewport.availableWidth;height:Math.max(openingsViewport.availableHeight,implicitHeight);
                Layout.margins:0;spacing:20
                Label {text:qsTr("Les suites jouées dans votre bibliothèque, depuis la position actuelle de l’échiquier.");color:win.muted;wrapMode:Text.WordWrap;Layout.fillWidth:true}
                Flow {Layout.fillWidth:true;spacing:8
                    TButton {objectName:"openingReport";text:qsTr("Dossier de la position");accent:true;onClicked:openingReportDialog.open()
                        ToolTip.visible:hovered;ToolTip.text:qsTr("Les coups joués ici, leur score, le même découpage par force des joueurs, et les parties les mieux classées qui y sont passées.")}
                    TButton {text:qsTr("←  Reculer");enabled:controller.state.canBack;onClicked:controller.step(-1)} TButton {text:qsTr("Position initiale");onClicked:controller.step(-2)} TButton {text:qsTr("Voir l’échiquier");onClicked:win.page=2} Item {Layout.fillWidth:true} TButton {text:qsTr("Préparer");onClicked:preparationDialog.open()} TButton {text:qsTr("Mes répertoires");objectName:"showRepertoires";onClicked:repertoiresDialog.open()} TButton {text:qsTr("Créer une carte depuis ici");onClicked:cardDialog.open()}}
                // Le nom de l'ouverture d'abord, en clair ; la FEN reste dessous, plus discrète,
                // pour qui veut la copier.
                Label {objectName:"openingsName";visible:!!controller.state.opening;text:controller.state.opening||"";font.weight:Font.DemiBold;elide:Text.ElideRight;Layout.fillWidth:true}
                TextEdit {objectName:"openingsFen";text:controller.state.fen;readOnly:true;selectByMouse:true;color:win.muted;selectionColor:win.accent;font.family:"monospace";opacity:.8
                    font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);Layout.fillWidth:true;wrapMode:TextEdit.NoWrap;clip:true}
                Rectangle {Layout.fillWidth:true;height:1;color:win.line}
                // En-têtes alignés sur les colonnes des lignes, en petites capitales comme le troisième.
                RowLayout {Layout.fillWidth:true;Layout.leftMargin:8;Layout.rightMargin:8;spacing:12;Label {text:qsTr("Coup");Layout.preferredWidth:76;color:win.muted;font.capitalization:Font.AllUppercase;font.letterSpacing:.4;font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale)} Label {text:qsTr("Parties");Layout.preferredWidth:66;color:win.muted;font.capitalization:Font.AllUppercase;font.letterSpacing:.4;font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale)} Label {text:qsTr("BLANCS / NULLES / NOIRS (RÉSULTATS TERMINÉS)");color:win.muted;font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);Layout.fillWidth:true}}
                // Une ligne par coup, serrées comme un tableau : à 62 px et espacées de 10, trois
                // suites remplissaient déjà l'écran.
                ListView {id:openingList;Layout.minimumHeight:160;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:2;model:controller.openings
                    delegate:Rectangle {
                        required property var modelData
                        width:openingList.width;height:40*win.uiScale;radius:4;color:win.panel;border.color:win.line
                        RowLayout {anchors.fill:parent;anchors.leftMargin:8;anchors.rightMargin:8;spacing:12
                            TButton {text:modelData.san;Layout.preferredWidth:76;onClicked:controller.move(modelData.uci)}
                            Label {text:modelData.count;Layout.preferredWidth:66;font.weight:Font.DemiBold}
                            Row {Layout.fillWidth:true;Layout.preferredHeight:22
                                property real total:Math.max(1,modelData.white+modelData.draw+modelData.black)
                                Rectangle {width:parent.width*modelData.white/parent.total;height:22;color:"#f3f0e8";border.color:win.line;Text {anchors.centerIn:parent;text:modelData.white;color:"#2b2925";font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);visible:parent.width>25}}
                                Rectangle {width:parent.width*modelData.draw/parent.total;height:22;color:"#a19c91";Text {anchors.centerIn:parent;text:modelData.draw;color:"#ffffff";font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);visible:parent.width>25}}
                                Rectangle {width:parent.width*modelData.black/parent.total;height:22;color:"#2b2925";Text {anchors.centerIn:parent;text:modelData.black;color:"#f3f0e8";font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);visible:parent.width>25}}
                            }
                            TButton {text:qsTr("Jouer  →");onClicked:controller.move(modelData.uci)}
                        }
                    }
                    ColumnLayout {anchors.centerIn:parent;visible:controller.openings.length===0;spacing:15
                        Label {text:qsTr("Aucune partie ne continue depuis cette position.");color:win.muted}
                        TButton {text:qsTr("Importer un PGN");Layout.alignment:Qt.AlignHCenter;onClicked:importDialog.open()}
                    }
                }
                Label {text:qsTr("Chaque partie compte une fois par coup. Les parties inachevées sont comptées mais exclues de la barre de résultats.");wrapMode:Text.WordWrap;Layout.fillWidth:true;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);color:win.muted}
            }
}

            // TRAINING
            ScrollView {id:trainingViewport;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
ColumnLayout {width:trainingViewport.availableWidth;height:Math.max(trainingViewport.availableHeight,implicitHeight);
                Layout.margins:0;spacing:20
                ColumnLayout {Layout.fillWidth:true
                    ColumnLayout {Layout.fillWidth:true;spacing:6;Label {text:qsTr("Vos propres lignes, avec des révisions espacées dans le temps.");Layout.fillWidth:true;wrapMode:Text.WordWrap;color:win.muted}}
                    Item {Layout.fillWidth:true}
                    TButton {text:qsTr("+  Créer une carte");accent:true;onClicked:cardDialog.open()}
                }
                Row {Layout.fillWidth:true;spacing:16
                    Repeater {model:[{title:"À réviser",key:"due"},{title:"Révisions effectuées",key:"reviews"},{title:"Réussites",key:"successes"}]
                        delegate:Rectangle {required property var modelData;width:(parent.width-32)/3;height:74*win.textScale;radius:6;color:win.panel;border.color:win.line
                            ColumnLayout {anchors.fill:parent;anchors.margins:14;spacing:2;Label {text:win.systemText(modelData.title);Layout.fillWidth:true;wrapMode:Text.WordWrap;color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)} Label {text:controller.stats[modelData.key]||0;font.family:controller.preferences.fontFamily;font.pixelSize:Math.round(30*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.Normal}}
                        }
                    }
                }
                Flow {Layout.fillWidth:true;spacing:8
                    TButton {objectName:"openPuzzles";text:qsTr("Puzzles");onClicked:puzzlesDialog.open()}
                    TButton {objectName:"openCoordinates";text:qsTr("Coordonnées");onClicked:coordinatesDialog.open()}
                    TButton {objectName:"openEndgames";text:qsTr("Finales");onClicked:endgamesDialog.open()}
                    TButton {text:qsTr("Réviser un répertoire");onClicked:repertoiresDialog.open()}
                    TButton {text:qsTr("Réviser la prochaine carte");accent:true;enabled:(controller.stats.due||0)>0;onClicked:controller.nextDueCard()}
                    Label {text:qsTr("Aujourd’hui : ")+(controller.stats.todayReviews||0)+" / "+controller.preferences.dailyGoal+qsTr(" · Cette semaine : ")+(controller.stats.weekReviews||0);color:win.muted;height:28*win.uiScale;verticalAlignment:Text.AlignVCenter;leftPadding:8*win.uiScale}
                    Label {text:qsTr("Objectif");height:28*win.uiScale;verticalAlignment:Text.AlignVCenter;leftPadding:8*win.uiScale} TSpinBox {from:1;to:1000;value:controller.preferences.dailyGoal;onValueModified:controller.setPreference("dailyGoal",value)}
                }
                ProgressBar {Layout.fillWidth:true;from:0;to:controller.preferences.dailyGoal;value:controller.stats.todayReviews||0}
                // Le programme du jour, tiré de vos données : vos cartes dues, vos propres
                // erreurs, votre thème tactique le plus faible, puis calcul et visualisation.
                ColumnLayout {objectName:"trainingPlan";Layout.fillWidth:true;spacing:6
                    readonly property var items:{
                        const list=[], p=controller.puzzles||{}, due=controller.stats.due||0
                        if(due>0)list.push({title:qsTr("Réviser vos cartes"),detail:qsTr("%1 carte(s) à revoir aujourd’hui").arg(due),run:function(){controller.nextDueCard()}})
                        if((p.mine||0)>0)list.push({title:qsTr("Vos erreurs"),detail:qsTr("%1 puzzle(s) tirés de vos parties").arg(p.mine),run:function(){controller.startPuzzle("","mine")}})
                        const weak=(p.themes||[]).filter(t=>t.attempts>=5&&t.success<70).sort((x,y)=>x.success-y.success)[0]
                        if(weak)list.push({title:qsTr("Votre point faible"),detail:qsTr("%1 : %2 % de réussite").arg(puzzlesDialog.themeLabel(weak.tag)).arg(weak.success),run:function(){controller.startPuzzle(weak.tag,"")}})
                        else if((p.total||0)>0)list.push({title:qsTr("Puzzles variés"),detail:qsTr("pour situer vos points faibles"),run:function(){controller.startPuzzle("","")}})
                        list.push({title:qsTr("Calcul"),detail:qsTr("vérifier une variante sans bouger les pièces (exercice de Stoyko)"),run:function(){stoykoDialog.open()}})
                        if((p.total||0)>0)list.push({title:qsTr("Visualisation"),detail:qsTr("un puzzle les pièces masquées"),run:function(){controller.startBlindPuzzle()}})
                        return list
                    }
                    Label {text:qsTr("Programme du jour");font.pixelSize:Math.round(13*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.DemiBold}
                    Repeater {model:parent.items.length
                        delegate:Rectangle {required property int index;readonly property var item:parent.items[index]||({})
                            Layout.fillWidth:true;implicitHeight:planRow.implicitHeight+12;radius:4;color:win.panel;border.color:win.line
                            RowLayout {id:planRow;anchors.fill:parent;anchors.leftMargin:10;anchors.rightMargin:6;spacing:10
                                Label {text:(index+1)+".";color:win.muted}
                                Label {text:item.title||"";font.weight:Font.DemiBold}
                                Label {text:item.detail||"";color:win.muted;elide:Text.ElideRight;Layout.fillWidth:true}
                                TButton {text:qsTr("Commencer");enabled:!controller.busy&&!controller.state.training;onClicked:item.run()}
                            }
                        }
                    }
                }
                Label {text:qsTr("Mes cartes");font.pixelSize:Math.round(13*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.DemiBold;Layout.topMargin:4}
                ListView {id:trainingList;Layout.minimumHeight:180;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:8;model:controller.cards;ScrollBar.vertical:ScrollBar {}
                    delegate:Rectangle {required property var modelData;width:trainingList.width;height:64*win.textScale;radius:6;color:win.panel;border.color:win.line
                        RowLayout {anchors.fill:parent;anchors.margins:16;spacing:16
                            Label {text:"◎";font.pixelSize:Math.round(28*controller.preferences.fontScale*controller.preferences.uiScale);color:win.accent}
                            ColumnLayout {Layout.fillWidth:true;spacing:6
                                Label {text:modelData.title;font.weight:Font.DemiBold;elide:Text.ElideRight;Layout.fillWidth:true}
                                Label {text:qsTr("Révision : ")+modelData.due+" · "+modelData.reviews+qsTr(" passage(s) · intervalle ")+modelData.interval+qsTr(" j");color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                            }
                            TButton {text:qsTr("Réviser");accent:true;onClicked:controller.startCard(modelData.id)}
                            TButton {text:"×";Accessible.name:qsTr("Supprimer la carte");onClicked:{deleteCardDialog.cardId=modelData.id;deleteCardDialog.open()}}
                        }
                    }
                    ColumnLayout {anchors.centerIn:parent;width:parent.width-24;visible:controller.cards.length===0;spacing:16
                        Label {text:qsTr("Vos variantes deviennent votre entraînement.");horizontalAlignment:Text.AlignHCenter;font.pixelSize:Math.round(17*controller.preferences.fontScale*controller.preferences.uiScale);Layout.alignment:Qt.AlignHCenter;Layout.fillWidth:true;wrapMode:Text.WordWrap}
                        Label {text:qsTr("Dans une étude, placez-vous avant les coups à apprendre,\npuis cliquez sur « Créer une carte ».");horizontalAlignment:Text.AlignHCenter;color:win.muted;Layout.alignment:Qt.AlignHCenter;Layout.fillWidth:true;wrapMode:Text.WordWrap}
                        TButton {text:qsTr("Ouvrir l’atelier");Layout.alignment:Qt.AlignHCenter;onClicked:win.page=2}
                    }
                }
            }
}

            // TOOLS
            ScrollView {id:toolsViewport;contentWidth:availableWidth;clip:true
                ColumnLayout {width:toolsViewport.availableWidth;spacing:20
                    Label {text:qsTr("Importer, sauvegarder, vérifier. Gardez la main sur vos données.");color:win.muted;wrapMode:Text.WordWrap;Layout.fillWidth:true}
                    GridLayout {columns:toolsViewport.availableWidth>1000*win.textScale?2:1;columnSpacing:18;rowSpacing:18;Layout.fillWidth:true
                        Rectangle {Layout.fillWidth:true;Layout.fillHeight:true;implicitHeight:children[0].implicitHeight+36;radius:6;color:win.panel;border.color:win.line
                            ColumnLayout {anchors.fill:parent;anchors.margins:18;spacing:8
                                Label {text:qsTr("PGN & positions");font.pixelSize:Math.round(15*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.DemiBold}
                                Label {text:qsTr("Ouvrir un fichier ou coller une partie et ses variantes.");wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted}
                                Item {Layout.fillHeight:true}
                                Flow {Layout.fillWidth:true;spacing:8;TButton {text:qsTr("Coller un PGN");onClicked:pgnDialog.open()} TButton {text:qsTr("Charger une FEN");onClicked:fenDialog.open()}}
                            }
                        }
                        Rectangle {Layout.fillWidth:true;Layout.fillHeight:true;implicitHeight:children[0].implicitHeight+36;radius:6;color:win.panel;border.color:win.line
                            ColumnLayout {anchors.fill:parent;anchors.margins:18;spacing:8
                                Label {text:qsTr("Sauvegarde complète");font.pixelSize:Math.round(15*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.DemiBold}
                                Label {text:qsTr("Parties, favoris, collections et cartes dans un fichier SQLite.");wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted}
                                Item {Layout.fillHeight:true}
                                Flow {Layout.fillWidth:true;spacing:8;TButton {text:qsTr("Sauvegarder");onClicked:backupDialog.open()} TButton {objectName:"openBackups";text:qsTr("Restaurer…");onClicked:backupsDialog.open()} TButton {text:qsTr("Ouvrir une base");onClicked:openDatabaseDialog.open()}}
                            }
                        }
                        Rectangle {Layout.fillWidth:true;Layout.fillHeight:true;implicitHeight:children[0].implicitHeight+36;radius:6;color:win.panel;border.color:win.line
                            ColumnLayout {anchors.fill:parent;anchors.margins:18;spacing:8
                                Label {text:qsTr("Intégrité & diagnostic");font.pixelSize:Math.round(15*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.DemiBold}
                                Label {text:qsTr("Vérifiez la bibliothèque et comptez les positions légales depuis l’échiquier.");wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted}
                                Item {Layout.fillHeight:true}
                                Flow {Layout.fillWidth:true;spacing:8;TButton {objectName:"exportDiagnostic";text:qsTr("Journal de diagnostic…");onClicked:diagnosticDialog.open()} TButton {text:qsTr("Vérifier la base");enabled:!controller.busy;onClicked:controller.verifyDatabase()} TButton {text:qsTr("Réindexer");enabled:!controller.busy;onClicked:controller.rebuildIndexes()} TButton {objectName:"repairDatabase";text:qsTr("Réparer la base");enabled:!controller.busy;ToolTip.visible:hovered;ToolTip.text:qsTr("Recopie ce qui se lit encore dans une base neuve. L’original est gardé dans les sauvegardes.");onClicked:controller.repairDatabase()}}
                            }
                        }
                        Rectangle {Layout.fillWidth:true;Layout.fillHeight:true;implicitHeight:children[0].implicitHeight+36;radius:6;color:win.panel;border.color:win.line
                            ColumnLayout {anchors.fill:parent;anchors.margins:18;spacing:8
                                Label {text:qsTr("Fichiers & exemples");font.pixelSize:Math.round(15*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.DemiBold}
                                Label {text:qsTr("Accédez aux fichiers locaux ou chargez les études de découverte.");wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted}
                                Item {Layout.fillHeight:true}
                                Flow {Layout.fillWidth:true;spacing:8;TButton {text:qsTr("Dossier local");onClicked:controller.openDataFolder()} TButton {text:qsTr("Exemples");onClicked:controller.loadExamples()}}
                            }
                        }
                    }
                    Rectangle {Layout.fillWidth:true;implicitHeight:children[0].implicitHeight+36;radius:6;color:"transparent";border.color:win.line
                        ColumnLayout {anchors.fill:parent;anchors.margins:18;spacing:6
                            Label {text:qsTr("Trait64 est un logiciel libre.");font.pixelSize:Math.round(14*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.DemiBold}
                            Label {text:qsTr("C++23 · Qt6 · chess-library · moteurs UCI externes. Code sous AGPL-3.0-or-later.\nVersion bêta : les fonctions sont là, elles s'affinent encore. Gardez des sauvegardes de vos bases.");color:win.muted;wrapMode:Text.WordWrap;Layout.fillWidth:true;font.pixelSize:Math.round(12*controller.preferences.fontScale*controller.preferences.uiScale)}
                        }
                    }
                    Item {height:20}
                }
            }
            Players {backend:controller;Layout.margins:0;onPreparePlayer:function(name){preparationDialog.usePlayer(name)};onImportOnline:accountsDialog.open()}
            ProfilePage {objectName:"profilePage";backend:controller}
            StudiesPage {objectName:"studiesPage";backend:controller;onOpenDetails:repertoiresDialog.open()}
        }
    }
    }
    footer:Rectangle {
        height:Math.max(32,32*win.textScale);color:win.bg
        Rectangle {width:parent.width;height:1;color:win.line}
        RowLayout {anchors.fill:parent;anchors.leftMargin:22;anchors.rightMargin:22
            Label {text:controller.repertoireTask.running?controller.repertoireTask.status+" · "+controller.repertoireTask.positions+" positions":controller.busy?"●  "+controller.taskStatus:controller.state.dirty?qsTr("●  Modifications · récupération de session automatique"):qsTr("●  Prêt");font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);color:win.muted;Layout.fillWidth:true;Layout.minimumWidth:0;elide:Text.ElideRight}
            TButton {visible:controller.busy;text:controller.resumableImport?qsTr("Mettre en pause"):qsTr("Interrompre");implicitHeight:Math.max(24,24*win.textScale);onClicked:controller.cancelTask()}
            Label {text:controller.state.training?qsTr("Entraînement local"):qsTr("Ctrl S  Enregistrer   ·   F  Retourner   ·   ← →  Parcourir");font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);color:win.muted;visible:win.width>=1350*win.textScale}
            // Une version qui attend se voit ici, même après avoir fermé la proposition :
            // « plus tard » ne doit pas vouloir dire « introuvable ».
            TButton {objectName:"versionButton";accent:controller.updater.state.available&&controller.updater.state.status!=="installed"
                     text:accent?qsTr("Mise à jour %1 disponible").arg(controller.updater.state.version):"Trait64 / "+controller.updater.state.current;flat:!accent;implicitHeight:Math.max(26,26*win.textScale);font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);onClicked:updatesDialog.open();ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Versions et mises à jour")}
        }
    }

    Timer {id:searchTimer;interval:220;onTriggered:win.filterLibrary()}
    Connections {target:controller
        // Les messages du contrôleur sont écrits en français dans le code ; on les
        // traduit à l'affichage, où ils passent tous, plutôt qu'à chacun des cent
        // soixante endroits qui les produisent. Un texte sans traduction revient tel quel.
        function onMessage(text,error){toastLabel.text=controller.uiText(text);toast.error=error;toast.open();toastTimer.restart()}
        function onPromotionNeeded(prefix){win.promotionPrefix=prefix;promotionDialog.open()}
        function onShowAnalysis(){win.page=2}
        function onShowAnalysisQueue(){analysisQueueDialog.open()}
    }
    // Pas de fermeture à Échap : un Popup qui l'accepte prend tous les raccourcis de la
    // fenêtre, et pendant les six secondes du message ni les flèches ni Ctrl+Z ne
    // répondaient plus. Un clic, sur le message ou ailleurs, l'écarte toujours.
    Popup {id:toast;objectName:"toast";parent:Overlay.overlay;property bool error:false;x:(parent.width-width)/2;y:Math.max(8,parent.height-height-34);width:Math.min(760,win.width-280);height:toastLabel.implicitHeight+topPadding+bottomPadding;padding:18;closePolicy:Popup.CloseOnPressOutside
        background:Rectangle {color:toast.error?"#633c37":win.raised;border.color:toast.error?"#a86559":win.line;radius:2}
        // Il se pose devant les boutons de navigation : au moins qu'un clic l'écarte, au
        // lieu d'obliger à attendre qu'il s'efface.
        contentItem:Label {id:toastLabel;wrapMode:Text.WordWrap;color:toast.error?"#ffffff":win.fg
            MouseArea {anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:toast.close()}}
    }
    Timer {id:toastTimer;interval:6500;onTriggered:toast.close()}
    Preparation {id:preparationDialog;objectName:"preparationDialog";backend:controller}
    OpeningReport {id:openingReportDialog;objectName:"openingReportDialog";parent:Overlay.overlay;anchors.centerIn:parent;backend:controller}
    Repertoires {id:repertoiresDialog;objectName:"repertoiresDialog";backend:controller}
    Imports {id:importsDialog;objectName:"importsDialog";backend:controller}
    Downloads {id:downloadsDialog;objectName:"downloadsDialog";backend:controller;onConfigureEngines:settingsDialog.open()}
    Native.FileDialog {id:sourceExportDialog;title:qsTr("Exporter cette base en PGN");fileMode:Native.FileDialog.SaveFile;defaultSuffix:"pgn";nameFilters:["PGN (*.pgn)"];onAccepted:controller.exportSource(selectedFile.toString(),baseBox.currentValue)}
    Native.FileDialog {id:importDialog;title:qsTr("Importer des parties");nameFilters:[qsTr("Parties, positions et bases (*.pgn *.t64 *.epd *.fen *.db3 *.si4 *.si5 *.cbh)"),qsTr("Tous les fichiers (*)")];onAccepted:labelImport.ask(selectedFile.toString())}
    Native.FileDialog {id:exportDialog;title:qsTr("Exporter en PGN ou archive T64 expérimentale");fileMode:Native.FileDialog.SaveFile;defaultSuffix:"pgn";nameFilters:["PGN (*.pgn)",qsTr("Page HTML autonome, étude ouverte (*.html)"),qsTr("Position EPD (*.epd)"),qsTr("Archive T64 expérimentale (*.t64)")];onAccepted:controller.exportFile(selectedFile.toString(),win.exportAll)}
    Native.FileDialog {id:diagnosticDialog;title:qsTr("Enregistrer le journal de diagnostic");fileMode:Native.FileDialog.SaveFile;defaultSuffix:"txt";nameFilters:[qsTr("Texte (*.txt)")];onAccepted:controller.exportDiagnostic(selectedFile.toString())}
    Native.FileDialog {id:backupDialog;title:qsTr("Créer une sauvegarde complète");fileMode:Native.FileDialog.SaveFile;defaultSuffix:"sqlite";nameFilters:[qsTr("Bibliothèque Trait64 (*.sqlite)")];onAccepted:controller.backup(selectedFile.toString())}
    Native.FileDialog {id:openDatabaseDialog;title:qsTr("Ouvrir une bibliothèque Trait64");nameFilters:[qsTr("Bibliothèque Trait64 (*.sqlite)")];onAccepted:controller.openBackup(selectedFile.toString())}
    Native.FileDialog {id:engineFileDialog;title:qsTr("Choisir l’exécutable Stockfish ou UCI");onAccepted:{controller.setPreference("enginePath",selectedFile.toString());enginePathField.text=controller.preferences.enginePath}}

    TDialog {id:fenDialog;objectName:"fenDialog";title:qsTr("Charger une position FEN");anchors.centerIn:parent;width:Math.min(650,win.width-32);height:Math.min(340,win.height-32);modal:true;standardButtons:Dialog.Ok|Dialog.Cancel
        ScrollView {id:fenDialogViewport;anchors.fill:parent;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
        ColumnLayout {width:fenDialogViewport.availableWidth;spacing:15
            Label {text:qsTr("Collez une FEN complète. L’étude en cours sera conservée dans votre bibliothèque.");wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted}
            TextArea {id:fenField;Layout.fillWidth:true;Layout.preferredHeight:95;wrapMode:TextEdit.Wrap;selectByMouse:true;placeholderText:"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"}
        }
        }
        onAccepted:controller.setFen(fenField.text)
    }
    TDialog {id:pgnDialog;objectName:"pgnDialog";title:qsTr("Importer un PGN ou un lien Lichess");anchors.centerIn:parent;width:Math.min(700,win.width-32);height:Math.min(550,win.height-32);modal:true;standardButtons:Dialog.Ok|Dialog.Cancel
        ScrollView {anchors.fill:parent;TextArea {id:pgnField;placeholderText:"[Event \"Mon étude\"]\n\n1. e4 e5 2. Nf3 *\n\nhttps://lichess.org/AbCd1234\nhttps://lichess.org/study/AbCd1234";wrapMode:TextEdit.Wrap;selectByMouse:true}}
        onAccepted:controller.importText(pgnField.text)
    }
    TDialog {id:promotionDialog;objectName:"promotionDialog";width:Math.min(650,win.width-32);title:qsTr("Choisir la promotion");anchors.centerIn:parent;modal:true
        Flow {width:parent.width;spacing:12;Repeater {model:[{label:qsTr("♛ Dame"),piece:"q"},{label:qsTr("♜ Tour"),piece:"r"},{label:qsTr("♝ Fou"),piece:"b"},{label:qsTr("♞ Cavalier"),piece:"n"}]
            delegate:TButton {required property var modelData;text:modelData.label;onClicked:{controller.move(win.promotionPrefix+modelData.piece);promotionDialog.close()}}
        }}
    }
    TDialog {id:cardDialog;objectName:"cardDialog";title:qsTr("Créer une carte d’entraînement");anchors.centerIn:parent;width:Math.min(620,win.width-32);height:Math.min(360,win.height-32);modal:true;standardButtons:Dialog.Ok|Dialog.Cancel
        ScrollView {id:cardDialogViewport;anchors.fill:parent;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
        ColumnLayout {width:cardDialogViewport.availableWidth;spacing:14
            Label {text:qsTr("La suite principale depuis la position actuelle sera à retrouver (40 demi-coups maximum). Placez-vous avant le premier coup à apprendre.");color:win.muted;wrapMode:Text.WordWrap;Layout.fillWidth:true}
            TTextField {id:cardTitle;placeholderText:qsTr("Ex. Italienne — mon plan avec les Blancs");Layout.fillWidth:true}
        }
        }
        onAccepted:controller.createCard(cardTitle.text)
    }
    TDialog {id:deleteCardDialog;objectName:"deleteCardDialog";width:Math.min(580,win.width-32);property int cardId:0;title:qsTr("Supprimer cette carte ?");anchors.centerIn:parent;modal:true;standardButtons:Dialog.Yes|Dialog.No
        Label {text:qsTr("L’activité déjà enregistrée restera dans votre progression.");width:parent.width;wrapMode:Text.WordWrap}
        onAccepted:controller.deleteCard(cardId)
    }
    TDialog {id:playDialog;objectName:"playDialog";title:qsTr("Partie locale");anchors.centerIn:parent;width:Math.min(620,win.width-32);height:Math.min(600,win.height-32);modal:true
        ScrollView {id:playDialogViewport;anchors.fill:parent;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
        ColumnLayout {width:playDialogViewport.availableWidth;spacing:16
            TComboBox {id:opponent;model:["Deux joueurs sur cet ordinateur","Contre le moteur UCI"];currentIndex:1;Layout.fillWidth:true}
            GridLayout {columns:2;Layout.fillWidth:true;Label {text:qsTr("Minutes")} TSpinBox {id:clockMinutes;from:1;to:180;value:10;editable:true} Label {text:qsTr("Incrément (s)")} TSpinBox {id:clockIncrement;from:0;to:120;value:5;editable:true}}
            TComboBox {id:playSide;model:["Je joue les Blancs","Je joue les Noirs"];Layout.fillWidth:true}
            RowLayout {Layout.fillWidth:true;visible:opponent.currentIndex===1;spacing:10
                TCheckBox {id:limitElo;objectName:"limitElo";text:qsTr("Limiter la force à");checked:controller.preferences.playElo>0
                    onToggled:controller.setPreference("playElo",checked?playEloValue.value:0)}
                TSpinBox {id:playEloValue;objectName:"playElo";from:800;to:3200;stepSize:50;editable:true;enabled:limitElo.checked
                    value:controller.preferences.playElo>0?controller.preferences.playElo:1500
                    onValueModified:if(limitElo.checked)controller.setPreference("playElo",value)}
                Label {text:qsTr("Elo");color:win.muted}
            }
            TButton {text:qsTr("Démarrer avec pendule");accent:true;Layout.fillWidth:true;onClicked:{controller.startLocalGame(clockMinutes.value,clockIncrement.value,opponent.currentIndex,playSide.currentIndex===0);playDialog.close()}}
            TButton {text:qsTr("Moteur sans pendule");visible:opponent.currentIndex===1;Layout.fillWidth:true;onClicked:{controller.playComputer(playSide.currentIndex===0);playDialog.close()}}
            Label {text:qsTr("La partie démarre depuis la position de l’échiquier. Parcourir les coups arrête la pendule.");wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
        }
        }
    }
    TDialog {id:tagsDialog;objectName:"tagsDialog";title:qsTr("Informations sur la partie");anchors.centerIn:parent;width:Math.min(700,win.width-32);height:Math.min(740,win.height-32);modal:true;standardButtons:Dialog.Close
        ScrollView {id:tagsDialogViewport;anchors.fill:parent;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
        GridLayout {width:tagsDialogViewport.availableWidth;columns:2;columnSpacing:16;rowSpacing:10
            Repeater {model:["White","Black","Event","Site","Date","Round","Result","ECO","WhiteElo","BlackElo"]
                delegate:ColumnLayout {required property string modelData;Layout.fillWidth:true;spacing:4
                    Label {text:({White:qsTr("Blancs"),Black:qsTr("Noirs"),Event:qsTr("Événement"),Site:qsTr("Lieu"),Date:qsTr("Date (AAAA.MM.JJ)"),Round:qsTr("Ronde"),Result:qsTr("Résultat : 1-0 / 0-1 / 1/2-1/2 / *"),ECO:qsTr("Code ECO"),WhiteElo:qsTr("Elo Blancs"),BlackElo:qsTr("Elo Noirs")})[modelData];font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);color:win.muted;wrapMode:Text.WordWrap;Layout.fillWidth:true}
                    TTextField {text:controller.state.tags[modelData]||"";Layout.fillWidth:true;onEditingFinished:{var t={};t[modelData]=text;controller.setTags(t)}}
                }
            }
        }
        }
    }
    Menu {id:analysisMenu;x:win.width-width-32;y:172
        MenuItem {text:qsTr("Analyses en arrière-plan");onTriggered:analysisQueueDialog.open()}
        MenuItem {text:qsTr("Comparer plusieurs moteurs");onTriggered:compareDialog.open()}
        MenuItem {text:qsTr("Tournoi de moteurs");onTriggered:tournamentDialog.open()}
        MenuItem {text:qsTr("Éditer la position");onTriggered:positionEditor.open()}
        MenuItem {text:qsTr("Copier la position FEN");onTriggered:controller.copyText("fen")}
        MenuItem {text:controller.state.playing?qsTr("Arrêter la partie"):qsTr("Jouer une partie");onTriggered:controller.state.playing?controller.stopPlaying():playDialog.open()}
        MenuItem {text:qsTr("Nouvelle partie Chess960");onTriggered:chess960Dialog.open()}
        MenuItem {text:qsTr("Dupliquer l’étude");onTriggered:controller.duplicateStudy()}
        MenuItem {text:qsTr("Identifier et renseigner l’ouverture");onTriggered:controller.identifyOpening()}
        MenuItem {text:qsTr("Chercher la nouveauté");onTriggered:controller.findNovelty()}
        MenuItem {text:qsTr("Exporter le diagramme (PNG ou SVG)");onTriggered:diagramDialog.open()}
        MenuItem {text:qsTr("Exporter l’étude en PDF");onTriggered:pdfOptions.open()}
        MenuItem {text:qsTr("Effacer les flèches et cercles");onTriggered:controller.clearAnnotations()}
        MenuItem {text:qsTr("Abandonner la partie");visible:controller.state.playing;onTriggered:controller.resignGame()}
        MenuItem {text:qsTr("Réclamer la nulle");visible:controller.state.playing;onTriggered:controller.claimDraw()}
        MenuItem {text:qsTr("Informations de la partie");onTriggered:tagsDialog.open()}
        MenuItem {text:qsTr("Exporter cette étude");onTriggered:win.openExport(false)}
        MenuItem {text:qsTr("Copier le PGN");onTriggered:controller.copyText("pgn")}
        MenuItem {text:qsTr("Créer une carte depuis ici");onTriggered:cardDialog.open()}
        MenuSeparator {}
        MenuItem {text:qsTr("Promouvoir cette variante");enabled:controller.state.node>0;onTriggered:controller.promote()}
        MenuItem {text:qsTr("Supprimer ce coup et sa suite…");enabled:controller.state.node>0;onTriggered:deleteVariationDialog.open()}
    }
    TDialog {id:deleteVariationDialog;objectName:"deleteVariationDialog";width:Math.min(580,win.width-32);title:qsTr("Supprimer cette variante ?");anchors.centerIn:parent;modal:true;standardButtons:Dialog.Yes|Dialog.No
        Label {text:qsTr("Le coup sélectionné et ses continuations seront retirés.");width:parent.width;wrapMode:Text.WordWrap}
        onAccepted:controller.deleteVariation()
    }
    Menu {id:gameMenu;x:win.width-width-40;y:220
        MenuItem {text:qsTr("Tableau du tournoi");onTriggered:crosstableDialog.show(win.selectedGame)}
        MenuItem {text:qsTr("Fusionner dans l’étude courante");onTriggered:controller.mergeGame(win.selectedGame)}
        MenuItem {text:qsTr("Modifier la collection");onTriggered:collectionDialog.open()}
        Menu {id:medalMenu;objectName:"medalMenu";title:qsTr("Médailles")
            Instantiator {model:win.medalNames.length
                delegate:MenuItem {required property int index;text:win.medalNames[index];checkable:true;checked:(win.selectedMedals&(1<<index))!==0
                    onTriggered:{win.selectedMedals^=(1<<index);controller.setMedals(win.selectedGame,win.selectedMedals)}}
                onObjectAdded:function(index,object){medalMenu.insertItem(index,object)}
                onObjectRemoved:function(index,object){medalMenu.removeItem(object)}
            }
        }
        MenuItem {text:trashCheck.checked?qsTr("Restaurer la partie"):qsTr("Mettre à la corbeille");onTriggered:controller.trash(win.selectedGame,!trashCheck.checked)}
    }
    // Un PGN ou une base, avant l'import : son étiquette (la collection de ses parties, un
    // recueil de la bibliothèque) et ce qu'il contient. Les positions et les archives
    // Trait64 s'importent sans question.
    TDialog {id:labelImport;objectName:"labelImportDialog";title:qsTr("Importer");anchors.centerIn:parent;modal:true;width:Math.min(520,win.width-32);standardButtons:Dialog.Ok|Dialog.Cancel
        property string url:""
        function ask(file) {
            if (!/\.(pgn|db3|si[45]|sn[45]|sg[45]|cbh|cbg|cbp|cbt)$/i.test(file)) { controller.importFile(file); return }
            url = file
            var name = decodeURIComponent(file.substring(file.lastIndexOf("/") + 1))
            labelField.text = name.substring(0, name.lastIndexOf(".")) || name
            studyKind.checked = false
            open()
        }
        ColumnLayout {anchors.fill:parent;spacing:8
            Label {text:qsTr("Étiquette");font.weight:Font.DemiBold}
            TTextField {id:labelField;objectName:"labelImportField";Layout.fillWidth:true;placeholderText:qsTr("Ex. Tournoi de Paris 2026, Cours de finales…")}
            Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;opacity:.7;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)
                text:qsTr("Elle devient la collection de ces parties : la bibliothèque les retrouve par elle. Laissez-la vide pour importer sans étiquette.")}
            // Le libellé revient à la ligne : traduit ou en grand, il dépassait du dialogue.
            TCheckBox {id:studyKind;objectName:"labelImportStudy";Layout.fillWidth:true;text:qsTr("Ce sont des études ou des cours, pas des parties jouées")
                contentItem:Label {text:studyKind.text;font:studyKind.font;wrapMode:Text.WordWrap;verticalAlignment:Text.AlignVCenter
                    leftPadding:studyKind.indicator.width+studyKind.spacing}
                ToolTip.visible:hovered;ToolTip.text:qsTr("Les noms qu’ils portent à la place des joueurs (chapitres, titres) n’entrent pas dans l’onglet Joueurs.")}
        }
        onAccepted:controller.importLabeled(url,labelField.text,studyKind.checked)
    }
    TDialog {id:collectionDialog;objectName:"collectionDialog";title:qsTr("Collection de la partie");anchors.centerIn:parent;modal:true;width:440;standardButtons:Dialog.Ok|Dialog.Cancel
        TTextField {id:collectionField;anchors.fill:parent;placeholderText:qsTr("Ex. Préparation tournoi, Répertoire Blancs…")}
        onAccepted:controller.setCollection(win.selectedGame,collectionField.text)
    }
    PositionEditor {id:positionEditor;objectName:"positionEditor";backend:controller}
    AnalysisQueue {id:analysisQueueDialog;objectName:"analysisQueueDialog";backend:controller}
    Review {id:reviewDialog;objectName:"reviewDialog";backend:controller}
    CompareEngines {id:compareDialog;objectName:"compareDialog";backend:controller}
    Tournament {id:tournamentDialog;objectName:"tournamentDialog";backend:controller}
    Puzzles {id:puzzlesDialog;objectName:"puzzlesDialog";backend:controller}
    Coordinates {id:coordinatesDialog;objectName:"coordinatesDialog"}
    Endgames {id:endgamesDialog;objectName:"endgamesDialog";backend:controller}
    Shortcuts {id:shortcutsDialog;objectName:"shortcutsDialog";backend:controller}
    Backups {id:backupsDialog;objectName:"backupsDialog";backend:controller}
    Crosstable {id:crosstableDialog;objectName:"crosstableDialog";backend:controller}
    TDialog {id:layoutsDialog;objectName:"layoutsDialog";title:qsTr("Dispositions de travail");modal:true;anchors.centerIn:parent
        width:Math.min(460,parent.width-32);standardButtons:Dialog.Close
        ColumnLayout {anchors.fill:parent;spacing:8
            Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;color:win.muted
                text:qsTr("Une disposition retient la place de l’échiquier et celle de chaque panneau de l’analyse.")}
            RowLayout {Layout.fillWidth:true;spacing:6
                TTextField {id:layoutName;objectName:"layoutName";Layout.fillWidth:true;placeholderText:qsTr("Nom, par exemple « Préparation »")}
                TButton {objectName:"saveLayout";text:qsTr("Enregistrer");accent:true;enabled:layoutName.text.trim().length>0
                    onClicked:{controller.saveLayout(win.currentLayout(layoutName.text));layoutName.text=""}}
            }
            Repeater {model:controller.layouts
                RowLayout {required property var modelData;required property int index;Layout.fillWidth:true;spacing:6
                    Label {text:modelData.name;Layout.fillWidth:true;elide:Text.ElideRight}
                    TButton {text:qsTr("Appliquer");onClicked:{win.applyLayout(modelData);layoutsDialog.close()}}
                    TButton {text:qsTr("Supprimer");flat:true;onClicked:controller.removeLayout(index)}
                }
            }
        }
    }
    Native.FileDialog {id:compareLibraryDialog;title:qsTr("Base Trait64 à comparer");nameFilters:[qsTr("Bibliothèques Trait64 (*.sqlite)"),qsTr("Tous les fichiers (*)")];onAccepted:controller.compareLibrary(selectedFile.toString())}
    Names {id:namesDialog;objectName:"namesDialog";backend:controller}
    Live {id:liveDialog;objectName:"liveDialog";backend:controller}
    Plans {id:plansDialog;objectName:"plansDialog";backend:controller}
    Similar {id:similarDialog;objectName:"similarDialog";backend:controller}
    // La suppression d'une base, confirmée : son nom, son nombre de parties, ce qui part à
    // la corbeille et ce qui reste.
    TDialog {id:removeBaseDialog;objectName:"removeBaseDialog";title:qsTr("Supprimer la base");anchors.centerIn:parent;modal:true
        width:Math.min(480,parent.width-32);standardButtons:Dialog.Cancel|Dialog.Ok
        property double baseId:0
        property var base:({})
        function ask(id){baseId=id;base=controller.sources.find(function(s){return s.id===id})||({});open()}
        onAccepted:{controller.removeBase(baseId);win.selectedBaseId=0;baseBox.currentIndex=0;win.filterLibrary()}
        ColumnLayout {width:parent.width;spacing:8
            Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;font.weight:Font.DemiBold;text:removeBaseDialog.base.title||""}
            Label {Layout.fillWidth:true;wrapMode:Text.WordWrap
                text:qsTr("Ses %1 partie(s) vont à la corbeille de la bibliothèque, d’où vous pouvez les restaurer. Celles qu’une autre base contient aussi restent en place.").arg(removeBaseDialog.base.games||0)}
            Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;color:win.muted;visible:/^(lichess|chesscom):/.test(removeBaseDialog.base.slug||"")
                text:qsTr("Cette base est alimentée par un compte en ligne : retirez aussi le compte, sinon ses parties reviendront à la prochaine synchronisation.")}
        }
    }
    Stoyko {id:stoykoDialog;objectName:"stoykoDialog";backend:controller}
    PositionQr {id:positionQr;objectName:"positionQrDialog";backend:controller}
    LibraryCharts {id:libraryCharts;objectName:"libraryChartsDialog";backend:controller}
    TournamentFinder {id:tournamentFinder;objectName:"tournamentFinder";backend:controller;crosstable:crosstableDialog}
    TDialog {id:saveSearchDialog;objectName:"saveSearchDialog";title:qsTr("Enregistrer cette recherche");anchors.centerIn:parent;modal:true;width:Math.min(440,win.width-32);standardButtons:Dialog.Ok|Dialog.Cancel
        onOpened:{searchName.text=searchField.text||"";searchName.forceActiveFocus()}
        onAccepted:controller.saveSearch(searchName.text,win.currentSearch())
        ColumnLayout {anchors.fill:parent
            Label {text:qsTr("Son nom, pour la retrouver dans le menu Recherches.");wrapMode:Text.WordWrap;Layout.fillWidth:true}
            TTextField {id:searchName;objectName:"searchName";Layout.fillWidth:true;onAccepted:saveSearchDialog.accept()}
        }
    }
    Guide {id:guideDialog;objectName:"guideDialog";backend:controller}
    // Au tout premier lancement, le guide s'ouvre sur « Premiers pas » ; ensuite il reste
    // dans Aide. Les captures et tests (--smoke-test) ne le voient pas.
    Timer {interval:900;running:!Qt.application.arguments.includes("--smoke-test");onTriggered:if(controller.firstLaunch())guideDialog.openAt(0)}
    Accounts {id:accountsDialog;objectName:"accountsDialog";backend:controller}
    SkillDashboard {id:skillDialog;objectName:"skillDashboardDialog";backend:controller}
    Native.FileDialog {id:polyglotDialog;title:qsTr("Exporter un livre Polyglot");fileMode:Native.FileDialog.SaveFile;defaultSuffix:"bin";nameFilters:[qsTr("Livre Polyglot (*.bin)")];onAccepted:controller.exportPolyglot(selectedFile,2)}
    Native.FileDialog {id:diagramDialog;title:qsTr("Exporter le diagramme");fileMode:Native.FileDialog.SaveFile;defaultSuffix:"png";nameFilters:["Image PNG (*.png)","Image vectorielle SVG (*.svg)"];onAccepted:controller.exportDiagram(selectedFile.toString())}
    // Ce que le rapport PDF contient, choisi avant le fichier.
    TDialog {id:pdfOptions;objectName:"pdfOptions";title:qsTr("Rapport PDF de l’étude");modal:true;anchors.centerIn:parent;width:Math.min(420,parent.width-32)
        standardButtons:Dialog.Ok|Dialog.Cancel
        readonly property var options:({headers:pdfHeaders.checked,comments:pdfComments.checked,variations:pdfVariations.checked,diagrams:["none","current","marked","every"][pdfDiagrams.currentIndex]})
        onAccepted:pdfDialog.open()
        ColumnLayout {anchors.fill:parent;spacing:8
            TCheckBox {id:pdfHeaders;text:qsTr("En-têtes de la partie");checked:true}
            TCheckBox {id:pdfComments;text:qsTr("Commentaires");checked:true}
            TCheckBox {id:pdfVariations;text:qsTr("Variantes");checked:true}
            Label {text:qsTr("Diagrammes")}
            TComboBox {id:pdfDiagrams;objectName:"pdfDiagrams";Layout.fillWidth:true;currentIndex:1
                model:[qsTr("Aucun"),qsTr("La position affichée"),qsTr("Aux coups marqués d’un diagramme"),qsTr("Aux coups marqués et tous les dix coups")]}
        }
    }
    Native.FileDialog {id:pdfDialog;title:qsTr("Exporter l’étude en PDF");fileMode:Native.FileDialog.SaveFile;defaultSuffix:"pdf";nameFilters:["PDF (*.pdf)"];onAccepted:controller.exportStudyPdf(selectedFile.toString(),pdfOptions.options)}
    TDialog {id:chess960Dialog;objectName:"chess960Dialog";title:qsTr("Nouvelle étude Chess960");anchors.centerIn:parent;modal:true;standardButtons:Dialog.Ok|Dialog.Cancel
        ColumnLayout {Label {text:qsTr("Numéro de la position (518 = position classique)")} TSpinBox {id:frcNumber;from:0;to:959;value:518;editable:true}}
        onAccepted:controller.newChess960(frcNumber.value)
    }
    Appearance {id:appearanceDialog;objectName:"appearanceDialog";backend:controller}
    TDialog {id:settingsDialog;objectName:"settingsDialog";title:qsTr("Paramètres");anchors.centerIn:parent;width:700;height:Math.min(690,win.height-32);modal:true;standardButtons:Dialog.Close
        ScrollView {id:settingsScroll;anchors.fill:parent;contentWidth:availableWidth;clip:true
        ColumnLayout {width:settingsScroll.availableWidth;spacing:18
            Label {text:qsTr("Moteur UCI local");font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);color:win.muted}
            RowLayout {Layout.fillWidth:true;TTextField {id:enginePathField;Layout.fillWidth:true;text:controller.preferences.enginePath;placeholderText:"/usr/games/stockfish";onEditingFinished:controller.setPreference("enginePath",text)} TButton {objectName:"detectEngine";text:qsTr("Détecter");onClicked:{var found=controller.useDetectedEngine();if(found!=="")enginePathField.text=found}
                         ToolTip.visible:hovered;ToolTip.text:qsTr("Cherche Stockfish et les autres moteurs dans les dossiers du système, dans Téléchargements et dans votre dossier personnel.")}
                TButton {text:qsTr("Parcourir");onClicked:engineFileDialog.open()}}
            TCheckBox {text:qsTr("Sauvegardes automatiques (14 instantanés)");checked:controller.preferences.autoBackup;onToggled:controller.setPreference("autoBackup",checked)}
            TButton {text:qsTr("Créer une sauvegarde automatique maintenant");onClicked:controller.automaticBackup()}
            GridLayout {columns:settingsScroll.availableWidth>700*win.textScale?4:2;columnSpacing:14;rowSpacing:10;Layout.fillWidth:true
                Label {text:qsTr("Fils de calcul");color:win.muted;ToolTip.visible:threadsHover.hovered;ToolTip.text:qsTr("Votre processeur offre %1 cœurs. Trait64 en laisse un libre pour que la machine reste fluide pendant l’analyse.").arg(controller.preferences.cpuCores);HoverHandler {id:threadsHover}}
                TSpinBox {from:1;to:1024;value:controller.preferences.threads;onValueModified:controller.setPreference("threads",value)}
                Label {text:qsTr("Mémoire (Mio)");color:win.muted;ToolTip.visible:hashHover.hovered;ToolTip.text:qsTr("Table de hachage du moteur. Votre machine dispose de %1 Mio.").arg(Math.round(controller.preferences.memoryMb));HoverHandler {id:hashHover}}
                TSpinBox {from:16;to:262144;stepSize:64;value:controller.preferences.hash;onValueModified:controller.setPreference("hash",value)}
                Label {text:qsTr("Variantes");color:win.muted} TSpinBox {from:1;to:5;value:controller.preferences.multiPV;onValueModified:controller.setPreference("multiPV",value)}
                Label {text:qsTr("Niveau de jeu");color:win.muted} TSpinBox {from:0;to:20;value:controller.preferences.skill;onValueModified:controller.setPreference("skill",value)}
                Label {text:qsTr("Réflexion (ms)");color:win.muted} TSpinBox {from:100;to:10000;stepSize:100;value:controller.preferences.moveTime;onValueModified:controller.setPreference("moveTime",value)}
            }
            Label {text:qsTr("Le niveau concerne le jeu contre Stockfish. L’analyse utilise sa pleine force.\nLes réglages sont appliqués au démarrage du moteur.");color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);wrapMode:Text.WordWrap;Layout.fillWidth:true}
            RowLayout {Label {text:qsTr("Profondeur d’analyse");color:win.muted} TSpinBox {objectName:"analysisDepth";from:0;to:99;value:controller.preferences.analysisDepth;onValueModified:controller.setPreference("analysisDepth",value)
                        // 0 : le moteur creuse tant qu'on regarde la position.
                        // Le texte affiché est une liaison à part : le SpinBox de Qt 6.4 ne
                        // recalcule pas son texte quand la langue change, et « no limit »
                        // restait affiché en français.
                        id:analysisDepthBox
                        textFromValue:function(value){return value===0?qsTr("sans limite"):String(value)}
                        contentItem:Label {text:analysisDepthBox.value===0?qsTr("sans limite"):String(analysisDepthBox.value);font:analysisDepthBox.font
                            horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;color:win.fg}
                        valueFromText:function(text){var n=parseInt(text);return isNaN(n)?0:n}
                        ToolTip.visible:hovered;ToolTip.text:qsTr("Sans limite, le moteur approfondit tant que la position reste affichée. Une profondeur fixe l’arrête dès qu’elle est atteinte.")}}
            RowLayout {Layout.fillWidth:true
                TTextField {Layout.fillWidth:true;text:controller.preferences.syzygyPath;placeholderText:qsTr("Dossier des tablebases Syzygy locales (optionnel)");onEditingFinished:controller.setPreference("syzygyPath",text);selectByMouse:true}
                TButton {text:qsTr("Parcourir");onClicked:syzygyFolder.open()}
                Native.FolderDialog {id:syzygyFolder;title:qsTr("Dossier des tables Syzygy");onAccepted:controller.setPreference("syzygyPath",selectedFolder.toString())}
            }
            Label {objectName:"syzygyStatus";Layout.fillWidth:true;wrapMode:Text.WordWrap;color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);text:(controller.syzygyDownload.running,controller.syzygyStatus(controller.preferences.syzygyPath))}
            // Télécharger les tables : chaque fichier est vérifié par la somme qu'il porte, et
            // un téléchargement interrompu reprend où il s'était arrêté.
            RowLayout {Layout.fillWidth:true;visible:!controller.syzygyDownload.running
                readonly property var pack:(controller.syzygyDownload.running,controller.preferences.syzygyPath,controller.syzygyPack("3-4-5"))
                TButton {objectName:"syzygySmall";text:qsTr("Télécharger 3 à 5 pièces (939 Mio)");onClicked:controller.downloadSyzygy("3-4-5")}
                TButton {text:qsTr("6 pièces (149 Gio)");onClicked:controller.downloadSyzygy("6")
                         ToolTip.visible:hovered;ToolTip.text:qsTr("Plusieurs jours de téléchargement selon la connexion. Les 3 à 5 pièces suffisent à la plupart des finales.")}
                Label {Layout.fillWidth:true;elide:Text.ElideMiddle;color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)
                       text:qsTr("%1 Gio libres dans %2").arg((parent.pack.free/1073741824).toFixed(1)).arg(parent.pack.folder)}
            }
            RowLayout {Layout.fillWidth:true;visible:!!controller.syzygyDownload.running
                ProgressBar {Layout.fillWidth:true;from:0;to:Math.max(1,controller.syzygyDownload.total||0);value:controller.syzygyDownload.done||0}
                Label {color:win.muted;text:qsTr("%1 / %2 fichiers · %3 Mio reçus").arg(controller.syzygyDownload.done||0).arg(controller.syzygyDownload.total||0).arg(Math.round((controller.syzygyDownload.received||0)/1048576))}
                TButton {text:qsTr("Interrompre");onClicked:controller.cancelSyzygyDownload()}
            }
            TTextField {objectName:"bookPathField";Layout.fillWidth:true;text:controller.preferences.bookPath;placeholderText:qsTr("Livre d'ouvertures Polyglot .bin (optionnel)");onEditingFinished:controller.setPreference("bookPath",text);selectByMouse:true}
            RowLayout {Layout.fillWidth:true
                TTextField {id:profileName;Layout.fillWidth:true;placeholderText:qsTr("Nom du profil moteur")}
                TButton {text:qsTr("Mémoriser");onClicked:controller.saveEngineProfile(profileName.text)}
            }
            RowLayout {Layout.fillWidth:true
                TComboBox {translateOptions:false;id:profileBox;Layout.fillWidth:true;model:controller.preferences.engineProfiles;textRole:"name"}
                TButton {text:qsTr("Charger");enabled:profileBox.currentIndex>=0;onClicked:controller.loadEngineProfile(profileBox.currentIndex)}
                TButton {text:"×";enabled:profileBox.currentIndex>=0;onClicked:controller.removeEngineProfile(profileBox.currentIndex)}
            }
            RowLayout {TButton {text:qsTr("Démarrer / appliquer");accent:true;onClicked:controller.startEngine()} Label {text:win.systemText(controller.engineStatus);elide:Text.ElideRight;Layout.fillWidth:true;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);color:win.muted}}
            Rectangle {Layout.fillWidth:true;height:1;color:win.line}
            RowLayout {TCheckBox {text:qsTr("Thème clair");checked:controller.preferences.lightTheme;onToggled:controller.setPreference("lightTheme",checked)} TCheckBox {text:qsTr("Coordonnées de l’échiquier");checked:controller.preferences.coordinates;onToggled:controller.setPreference("coordinates",checked)}}
            TButton {text:qsTr("Télécharger d’autres moteurs");onClicked:{settingsDialog.close();downloadsDialog.open()}}
            EngineOptions {backend:controller}
            Label {text:qsTr("Vos données : ")+controller.preferences.dataPath;wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted;font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale)}
        }
    }
    }
    TDialog {id:helpDialog;objectName:"helpDialog";title:qsTr("Bienvenue dans Trait64");anchors.centerIn:parent;width:Math.min(760,win.width-32);height:Math.min(700,win.height-32);modal:true;standardButtons:Dialog.Close
        ScrollView {id:helpScroll;anchors.fill:parent;contentWidth:availableWidth;clip:true
        ColumnLayout {width:helpScroll.availableWidth;spacing:15
            Label {objectName:"helpTitle";Layout.fillWidth:true;Layout.minimumWidth:0;wrapMode:Text.WordWrap;text:qsTr("Un atelier personnel pour vos échecs.");font.pixelSize:Math.round(21*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.DemiBold}
            Label {text:qsTr("• Cliquez une pièce puis sa case, ou glissez-la.\n• Revenez dans une partie et jouez un autre coup pour créer une variante.\n• Clic droit glissé : flèche ; clic droit sur une case : cercle ;\n  double clic, gauche ou droit : effacer toutes les annotations.\n• Ctrl + molette au-dessus de l’échiquier : régler sa taille.\n• Flèches gauche/droite : coups ; haut/bas : variantes ; Espace : lecture automatique.\n• Clic droit sur un coup ou touche Suppr : le supprimer avec sa suite (Ctrl+Z pour annuler).\n• Enregistrez une étude pour l’ajouter à votre bibliothèque.\n• Importez un PGN pour alimenter l’explorateur d’ouvertures.\n• Créez des cartes à partir des lignes que vous souhaitez retenir.");wrapMode:Text.WordWrap;Layout.fillWidth:true;lineHeight:1.5}
            Label {text:qsTr("Ctrl N : nouvelle étude    ·    Ctrl O : importer\nCtrl S : enregistrer    ·    Ctrl K : commandes\n← / → : précédent / suivant    ·    F : retourner\nDébut / Fin : début / fin de la ligne");Layout.fillWidth:true;wrapMode:Text.WordWrap;color:win.muted;lineHeight:1.6}
            Label {text:qsTr("Version %1. Les sauvegardes PGN et SQLite sont recommandées pour vos études importantes. Le format .t64 est expérimental.").arg(Qt.application.version);wrapMode:Text.WordWrap;Layout.fillWidth:true;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);color:win.muted}
        }}
    }
    TDialog {id:advancedDialog;objectName:"advancedDialog";title:qsTr("Recherche avancée");anchors.centerIn:parent;width:Math.min(690,win.width-32);height:Math.min(760,win.height-32);modal:true;standardButtons:Dialog.Ok|Dialog.Cancel
        property var draft:({})
        property int themesPending:0
        onOpened:{draft=Object.assign({},win.advanced);minElo.value=draft.minElo||0;maxElo.value=draft.maxElo||0;maxPlies.value=draft.maxPlies||0;sortBy.currentIndex=["updated","date","players","elo","length"].indexOf(draft.sort||"updated");colourFilter.currentIndex=Math.max(0,["","white","black"].indexOf(draft.colour||""));nagFilter.currentIndex=Math.max(0,[0,3,1,5,6,2,4].indexOf(draft.nag||0));medalFilter.currentIndex=draft.medal?Math.round(Math.log2(draft.medal))+1:0;endgameFilter.currentIndex=Math.max(0,endgameFilter.keys.indexOf(draft.endgame||""));themeFilter.currentIndex=win.themeKeys.indexOf(draft.theme||"")+1;themesPending=controller.gamesWithoutThemes()}
        ScrollView {id:advancedScroll;anchors.fill:parent;contentWidth:availableWidth;clip:true
        GridLayout {width:advancedScroll.availableWidth;columns:2;columnSpacing:12;rowSpacing:10
            Repeater {id:fieldRepeater;model:[{key:"player",label:qsTr("Joueur (selon la couleur ci-dessous)")},{key:"openingName",label:qsTr("Ouverture : Française, Najdorf… → ECO")},{key:"white",label:qsTr("Blancs")},{key:"black",label:qsTr("Noirs")},{key:"event",label:qsTr("Tournoi / événement")},{key:"eco",label:qsTr("Code ECO ou fragment")},{key:"ecoFrom",label:qsTr("ECO de (ex. B20)")},{key:"ecoTo",label:qsTr("ECO à (ex. B99)")},{key:"collection",label:qsTr("Collection personnelle")},{key:"material",label:qsTr("Matériel exact (ex. KRkrp)")},{key:"placement",label:qsTr("Pièces sur des cases (ex. Ke1 Pd4 pd5 ; majuscule = Blancs ; lent : rejoue toutes les parties)")},{key:"dateFrom",label:qsTr("À partir de (AAAA.MM.JJ)")},{key:"dateTo",label:qsTr("Jusqu’au (AAAA.MM.JJ)")},{key:"comment",label:qsTr("Texte dans les commentaires")},{key:"maneuver",label:qsTr("Manœuvre d’une pièce (ex. Cf3-d2-f1)")},{key:"cql",label:qsTr("Requête CQL (ex. « Ra8 kg8 mate » ; lent : rejoue chaque partie, variantes comprises)")}]
                delegate:ColumnLayout {required property var modelData;Layout.fillWidth:true
                    Label {text:modelData.label;wrapMode:Text.WordWrap;Layout.fillWidth:true;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                    TTextField {Layout.fillWidth:true;text:advancedDialog.draft[modelData.key]||"";onTextEdited:advancedDialog.draft[modelData.key]=text}
                }
            }
            // Les champs sont en nombre impair : une case vide remet chaque libellé suivant en
            // face de sa liste.
            Item {Layout.fillWidth:true;visible:fieldRepeater.count%2===1}
            Label {text:qsTr("Couleur du joueur")} TComboBox {id:colourFilter;Layout.fillWidth:true;model:[qsTr("Les deux couleurs"),qsTr("Blancs"),qsTr("Noirs")]}
            // Les six symboles courants, dans l'ordre du plus fort au plus faible ; la valeur
            // est le NAG du PGN ($3 pour !!).
            Label {text:qsTr("Finale atteinte")} TComboBox {id:endgameFilter;objectName:"endgameFilter";Layout.fillWidth:true
                readonly property var keys:["","pawns","knights","bishops","knightBishop","minor","rooks","rookMinor","rookMinorRook","doubleRooks","queens","queenRook"]
                model:[qsTr("Aucune"),qsTr("Pions seuls"),qsTr("Cavalier contre cavalier"),qsTr("Fou contre fou"),qsTr("Cavalier contre fou"),qsTr("Pièces mineures"),
                    qsTr("Tour contre tour"),qsTr("Tour contre pièce mineure"),qsTr("Tour et pièce mineure contre tour"),qsTr("Deux tours contre deux tours"),qsTr("Dame contre dame"),qsTr("Dame contre tour")]}
            Label {text:qsTr("Clé stratégique")} ColumnLayout {Layout.fillWidth:true;spacing:2
                TComboBox {id:themeFilter;objectName:"themeFilter";Layout.fillWidth:true;model:[qsTr("Aucune")].concat(win.themeNames)}
                Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;opacity:.7;visible:advancedDialog.themesPending>0
                    font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)
                    text:qsTr("%1 partie(s) importée(s) avant cette version ne sont pas encore classées : Outils → Réindexer.").arg(advancedDialog.themesPending)}}
            Label {text:qsTr("Médaille")} TComboBox {id:medalFilter;objectName:"medalFilter";Layout.fillWidth:true;model:[qsTr("Aucune")].concat(win.medalNames)}
            Label {text:qsTr("Symbole d’annotation")} TComboBox {id:nagFilter;objectName:"nagFilter";Layout.fillWidth:true;model:[qsTr("Aucun"),"!!","!","!?","?!","?","??"]}
            TCheckBox {text:qsTr("Parties commentées seulement");font.pixelSize:Math.round(12*win.textScale);checked:advancedDialog.draft.annotated||false;Layout.columnSpan:2;onToggled:advancedDialog.draft.annotated=checked}
            Label {text:qsTr("Elo minimal des deux joueurs");wrapMode:Text.WordWrap;Layout.fillWidth:true} TSpinBox {id:minElo;from:0;to:3500;stepSize:100;editable:true}
            Label {text:qsTr("Elo maximal (0 = sans limite)");wrapMode:Text.WordWrap;Layout.fillWidth:true} TSpinBox {id:maxElo;from:0;to:3500;stepSize:100;editable:true}
            Label {text:qsTr("Demi-coups maximum (0 = tous)");wrapMode:Text.WordWrap;Layout.fillWidth:true} TSpinBox {id:maxPlies;from:0;to:2000;editable:true}
            TCheckBox {text:qsTr("Correspondance exacte des champs texte");font.pixelSize:Math.round(12*win.textScale);checked:advancedDialog.draft.exactNames||false;Layout.columnSpan:2;onToggled:advancedDialog.draft.exactNames=checked}
            Label {text:qsTr("Trier par")} TComboBox {id:sortBy;model:["Modification","Date","Joueurs","Elo","Longueur"]}
            Label {text:qsTr("Le nom d’ouverture sélectionne les codes ECO du catalogue Lichess, pas une variante exacte. Pour celle-ci, utilisez la position de l’échiquier. Les filtres Elo excluent les classements inconnus. Le matériel est recherché dans toutes les variantes.");wrapMode:Text.WordWrap;Layout.fillWidth:true;Layout.columnSpan:2;font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);color:win.muted}
            TButton {text:qsTr("Effacer les filtres");onClicked:{win.advanced={};win.applyAdvanced();advancedDialog.close()}}
        }}
        onAccepted:{draft.minElo=minElo.value;draft.maxElo=maxElo.value;draft.maxPlies=maxPlies.value;draft.colour=["","white","black"][colourFilter.currentIndex];draft.nag=[0,3,1,5,6,2,4][nagFilter.currentIndex];draft.medal=medalFilter.currentIndex>0?1<<(medalFilter.currentIndex-1):0;draft.endgame=endgameFilter.keys[endgameFilter.currentIndex];draft.theme=themeFilter.currentIndex>0?win.themeKeys[themeFilter.currentIndex-1]:"";if(sortBy.currentIndex>=0&&["updated","date","players","elo","length"][sortBy.currentIndex]!==(draft.sort||"updated")){draft.sort=["updated","date","players","elo","length"][sortBy.currentIndex];draft.reverse=false}win.advanced=Object.assign({},draft);win.applyAdvanced()}
    }
    TDialog {id:commands;objectName:"commandsDialog";title:qsTr("Commandes rapides");anchors.centerIn:parent;width:Math.min(680,win.width-32);height:Math.min(620,win.height-32);modal:true;standardButtons:Dialog.Close
        readonly property var entries:[
            {title:qsTr("Nouvelle étude"),run:function(){controller.newGame()}},
            {title:qsTr("Enregistrer"),run:function(){controller.saveGame()}},
            {title:qsTr("Importer un PGN"),run:function(){importDialog.open()}},
            {title:qsTr("Accueil"),run:function(){win.page=0}},
            {title:qsTr("Bibliothèque"),run:function(){win.page=1}},
            {title:qsTr("Analyse"),run:function(){win.page=2}},
            {title:qsTr("Ouvertures"),run:function(){win.page=3}},
            {title:qsTr("Entraînement"),run:function(){win.page=4}},
            {title:qsTr("Outils"),run:function(){win.page=5}},
            {title:qsTr("Joueurs"),run:function(){win.page=6}},
            {title:qsTr("Mes répertoires"),run:function(){repertoiresDialog.open()}},
            {title:qsTr("Préparation par position"),run:function(){preparationDialog.open()}},
            {title:qsTr("Moteurs et profils"),run:function(){settingsDialog.open()}},
            {title:qsTr("Comparer les moteurs"),run:function(){compareDialog.open()}},
            {title:qsTr("Tournoi de moteurs"),run:function(){tournamentDialog.open()}},
            {title:qsTr("Puzzles tactiques"),run:function(){puzzlesDialog.open()}},
            {title:qsTr("Exercice de coordonnées"),run:function(){coordinatesDialog.open()}},
            {title:qsTr("Finales à jouer"),run:function(){endgamesDialog.open()}},
            {title:qsTr("Deviner les coups des Blancs"),run:function(){controller.startGuessing(true)}},
            {title:qsTr("Deviner les coups des Noirs"),run:function(){controller.startGuessing(false)}},
            {title:qsTr("Répondre aux questions de l’étude"),run:function(){controller.startQuestions()}},
            {title:qsTr("File d’analyse"),run:function(){analysisQueueDialog.open()}},
            {title:qsTr("Imports PGN reprenables"),run:function(){importsDialog.open()}},
            {title:qsTr("Comptes Lichess et Chess.com"),run:function(){accountsDialog.open()}},
            {title:qsTr("Téléchargements"),run:function(){downloadsDialog.open()}},
            {title:qsTr("Tournois en direct"),run:function(){liveDialog.open()}},
            {title:qsTr("Tournois de la bibliothèque"),run:function(){tournamentFinder.open()}},
            {title:qsTr("Graphiques de la sélection"),run:function(){libraryCharts.open()}},
            {title:qsTr("Plans de cette structure"),run:function(){plansDialog.open()}},
            {title:qsTr("Positions semblables"),run:function(){similarDialog.open()}},
            {title:qsTr("Apparence et langue"),run:function(){appearanceDialog.open()}},
            {title:qsTr("Raccourcis clavier"),run:function(){shortcutsDialog.open()}},
            {title:qsTr("Recherche avancée"),run:function(){advancedDialog.open()}},
            {title:qsTr("Éditer la position"),run:function(){positionEditor.open()}},
            {title:qsTr("Charger une FEN"),run:function(){fenDialog.open()}},
            {title:qsTr("Coller un PGN"),run:function(){pgnDialog.open()}},
            {title:qsTr("Créer une carte d’entraînement"),run:function(){cardDialog.open()}},
            {title:qsTr("Bilan de l’étude"),run:function(){reviewDialog.open()}},
            {title:qsTr("Jouer une partie"),run:function(){playDialog.open()}},
            {title:qsTr("Ouvrir une base"),run:function(){openDatabaseDialog.open()}},
            {title:qsTr("Sauvegarde complète"),run:function(){backupDialog.open()}},
            {title:qsTr("Restaurer une sauvegarde"),run:function(){backupsDialog.open()}},
            {title:qsTr("Guide de l’utilisateur"),run:function(){guideDialog.openAt(0)}},
            {title:qsTr("Exporter cette étude"),run:function(){win.openExport(false)}},
            {title:qsTr("Exporter le diagramme en PNG"),run:function(){diagramDialog.open()}},
            {title:qsTr("Exporter l’étude en PDF"),run:function(){pdfOptions.open()}},
            {title:qsTr("Mises à jour"),run:function(){updatesDialog.open()}},
            {title:qsTr("Aide & raccourcis"),run:function(){helpDialog.open()}}
        ]
        function choose(index){if(index>=0&&index<commandResults.count){var action=commandResults.model[index].run;close();action()}}
        onOpened:{commandSearch.clear();commandSearch.forceActiveFocus()}
        ColumnLayout {anchors.fill:parent;spacing:10
            TTextField {id:commandSearch;objectName:"commandSearch";Layout.fillWidth:true;placeholderText:qsTr("Rechercher une commande…");selectByMouse:true;onAccepted:commands.choose(0);Keys.onDownPressed:{commandResults.currentIndex=0;commandResults.forceActiveFocus()}}
            ListView {id:commandResults;objectName:"commandResults";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:4;ScrollBar.vertical:ScrollBar {}
                model:commands.entries.filter(function(entry){return entry.title.toLocaleLowerCase().indexOf(commandSearch.text.toLocaleLowerCase())>=0})
                delegate:ItemDelegate {required property var modelData;required property int index;width:commandResults.width;text:modelData.title;highlighted:ListView.isCurrentItem;onClicked:commands.choose(index)}
                Keys.onReturnPressed:commands.choose(currentIndex)
                Keys.onEnterPressed:commands.choose(currentIndex)
                Label {anchors.centerIn:parent;text:qsTr("Aucune commande trouvée.");visible:commandResults.count===0}
            }
        }
    }

    Shortcut {sequence:win.keyFor("newGame");onActivated:controller.newGame()}
    Shortcut {sequence:win.keyFor("open");onActivated:importDialog.open()}
    Shortcut {sequence:win.keyFor("save");onActivated:controller.saveGame()}
    Shortcut {sequence:win.keyFor("commands");onActivated:commands.open()}
    // Ce qu'un camp a pris, puis son avance en points s'il en a une.
    function takenLine(white){var lead=(controller.state.materialLead||0)*(white?1:-1);return ((white?controller.state.takenByWhite:controller.state.takenByBlack)||"")+(lead>0?" +"+lead:"")}
    // Les raccourcis viennent des réglages ; Raccourcis clavier… permet de les changer.
    function keyFor(action){return (controller.preferences.shortcuts||{})[action]||""}
    function navigationKeys(){var f=win.activeFocusItem;return win.page===2&&!(f && (f.cursorPosition!==undefined || f.editable===true))&&!downloadsDialog.visible&&!appearanceDialog.visible&&!shortcutsDialog.visible&&!positionEditor.visible&&!settingsDialog.visible&&!fenDialog.visible&&!tagsDialog.visible&&!cardDialog.visible&&!commands.visible&&!playDialog.visible&&!analysisQueueDialog.visible&&!reviewDialog.visible&&!compareDialog.visible&&!updatesDialog.visible&&!preparationDialog.visible&&!repertoiresDialog.visible&&!importsDialog.visible&&!advancedDialog.visible&&!helpDialog.visible&&!pgnDialog.visible&&!collectionDialog.visible&&!promotionDialog.visible&&!chess960Dialog.visible&&!deleteCardDialog.visible&&!deleteVariationDialog.visible&&!analysisMenu.visible&&!positionMenu.visible}
    // Lecture automatique : un coup toutes les secondes et demie, jusqu'au bout de la
    // variante ; quitter l'analyse ou reprendre la main l'arrête.
    Timer {
        id:autoplay;objectName:"autoplayTimer";interval:1500;repeat:true
        onTriggered:{if(win.page===2&&controller.state.canForward&&!controller.state.training)controller.step(1);else running=false}
    }
    // Espace actionne déjà le bouton ou la liste qui a le focus : on ne le lui prend pas.
    // Passé le 20e coup, les positions ne sont plus indexées une à une : l'arbre se tait,
    // et il vaut mieux le dire que laisser croire que personne n'a joué la suite.
    function pastOpeningIndex(){var f=(controller.state.fen||"").split(" ");return f.length>5&&(parseInt(f[5])-1)*2+(f[1]==="b"?1:0)>=40}
    function spaceIsFree(){var f=win.activeFocusItem;return win.navigationKeys()&&!(f&&f.down!==undefined)}
    Shortcut {sequence:win.keyFor("boardOnly");onActivated:{win.boardOnly=!win.boardOnly;win.page=2}}
    Shortcut {sequence:win.keyFor("fullScreen");onActivated:win.toggleFullScreen()}
    Shortcut {sequence:win.keyFor("typeMove");onActivated:{win.page=2;moveEntry.forceActiveFocus()}}
    Shortcut {sequence:win.keyFor("threat");enabled:win.navigationKeys();onActivated:controller.toggleThreat()}
    Shortcut {sequence:win.keyFor("autoplay");enabled:win.spaceIsFree();onActivated:autoplay.running=!autoplay.running}
    // Haut et bas servent d'abord à la liste ou au menu déroulant qui a le focus.
    function verticalKeysFree(){var f=win.activeFocusItem;return win.navigationKeys()&&!(f&&(f.currentIndex!==undefined||f.down!==undefined))}
    Shortcut {sequence:win.keyFor("previousVariation");enabled:win.verticalKeysFree();onActivated:controller.stepVariation(-1)}
    Shortcut {sequence:win.keyFor("nextVariation");enabled:win.verticalKeysFree();onActivated:controller.stepVariation(1)}
    Shortcut {sequence:win.keyFor("back");enabled:win.navigationKeys();onActivated:controller.step(-1)}
    Shortcut {sequence:win.keyFor("forward");enabled:win.navigationKeys();onActivated:controller.step(1)}
    // Les flèches parcourent les coups, Alt + flèche les parties de la liste.
    Shortcut {sequence:win.keyFor("previousGame");enabled:win.navigationKeys();onActivated:controller.stepGame(-1)}
    Shortcut {sequence:win.keyFor("nextGame");enabled:win.navigationKeys();onActivated:controller.stepGame(1)}
    Shortcut {sequence:win.keyFor("start");enabled:win.navigationKeys();onActivated:controller.step(-2)}
    Shortcut {sequence:win.keyFor("end");enabled:win.navigationKeys();onActivated:controller.step(2)}
    Shortcut {sequence:win.keyFor("flip");enabled:win.navigationKeys();onActivated:controller.flip()}
    Shortcut {sequence:win.keyFor("undo");enabled:win.navigationKeys()&&controller.state.canUndo;onActivated:controller.undo()}
    Shortcut {sequence:win.keyFor("deleteMove");enabled:win.navigationKeys()&&controller.state.node>0&&!controller.state.training;onActivated:controller.deleteVariation()}
    Shortcut {sequence:win.keyFor("redo");enabled:win.navigationKeys()&&controller.state.canRedo;onActivated:controller.redo()}
    // Pendant le glisser d'un panneau : son fantôme sous le pointeur, et les bords de
    // l'analyse où il peut ouvrir une nouvelle colonne.
    Item {
        id:dockOverlay;anchors.fill:parent;z:1000;visible:win.dockDragging
        Repeater {
            model:[{zone:"left",where:"start",label:qsTr("Colonne de gauche")},{zone:"right",where:"end",label:qsTr("Colonne de droite")},{zone:"bottom",where:"end",label:qsTr("Sous l’échiquier")}]
            DropArea {
                required property var modelData
                readonly property real edge:Math.round(44*win.uiScale)
                x:modelData.zone==="right"?win.dockArea.x+win.dockArea.width-edge:win.dockArea.x
                y:modelData.zone==="bottom"?win.dockArea.y+win.dockArea.height-edge:win.dockArea.y
                width:modelData.zone==="bottom"?win.dockArea.width:edge
                height:modelData.zone==="bottom"?edge:win.dockArea.height-(modelData.zone==="bottom"?0:edge)
                keys:["t64panel"]
                onDropped:win.dropPanel(modelData.zone,-1,modelData.where)
                Rectangle {anchors.fill:parent;radius:4;color:Qt.rgba(win.accent.r,win.accent.g,win.accent.b,parent.containsDrag?.28:.08)
                           border.color:win.accent;border.width:parent.containsDrag?2:1}
                Label {anchors.centerIn:parent;text:parent.modelData.label;visible:parent.containsDrag;color:win.fg;font.weight:Font.DemiBold
                       rotation:parent.modelData.zone==="bottom"?0:-90}
            }
        }
        Rectangle {
            id:dockGhost
            readonly property point at:win.dockHandler?dockOverlay.mapFromItem(null,win.dockHandler.centroid.scenePosition.x,win.dockHandler.centroid.scenePosition.y):Qt.point(0,0)
            x:at.x-width/2;y:at.y-height/2
            width:ghostLabel.implicitWidth+28;height:Math.round(30*win.uiScale);radius:6
            color:win.raised;border.color:win.accent;border.width:2;opacity:.94
            Drag.active:win.dockDragging;Drag.keys:["t64panel"];Drag.hotSpot.x:width/2;Drag.hotSpot.y:height/2
            Label {id:ghostLabel;anchors.centerIn:parent;text:win.panelTitle(win.dockDragged);font.weight:Font.DemiBold;color:win.fg}
        }
    }
}
