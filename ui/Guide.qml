// SPDX-License-Identifier: AGPL-3.0-or-later
// Guide de l'utilisateur : les sections à gauche, le texte à droite. Le texte vient de
// assets/guide/<langue>.md, découpé à chaque titre de niveau 2.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
TDialog {
    id:root
    property var backend
    property int current:0
    readonly property var sections:{
        const text=backend?backend.userGuide():""
        const result=[]
        for(const part of text.split(/^## /m)){
            const cut=part.indexOf("\n")
            if(cut>0)result.push({title:part.slice(0,cut).trim(),body:part.slice(cut+1).trim()})
        }
        return result
    }
    title:qsTr("Guide de l’utilisateur")
    anchors.centerIn:parent;modal:true;standardButtons:Dialog.Close
    width:Math.min(980,parent.width-32);height:Math.min(760,parent.height-32)
    function openAt(index){current=Math.max(0,Math.min(index,sections.length-1));open()}
    RowLayout {
        anchors.fill:parent;spacing:12
        ListView {
            id:index;objectName:"guideSections";Layout.preferredWidth:Math.min(240,root.width*.3);Layout.fillHeight:true
            clip:true;model:root.sections;currentIndex:root.current
            delegate:ItemDelegate {
                required property var modelData;required property int index
                width:ListView.view.width;text:modelData.title;highlighted:index===root.current
                onClicked:root.current=index
            }
        }
        ScrollView {
            id:page;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:availableWidth
            ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
            ColumnLayout {
                width:page.availableWidth;spacing:12
                Label {text:root.sections.length?root.sections[root.current].title:"";font.pixelSize:20;font.bold:true;Layout.fillWidth:true;wrapMode:Text.WordWrap}
                Text {
                    objectName:"guideText";Layout.fillWidth:true;wrapMode:Text.WordWrap;textFormat:Text.MarkdownText
                    color:root.palette.windowText;font.pixelSize:15;lineHeight:1.15
                    text:root.sections.length?root.sections[root.current].body:""
                }
            }
        }
    }
    onCurrentChanged:page.ScrollBar.vertical.position=0
}
