# Symbian-порт на Windows: тестовый стенд

Дата: 2026-11-26. Ветка: `2.32.4-symbian` (форк wirelune/WebKit-Symbian).

## Цель

Собрать движок браузера под Windows так, чтобы он вёл себя как на Nokia N8:
тот же код WebKit, те же настройки, та же сеть. На нём отлаживать сайты с
Web Inspector, а исправления без переделок переносить на телефон.

Конечная цель — рабочий вход на ozon.ru: сначала на стенде, потом на телефоне.

Рамки: на стенде ищем и чиним настоящие ошибки и пробелы движка. Подделку
отпечатка (WebGL, шрифты, признаки браузера) и расшифровку отчёта антибота
не делаем.

## Что совпадает с телефоном и что нет

| | Телефон | Стенд |
| --- | --- | --- |
| Исходники WebCore, JSC, WTF | ветка порта | те же |
| Настройки сборки | `OptionsSymbian.cmake` | он же; меняется только то, без чего нельзя на Windows |
| Слой встраивания | `Source/WebKitLegacy/symbian` | тот же, без правок под Windows |
| Платформа WebCore | `Source/WebCore/platform/symbian` | та же; Symbian-зависимые места под `OS(SYMBIAN)` |
| Цикл событий | generic RunLoop, `CRunLoopDriver` | generic RunLoop, Win32-таймер |
| Сеть | curl 7.88 + mbedTLS 3.4.1 + nghttp2 1.52, наши патчи TLS 1.3 | те же версии и патчи |
| CA-сертификаты | `C:/resource/webkit/cacert.pem` | тот же файл в каталоге стенда |
| Шрифты | 20 шрифтов `Z:\resource\fonts` | те же файлы, пользователь копирует с телефона |
| Графика | Cairo, без композитинга | то же |
| WebGL 1 | EGL + GLES2, Broadcom | EGL + GLES2 через ANGLE (D3D11); строка рендерера будет другой |
| JS | 32 бита (JSVALUE32_64), ARMv7 JIT + DFG | 32 бита (JSVALUE32_64), только LLInt: в 2.32 на 32-битном x86 под Windows нет JIT |
| WebAssembly | нет | нет |
| Экран | 360x640, вид 360x510 | окно с тем же размером вида |
| UA, языки | `UserAgentSymbian`, язык телефона | то же; язык из настроек стенда |
| Данные | `C:\Data\WebKitQt\` | `%LOCALAPPDATA%\WebKitQt\`, та же раскладка |
| Скорость | N8, ARM11 680 МГц | ПК, в разы быстрее даже без JIT |
| Web Inspector | нет | есть |

Расхождения, которые не убрать: скорость, GPU и JIT. Если результат на стенде
зависит от них, это видно сравнением с телефоном.

## Компоненты

### 1. Зависимости — `Tools/Symbian/host/deps.ps1`

* vcpkg, триплет `x86-windows`: cairo, pixman, freetype, harfbuzz, fontconfig,
  ICU, libxml2, libxslt, sqlite3, libjpeg-turbo, libpng, libwebp, zlib.
  Версии фиксируются baseline-коммитом vcpkg.
* curl, mbedTLS и nghttp2 не берутся из vcpkg. Их собирает тот же код, что для
  телефона: общая часть `build-deps.sh` (версии, URL, патчи) выносится так,
  чтобы её звали обе сборки.
* ANGLE из дерева WebKit собирается целиком (libEGL + libGLESv2 с D3D11),
  а не только транслятор.

### 2. CMake-порт `SymbianHost`

* `Source/cmake/OptionsSymbianHost.cmake` подключает общий список опций из
  `OptionsSymbian.cmake`: тот вынесен в `OptionsSymbianCommon.cmake`, чтобы не
  дублировать ENABLE_*. Отличия стенда:
  * MSVC/clang-cl, x86;
  * ENABLE_JIT OFF (LLInt);
  * системный malloc;
  * USE_ANGLE_EGL ON;
  * сборка ресурсов WebInspectorUI для фронтенда инспектора.
* Код под `OS(SYMBIAN)` на стенде не компилируется. Где он нужен по смыслу
  (пути к данным, CA-файл, шрифты), вводится `PLATFORM(SYMBIAN)`: он
  включён в обеих сборках. `OS(SYMBIAN)` остаётся только для вызовов Symbian API.
  Правило для будущих правок: поведение — под `PLATFORM(SYMBIAN)`, Symbian API —
  под `OS(SYMBIAN)`.
* `initializeFonts` на стенде смотрит только в `<данные>\fonts`, а не в Z:/C:/E:/F:.
  На этом ПК есть диски E: и F:, и чужие шрифты не должны в них найтись.

### 3. Хост — `Tools/Symbian/host/wkhost.exe`

Заменяет `wkengine.exe` и Qt-морду, говорит с WebView через тот же
`WebKitSymbian.h`.

* Окно Win32 с адресной строкой и видом 360x510. Буфер страницы рисуется так же,
  как на телефоне (общий RGBA-буфер, `StretchDIBits`).
* Мышь превращается в касания (touchstart/move/end) с теми же правилами, что в
  Qt-морде. Колесо прокручивает. Клавиатура идёт через `PlatformKeyboardEvent`.
* `log.txt` в том же формате: время, `threads`, `memory`, console.
  `curl.txt` — при файле `curl-verbose`.
* `useragent.txt` и язык (`language.txt`, по умолчанию `en-GB`, как сейчас на N8) читаются так же,
  как в `EngineServer.cpp`. Общий код чтения переносится в слой встраивания.
* Командная строка: `wkhost.exe [URL] [--inspect]`.

### 4. Web Inspector

* Новый `InspectorClient` для Symbian-слоя в `Source/WebKitLegacy/symbian`.
  Он открывает фронтенд через колбэк в `WebKitSymbian.h`. Телефон колбэк
  не задаёт, и инспектор там выключен.
* Хост открывает второе окно с WebView, которое грузит `Main.html` из
  ресурсов WebInspectorUI. Связь — `InspectorFrontendClientLocal`, как у
  WebKitLegacy на Windows.
* Нужны консоль, сеть, исходники с точками останова, ошибки JS.

### 5. CI — `.github/workflows/symbian.yml`

* Новый job `windows-host` на `windows-2022`, параллельно сборке для телефона.
* Кэш vcpkg и каталога сборки по ключу из версий зависимостей.
* Артефакт `wkhost`: zip с `wkhost.exe`, DLL, ICU-данными, `cacert.pem`,
  ресурсами инспектора и `README.txt` о копировании шрифтов.
* Ошибки сборки печатаются в логе шага, как в `build-webcore.sh`.

## Проверка

Пользователь запускает `wkhost.exe` из артефакта. Этапы:

1. Открываются duckduckgo.com/lite и google.com, `log.txt` в формате телефона.
2. Шрифты из `<данные>\fonts`: в логе `fonts: 19`, как на N8.
3. `localStorage` (4chan), cookie после перезапуска, WebGL (get.webgl.org).
4. Инспектор открывается на ozon.ru, видны запросы `/abt/result` и консоль.
5. Ozon: найти, на чём челлендж отличается от настоящего Safari 14, и исправить
   движок. Цель этапа — вход на стенде.
6. Перенос: те же коммиты в сборку для телефона, проверка на N8.

Для этапов 1–3 сравниваю `log.txt` стенда и телефона.

## Дополнение: EKA2L1

В `Tools/Symbian/eka2l1` уже есть запуск `jsc.exe` и `browser.exe` в эмуляторе.
Там работают те же ARM-бинарники, но сеть, EGL и скорость под вопросом.
Отдельной задачей позже: проверить, грузит ли `wkengine.exe` в EKA2L1 страницу.
Если грузит — это второй стенд, ближе к телефону, без инспектора.

## Чего не делаем

* WebKit2, композитинг, 64 бита, JIT на стенде.
* Вкладки, закладки и прочий интерфейс браузера: стенд — инструмент отладки.
* Подделку отпечатка и обход антибота.

## Риски

* Порт впервые собирается MSVC/clang-cl: код порта писался под GCC/clang Symbian
  SDK. Ожидаемы мелкие правки совместимости.
* WebKit 2.32 и VS 2022: возможны ошибки новых компиляторов. Запасной вариант —
  clang-cl из VS.
* vcpkg-версии библиотек новее, чем в SDK телефона (cairo, harfbuzz, ICU).
  Если отличие влияет на результат, собрать их нашими скриптами.
* Время первой сборки в CI: 2–3 часа, потом с кэшем примерно час.
