import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
TDialog {
    id: root
    property var backend
    property var info: backend.state
    title: qsTr("Versions et mises à jour")
    width: Math.min(740,parent ? parent.width-32 : 740)
    height: Math.min(680,parent ? parent.height-32 : 680)
    anchors.centerIn:parent;modal:true
    closePolicy: info.status==="installing" ? Popup.NoAutoClose : Popup.CloseOnEscape
    ScrollView {id:updatesScroll;anchors.fill:parent;contentWidth:availableWidth;clip:true
        ColumnLayout {width:updatesScroll.availableWidth;spacing:14
            Label {text:qsTr("Version installée : ")+root.info.current;font.bold:true;font.pixelSize:Math.round(18*controller.preferences.fontScale*controller.preferences.uiScale)}
            Flow {Layout.fillWidth:true;spacing:8
                Label {text:qsTr("Canal")}
                TComboBox {model:["Stable","Bêta","Développement"];currentIndex:["stable","beta","development"].indexOf(root.info.channel);enabled:!root.info.busy;onActivated:root.backend.setChannel(["stable","beta","development"][currentIndex])}
                TButton {text:qsTr("Vérifier maintenant");enabled:!root.info.busy;onClicked:root.backend.check()}
            }
            CheckBox {text:qsTr("Vérifier au lancement");checked:root.info.automatic;onToggled:root.backend.setAutomatic(checked)}
            Label {text:qsTr("Stable reçoit uniquement les versions stables publiées. Bêta inclut les préversions bêta ; Développement inclut les versions de travail testées.");wrapMode:Text.WordWrap;Layout.fillWidth:true}
            GroupBox {title:qsTr("Accès au dépôt privé");Layout.fillWidth:true
                ColumnLayout {anchors.fill:parent
                    CheckBox {text:qsTr("Utiliser ma connexion GitHub CLI (gh)");checked:root.info.githubCli;enabled:!root.info.busy;onToggled:root.backend.useGithubCli(checked)}
                    Label {text:qsTr("Connectez gh avec « gh auth login », puis activez cette option. Autre possibilité : un jeton limité à Trait64, permission Contents en lecture, conservé uniquement pour cette session.");wrapMode:Text.WordWrap;Layout.fillWidth:true;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                    RowLayout {Layout.fillWidth:true
                        TextField {id:token;Layout.fillWidth:true;echoMode:TextInput.Password;placeholderText:root.info.authenticated?qsTr("Accès configuré"):qsTr("Jeton de session");enabled:!root.info.busy}
                        TButton {text:qsTr("Utiliser");enabled:!root.info.busy && token.text.length>0;onClicked:{root.backend.setSessionToken(token.text);token.clear();root.backend.check()}}
                    }
                }
            }
            Label {text:root.info.message||qsTr("Les fonctions d’échecs restent utilisables hors ligne.");wrapMode:Text.WordWrap;Layout.fillWidth:true}
            Label {visible:root.info.available;text:qsTr("Nouvelle version : ")+root.info.version;font.bold:true;font.pixelSize:Math.round(20*controller.preferences.fontScale*controller.preferences.uiScale)}
            TextArea {visible:root.info.available;Layout.fillWidth:true;Layout.preferredHeight:160;readOnly:true;wrapMode:TextEdit.Wrap;text:root.info.notes;textFormat:TextEdit.PlainText;selectByMouse:true}
            ProgressBar {visible:root.info.status==="downloading";Layout.fillWidth:true;value:root.info.progress}
            Label {visible:root.info.available;text:qsTr("Installation : ")+root.info.kind+qsTr(" · sauvegarde automatique des données avant remplacement.");Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
            Flow {Layout.fillWidth:true;spacing:8
                TButton {text:qsTr("Mettre à jour");accent:true;visible:root.info.available&&root.info.status!=="installed";enabled:!root.info.busy;onClicked:root.backend.downloadAndInstall()}
                TButton {text:qsTr("Plus tard");visible:root.info.available&&root.info.status!=="installed";enabled:!root.info.busy;onClicked:{root.backend.later();root.close()}}
                TButton {text:qsTr("Ignorer");visible:root.info.available&&root.info.status!=="installed";enabled:!root.info.busy;onClicked:{root.backend.ignore();root.close()}}
                TButton {text:qsTr("Annuler");visible:root.info.busy&&root.info.status!=="installing";onClicked:root.backend.cancel()}
                TButton {text:qsTr("Redémarrer");visible:root.info.status==="installed";accent:true;onClicked:root.backend.restart()}
                Item {Layout.fillWidth:true}
                TButton {text:qsTr("Fermer");enabled:root.info.status!=="installing";onClicked:root.close()}
            }
        }
    }
    Connections {target:root.backend;function onOffered(){root.open()}}
}
