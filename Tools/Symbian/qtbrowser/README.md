# WebKit Qt — браузер на Qt 4.7.4 для Symbian Anna

Интерфейс сделан на Qt 4.7.4 и Qt Quick Components for Symbian 1.1. Страницы рисует
WebKit 2.32 из этого порта, который работает отдельным процессом `wkengine.exe`.

## Как это устроено

- `../engine/wkengine.exe` — WebView порта, обёрнутый в Symbian-сервер (`CServer2`).
  Его собирает CI тулчейном symbian-platform (clang, C++17), он входит в `browser.sis`.
- `WebKitQt.exe` — эта папка. Собирается обычным Qt SDK (Qt 4.7.4, GCCE),
  запускает движок, когда стартует, и общается с ним по протоколу
  `../engine/EngineProtocol.h`:
  - страница лежит в общем глобальном чанке (32-битные пиксели), процесс Qt рисует
    его через `QImage`, не копируя;
  - касания, клавиши и ввод (preedit/commit от FEP) уходят в движок;
  - `alert`/`confirm`/`<select>` показываются диалогами Components.
- HTTPS делает сам движок (curl + mbedTLS, TLS 1.2), сеть Qt не используется.
- Когда процесс Qt закрывается, движок тоже завершается.

## Установка на телефон

1. `symbian-webkit-icu.sis` (на диск C:).
2. `browser.sis` из CI — в нём AVKON-браузер и `wkengine.exe`.
3. `WebKitQt.sis` — собирается здесь:

```bat
tools\build.bat projects\WebKit-Symbian\Tools\Symbian\qtbrowser
```

На телефоне должны стоять Qt 4.7.4 и Qt Quick Components 1.1.

Данные (куки, кэш, `log.txt`) лежат в `C:\Data\WebKitQt`. Трассировка curl
включается, если создать файл `C:\Data\WebKitQt\curl-verbose`.

В Qt Simulator движка нет: там видна только оболочка и сообщение, что движок
работает только на телефоне.

Видео не воспроизводится: у процесса движка нет окна. Аудио работает.
