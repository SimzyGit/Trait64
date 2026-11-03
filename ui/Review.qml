import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
TDialog {
    id: root
    property var backend
    function explain(index) {explanation.rowIndex=index;choice.currentIndex=0;explanation.open()}
    title:qsTr("Bilan de la partie")
    width:Math.min(860,parent?parent.width-24:860)
    height:Math.min(700,parent?parent.height-24:700)
    anchors.centerIn:parent;modal:true;standardButtons:Dialog.Close
    function average(white) {var sum=0,n=0;for(var i=0;i<backend.review.length;i++){var row=backend.review[i];if(row.white===white&&row.lossKnown){sum+=row.loss;n++}}return n?Math.round(sum/n):"—"}
    Connections {target:root.backend;function onReviewChanged(){graph.requestPaint()}}
    ScrollView {id:reviewViewport;anchors.fill:parent;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
ColumnLayout {width:reviewViewport.availableWidth;height:Math.max(reviewViewport.availableHeight,implicitHeight);
        spacing:12
        RowLayout {Layout.fillWidth:true
            Label {text:qsTr("Temps par position")}
            TComboBox {id:budget;model:["0,2 s","0,5 s","1 s","2 s","5 s"];currentIndex:1;enabled:!root.backend.reviewState.running}
            TButton {text:root.backend.reviewState.running?qsTr("Interrompre"):qsTr("Analyser la partie");accent:true;onClicked:root.backend.reviewState.running?root.backend.cancelReview():root.backend.analyseGame([200,500,1000,2000,5000][budget.currentIndex])}
            Item {Layout.fillWidth:true}
            Label {text:root.backend.reviewState.done+" / "+root.backend.reviewState.total}
        }
        ProgressBar {Layout.fillWidth:true;from:0;to:Math.max(1,root.backend.reviewState.total);value:root.backend.reviewState.done}
        Label {text:root.backend.reviewState.error||qsTr("Évaluations du point de vue des Blancs. Cliquez le graphe ou un coup pour revoir la position.");wrapMode:Text.WordWrap;Layout.fillWidth:true;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
        Rectangle {Layout.fillWidth:true;Layout.preferredHeight:155;color:root.backend.preferences.panel;radius:8
            Canvas {id:graph;anchors.fill:parent;anchors.margins:10
                onWidthChanged:requestPaint();onHeightChanged:requestPaint()
                onPaint:{
                    var ctx=getContext("2d");ctx.clearRect(0,0,width,height);ctx.strokeStyle=root.backend.preferences.line;ctx.lineWidth=1
                    ctx.beginPath();ctx.moveTo(0,height/2);ctx.lineTo(width,height/2);ctx.stroke()
                    var rows=root.backend.review;if(!rows.length)return
                    ctx.strokeStyle=root.backend.preferences.accentColor;ctx.lineWidth=2;ctx.beginPath()
                    for(var i=0;i<rows.length;i++){var x=rows.length===1?width/2:i*width/(rows.length-1),y=height/2-Math.max(-5,Math.min(5,rows[i].score))*height/10;if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y)}ctx.stroke()
                    for(var j=0;j<rows.length;j++)if(rows[j].loss>=100){ctx.fillStyle=rows[j].loss>=250?"#dc6659":"#d7a355";var px=rows.length===1?width/2:j*width/(rows.length-1),py=height/2-Math.max(-5,Math.min(5,rows[j].score))*height/10;ctx.beginPath();ctx.arc(px,py,4,0,Math.PI*2);ctx.fill()}
                }
                MouseArea {anchors.fill:parent;onClicked:function(mouse){var rows=root.backend.review;if(rows.length){var index=Math.round(Math.max(0,Math.min(1,mouse.x/width))*(rows.length-1));root.backend.navigate(rows[index].node)}}}
            }
        }
        Label {text:qsTr("Perte moyenne (centipions) — Blancs : ")+root.average(true)+qsTr(" · Noirs : ")+root.average(false);Layout.fillWidth:true;wrapMode:Text.WordWrap;font.bold:true}
        ListView {id:rows;Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:170;model:root.backend.review;clip:true;spacing:3;ScrollBar.vertical:ScrollBar{}
            currentIndex:-1
            delegate:ItemDelegate {required property var modelData;required property int index;width:rows.width;implicitHeight:36*controller.preferences.fontScale*controller.preferences.uiScale;highlighted:ListView.isCurrentItem
                text:(index+1)+".  "+modelData.move+"    "+modelData.eval+"    "+modelData.label+(modelData.lossKnown?"    (−"+modelData.loss+" cp)":"");onClicked:{rows.currentIndex=index;root.backend.navigate(modelData.node)}
            }
        }
        Flow {Layout.fillWidth:true;spacing:8
TButton {objectName:"explainReviewButton";text:qsTr("Comprendre ce coup");enabled:rows.currentIndex>=0&&rows.currentIndex<root.backend.review.length;onClicked:root.explain(rows.currentIndex)}
            TButton {text:qsTr("Annoter le PGN");enabled:!root.backend.reviewState.running&&root.backend.review.length>0;onClicked:root.backend.annotateReview()}
            TButton {text:qsTr("Créer des exercices des erreurs");enabled:!root.backend.reviewState.running&&root.backend.review.length>0;onClicked:root.backend.createMistakeCards()}
            TButton {objectName:"mistakePuzzles";text:qsTr("Créer des puzzles des erreurs");enabled:!root.backend.reviewState.running&&root.backend.review.length>0;onClicked:root.backend.createMistakePuzzles()}
        }
        Label {text:qsTr("Seuils indicatifs : 50 / 100 / 250 centipions. Les résultats dépendent du moteur et du temps accordé ; les exercices suivent sa variante principale.");wrapMode:Text.WordWrap;Layout.fillWidth:true;opacity:.7;font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale)}
    }
}
    TDialog {
        id:explanation;objectName:"reviewExplanationDialog";parent:Overlay.overlay
        property int rowIndex:-1
        property var details:visible&&root.backend.review.length>rowIndex&&rowIndex>=0?root.backend.explainReview(rowIndex,choice.currentIndex===1):({available:false,error:qsTr("Sélectionnez un coup analysé.")})
        title:qsTr("Comprendre le coup");modal:true;standardButtons:Dialog.Close;anchors.centerIn:parent
        width:Math.min(760,parent?parent.width-24:760);height:Math.min(660,parent?parent.height-24:660)
        ScrollView {id:explanationViewport;anchors.fill:parent;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
        ColumnLayout {
            width:explanationViewport.availableWidth;spacing:10
            Label {Layout.fillWidth:true;text:explanation.details.available?explanation.details.move+" · "+explanation.details.label+" · "+explanation.details.beforeEval+" → "+explanation.details.afterEval+qsTr(" (Blancs)"):explanation.details.error;wrapMode:Text.WordWrap}
            TComboBox {id:choice;objectName:"reviewLineChoice";Layout.fillWidth:true;model:["Coup joué et réponse moteur", "Proposition moteur avant le coup"]}
            Label {Layout.fillWidth:true;visible:explanation.details.available;text:explanation.details.available?explanation.details.engine+" · "+explanation.details.milliseconds+qsTr(" ms par position · profondeur ")+explanation.details.depth:"";wrapMode:Text.WordWrap;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
            Label {Layout.fillWidth:true;visible:explanation.details.available;text:explanation.details.available?qsTr("Matériel (Blancs − Noirs) : ")+explanation.details.initialMaterial+" → "+explanation.details.finalMaterial+qsTr(". Pion = 1, cavalier/fou = 3, tour = 5, dame = 9. Ce compte ne mesure pas la qualité de la position."):"";wrapMode:Text.WordWrap;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
            ListView {
                id:factList;objectName:"reviewFacts";Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:170;clip:true;spacing:8
                model:explanation.details.steps||[];ScrollBar.vertical:ScrollBar{}
                delegate:Column {
                    required property var modelData;required property int index;width:factList.width;spacing:3
                    Label {width:parent.width;text:(index+1)+" · "+modelData.side+" · "+modelData.san;font.bold:true;wrapMode:Text.WordWrap}
                    Label {width:parent.width;text:modelData.facts;wrapMode:Text.WordWrap}
                }
            }
            Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);text:qsTr("Motifs observés, sans garantie de gain forcé. Dans la copie : flèches vertes = attaques, marques jaunes = alignements de clouage/enfilade. La suite du moteur peut évoluer ; les répétitions antérieures au diagramme ne sont pas reproduites.")+(explanation.details.truncated?qsTr(" Suite limitée aux 128 premiers demi-coups."):"")}
            TButton {objectName:"openReviewLineButton";text:qsTr("Ouvrir la suite dans une nouvelle étude");enabled:explanation.details.available&&!root.backend.reviewState.running;onClicked:{if(root.backend.openReviewLine(explanation.rowIndex,choice.currentIndex===1)){explanation.close();root.close()}}}
        }
        }
    }
}
