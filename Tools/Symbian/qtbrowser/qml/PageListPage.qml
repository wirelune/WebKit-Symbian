import QtQuick 1.1
import com.nokia.symbian 1.1

// The bookmarks or the history. A tap opens the page; holding a bookmark
// removes it.
Page {
    id: listPage
    property string headingText
    property bool bookmarks: true
    property variant browser

    ListHeading {
        id: heading
        anchors { top: parent.top; left: parent.left; right: parent.right }
        ListItemText {
            anchors.fill: parent.paddingItem
            role: "Heading"
            text: listPage.headingText
        }
    }

    ListView {
        id: list
        anchors { top: heading.bottom; left: parent.left; right: parent.right; bottom: parent.bottom }
        clip: true
        model: listPage.bookmarks ? pageStore.bookmarks : pageStore.history
        delegate: ListItem {
            Column {
                anchors.fill: parent.paddingItem
                ListItemText { width: parent.width; role: "Title"; text: modelData.title }
                ListItemText { width: parent.width; role: "SubTitle"; text: modelData.url }
            }
            onClicked: {
                listPage.browser.go(modelData.url)
                pageStack.pop()
            }
            onPressAndHold: {
                if (listPage.bookmarks)
                    pageStore.removeBookmark(modelData.url)
            }
        }
    }

    Label {
        anchors.centerIn: parent
        visible: list.count === 0
        text: listPage.bookmarks ? "Закладок нет" : "История пуста"
    }

    tools: ToolBarLayout {
        ToolButton {
            iconSource: "toolbar-back"
            onClicked: pageStack.pop()
        }
        ToolButton {
            iconSource: "toolbar-delete"
            visible: !listPage.bookmarks
            onClicked: pageStore.clearHistory()
        }
    }
}
