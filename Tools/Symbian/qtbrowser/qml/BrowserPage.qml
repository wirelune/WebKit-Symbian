import QtQuick 1.1
import com.nokia.symbian 1.1
import WebKitSymbian 1.0

Page {
    id: root

    function go(text) {
        var address = text.replace(/^\s+|\s+$/g, "")
        if (address === "")
            return
        // Words without a dot or a scheme are a search.
        if (address.indexOf(" ") >= 0 || (address.indexOf(".") < 0 && address.indexOf(":") < 0))
            address = "https://duckduckgo.com/lite/?q=" + encodeURIComponent(address)
        page.load(address)
        page.forceActiveFocus()
    }

    function errorText(code) {
        if (code === -1)
            return "Движок не найден. Установите symbian-webkit-icu.sis и browser.sis."
        if (code === -5)
            return "Движок WebKit работает только на телефоне."
        if (code === -4)
            return "Не хватает памяти для движка."
        return "Ошибка " + code
    }

    // The address field, with the load progress under it.
    Item {
        id: header
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: urlField.height + 2 * platformStyle.paddingSmall

        TextField {
            id: urlField
            anchors {
                left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter
                leftMargin: platformStyle.paddingSmall; rightMargin: platformStyle.paddingSmall
            }
            placeholderText: "Адрес или поиск"
            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            Keys.onReturnPressed: root.go(text)
            Keys.onEnterPressed: root.go(text)
            onActiveFocusChanged: {
                if (activeFocus)
                    selectAll()
                else
                    text = page.url
            }
        }

        ProgressBar {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 4
            visible: page.loading
            minimumValue: 0
            maximumValue: 100
            value: page.progress
        }
    }

    WebPage {
        id: page
        anchors { top: header.bottom; left: parent.left; right: parent.right; bottom: parent.bottom }
        focus: true
        initialUrl: startUrl !== "" ? startUrl : pageStore.homePage()

        onUrlChanged: {
            if (!urlField.activeFocus)
                urlField.text = url
            pageStore.visited(url, "")
        }
        onTitleChanged: pageStore.visited(url, title)
        onPressed: {
            if (urlField.activeFocus)
                page.forceActiveFocus()
        }
        onAlertRequested: {
            alertDialog.message = message
            alertDialog.open()
        }
        onConfirmRequested: {
            confirmDialog.message = message
            confirmDialog.answered = false
            confirmDialog.open()
        }
        onSelectRequested: {
            selectModel.clear()
            for (var i = 0; i < items.length; ++i)
                selectModel.append({ "name": items[i] })
            selectDialog.answered = false
            selectDialog.selectedIndex = selected
            selectDialog.open()
        }
        onNetworkFailed: banner.show("Нет подключения к сети (" + error + ")")
    }

    // Pages from the bookmarks and the history that match the address
    // being typed.
    ListView {
        id: suggestions
        anchors { top: header.bottom; left: parent.left; right: parent.right }
        height: Math.min(count, 5) * 64
        visible: urlField.activeFocus && count > 0
        clip: true
        interactive: false
        model: urlField.activeFocus ? pageStore.suggestions(urlField.text, 5) : []
        delegate: ListItem {
            height: 64
            Column {
                anchors.fill: parent.paddingItem
                ListItemText { width: parent.width; role: "Title"; text: modelData.title }
                ListItemText { width: parent.width; role: "SubTitle"; text: modelData.url }
            }
            onClicked: root.go(modelData.url)
        }
        Rectangle { anchors.fill: parent; z: -1; color: platformStyle.colorBackground }
    }

    // While the engine starts, or if it can't.
    Rectangle {
        anchors.fill: page
        color: "white"
        visible: page.status !== "ready"

        Column {
            anchors.centerIn: parent
            width: parent.width - 2 * platformStyle.paddingLarge
            spacing: platformStyle.paddingLarge

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: page.status === "starting"
                visible: running
            }
            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: "black"
                text: page.status === "starting" ? "Запуск WebKit…"
                    : page.status === "died" ? "Движок WebKit завершился (" + page.error + ")"
                    : errorText(page.error)
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Перезапустить"
                visible: page.status === "died"
                onClicked: page.restart()
            }
        }
    }

    InfoBanner { id: banner; function show(message) { text = message; open() } }

    QueryDialog {
        id: alertDialog
        titleText: "Сообщение страницы"
        acceptButtonText: "OK"
        onAccepted: page.answerPopup(0)
        onRejected: page.answerPopup(0)
    }

    QueryDialog {
        id: confirmDialog
        property bool answered: false
        titleText: "Подтверждение"
        acceptButtonText: "OK"
        rejectButtonText: "Отмена"
        onAccepted: { if (!answered) { answered = true; page.answerPopup(1) } }
        onRejected: { if (!answered) { answered = true; page.answerPopup(0) } }
    }

    ListModel { id: selectModel }

    SelectionDialog {
        id: selectDialog
        property bool answered: false
        titleText: "Выбор"
        model: selectModel
        onAccepted: { if (!answered) { answered = true; page.answerPopup(selectedIndex) } }
        onRejected: { if (!answered) { answered = true; page.answerPopup(-1) } }
    }

    Menu {
        id: menu
        content: MenuLayout {
            MenuItem {
                text: pageStore.bookmarks.length >= 0 && pageStore.isBookmarked(page.url) ? "Удалить из закладок" : "Добавить в закладки"
                onClicked: {
                    if (pageStore.isBookmarked(page.url))
                        pageStore.removeBookmark(page.url)
                    else
                        pageStore.addBookmark(page.url, page.title)
                }
            }
            MenuItem {
                text: "Закладки"
                onClicked: pageStack.push(Qt.resolvedUrl("PageListPage.qml"),
                    { headingText: "Закладки", bookmarks: true, browser: root })
            }
            MenuItem {
                text: "История"
                onClicked: pageStack.push(Qt.resolvedUrl("PageListPage.qml"),
                    { headingText: "История", bookmarks: false, browser: root })
            }
            MenuItem {
                text: "Домашняя страница"
                onClicked: root.go(pageStore.homePage())
            }
            MenuItem {
                text: "Выход"
                onClicked: Qt.quit()
            }
        }
    }

    tools: ToolBarLayout {
        ToolButton {
            iconSource: "toolbar-back"
            onClicked: page.canGoBack ? page.goBack() : Qt.quit()
        }
        ToolButton {
            iconSource: "toolbar-next"
            enabled: page.canGoForward
            onClicked: page.goForward()
        }
        ToolButton {
            iconSource: page.loading ? "toolbar-mediacontrol-stop" : "toolbar-refresh"
            onClicked: page.loading ? page.stop() : page.reload()
        }
        ToolButton {
            iconSource: "toolbar-menu"
            onClicked: menu.open()
        }
    }
}
