# Windows-стенд Symbian-порта: план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** собрать в CI `wkhost.exe` — движок Symbian-порта под Windows (x86, 32 бита) с тем же кодом, настройками и сетью, что на N8, плюс Web Inspector.

**Architecture:** новый CMake-порт `SymbianHost` берёт общие опции из `OptionsSymbianCommon.cmake` и общий слой `Source/WebKitLegacy/symbian`. Поведение порта помечается `PLATFORM(SYMBIAN)` (обе сборки), вызовы Symbian API — `OS(SYMBIAN)` (только телефон). Win32-хост говорит с движком через `WebKitSymbian.h`, как `wkengine.exe`.

**Tech Stack:** CMake + Ninja, MSVC (VS 2022, x86), vcpkg `x86-windows`, curl 7.88.1 + mbedTLS 3.4.1 + nghttp2 1.52.0, ANGLE (D3D11), GitHub Actions `windows-2022`.

**Spec:** `Tools/Symbian/docs/WindowsHost-design.md`

## Global Constraints

- Ветка `2.32.4-symbian`, remote `fork` (github.com/wirelune/WebKit-Symbian). Push прямо в ветку, без PR.
- Каждый коммит кончается строкой `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Сборка для телефона (job `build`) после каждой задачи остаётся зелёной. Это главный тест на регрессии.
- Архитектура стенда: x86, 32 бита (JSVALUE32_64), `ENABLE_JIT OFF`, без WebAssembly.
- Версии сети как на телефоне: curl 7.88.1, mbedTLS 3.4.1 (конфиг из SDK symbian-platform 0.3.0), nghttp2 1.52.0.
- Подделку отпечатка и обход антибота не делаем.
- Шрифты N8 в репозиторий и артефакт не кладём.
- Локальный чекаут — partial clone `blob:none`, sparse. Не запускать `git grep` по всему дереву: он виснет, докачивая блобы. Файл вне sparse — `git show HEAD:path` или добавить путь в `.git/info/sparse-checkout` и выполнить `git read-tree -mu HEAD`.
- Файлы ANGLE и некоторые другие приходят с CRLF: перед Edit выполнить `sed -i 's/\r$//' FILE`.
- Ошибки сборки на CI читать в логе шага (страница job, нужен вход в GitHub во встроенном браузере). Статус запуска: `curl -s "https://api.github.com/repos/wirelune/WebKit-Symbian/actions/runs?head_sha=$(git rev-parse HEAD)&per_page=1"`.

## Карта файлов

| Файл | Что делает |
| --- | --- |
| `.github/workflows/symbian.yml` | + job `windows-host` |
| `Tools/Symbian/host/ci-windows.ps1` | весь CI-сценарий стенда, по шагам (`-Step deps|configure|build|test|package`) |
| `Tools/Symbian/host/list-conditionals.sh` | список мест `OS(SYMBIAN)` / `__SYMBIAN32__` в исходниках |
| `Tools/Symbian/host/vcpkg.json` | зависимости vcpkg с baseline |
| `Tools/Symbian/host/build-net.ps1` | mbedTLS, nghttp2, curl для стенда |
| `Tools/Symbian/scripts/patch-curl.sh` | патчи curl 7.88.1, общие для обеих сборок (вынесены из `build-deps.sh`) |
| `Source/WTF/wtf/PlatformLegacy.h` | `PLATFORM(SYMBIAN)` |
| `Source/cmake/OptionsSymbianCommon.cmake` | опции, общие для телефона и стенда |
| `Source/cmake/OptionsSymbian.cmake` | телефон: подключает Common + своё |
| `Source/cmake/OptionsSymbianHost.cmake` | стенд: подключает Common + своё |
| `Source/cmake/WebKitCommon.cmake` | `SymbianHost` в `ALL_PORTS` |
| `Source/{WTF/wtf,JavaScriptCore,bmalloc,WebCore,WebCore/PAL/pal,WebKitLegacy,ThirdParty/ANGLE}/PlatformSymbianHost.cmake` | исходники стенда для каждого проекта |
| `Tools/Symbian/host/CMakeLists.txt` | цель `wkhost` |
| `Tools/Symbian/host/HostLog.{h,cpp}` | `log.txt` в формате телефона |
| `Tools/Symbian/host/HostSettings.{h,cpp}` | `useragent.txt`, `language.txt` |
| `Tools/Symbian/host/HostWindow.{h,cpp}` | окно, буфер, ввод, `WebViewClient` |
| `Tools/Symbian/host/main.cpp` | разбор командной строки, цикл сообщений |
| `Tools/Symbian/host/tests/*.html`, `*.js` | страницы и скрипты для CI-проверок |
| `Source/WebKitLegacy/symbian/SymbianInspectorClient.{h,cpp}` | Web Inspector для слоя встраивания |
| `Tools/Symbian/host/README.txt` | что лежит в артефакте, куда класть шрифты |

---

### Task 1: CI-скелет и инвентаризация

Задача создаёт job `windows-host`. Он выкачивает исходники, ставит инструменты и печатает места, где порт зависит от `OS(SYMBIAN)`. Этот список нужен в Task 2.

**Files:**
- Create: `Tools/Symbian/host/list-conditionals.sh`
- Create: `Tools/Symbian/host/ci-windows.ps1`
- Modify: `.github/workflows/symbian.yml` (новый job после `build`)

**Interfaces:**
- Produces: `ci-windows.ps1 -Step <name>` — каждый следующий шаг CI добавляет сюда свою ветку `switch`.
- Produces: вывод `list-conditionals.sh` в формате `path:line:text`.

- [ ] **Step 1: Скрипт инвентаризации**

```bash
#!/usr/bin/env bash
# Lists where the port's sources depend on Symbian: OS(SYMBIAN) and
# __SYMBIAN32__, outside the Symbian-only directories. Each site is either
# behaviour of the port (becomes PLATFORM(SYMBIAN), built on the test host
# too) or a Symbian API call (stays OS(SYMBIAN)).
set -euo pipefail
cd "$(dirname "$0")/../../.."
grep -rnE 'OS\(SYMBIAN\)|__SYMBIAN32__' Source \
  --include='*.h' --include='*.cpp' --include='*.c' --include='*.cmake' --include='*.txt' \
  | grep -vE '^Source/(WTF/wtf/symbian|WebCore/platform/symbian|WebKitLegacy/symbian)/' \
  | sort
```

- [ ] **Step 2: Сценарий CI, шаг `inventory`**

```powershell
# The Windows test host's CI, one step per call: ci-windows.ps1 -Step NAME.
param([Parameter(Mandatory)][string]$Step)
$ErrorActionPreference = 'Stop'
$Repo = (Resolve-Path "$PSScriptRoot\..\..\..").Path
$Work = if ($env:HOST_WORK) { $env:HOST_WORK } else { "$Repo\WebKitBuild\SymbianHost" }
New-Item -ItemType Directory -Force $Work | Out-Null

function Invoke-Native([string]$exe, [string[]]$arguments) {
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "$exe failed with $LASTEXITCODE" }
}

switch ($Step) {
    'inventory' {
        Invoke-Native 'bash' @("$Repo/Tools/Symbian/host/list-conditionals.sh")
        & cl.exe 2>&1 | Select-Object -First 1
    }
    default { throw "unknown step $Step" }
}
```

- [ ] **Step 3: Job в workflow**

Добавить в `.github/workflows/symbian.yml` под `jobs:` после job `build`:

```yaml
  windows-host:
    runs-on: windows-2022
    timeout-minutes: 350
    defaults:
      run:
        shell: pwsh
    steps:
      - uses: actions/checkout@v4
        with:
          filter: blob:none
          sparse-checkout: |
            .github
            Source/JavaScriptCore
            Source/ThirdParty/ANGLE
            Source/ThirdParty/capstone
            Source/WTF
            Source/WebCore
            Source/WebInspectorUI
            Source/WebKitLegacy
            Source/bmalloc
            Source/cmake
            Tools/Scripts
            Tools/Symbian
      - uses: ilammy/msvc-dev-cmd@v1
        with:
          arch: x86
      - name: Inventory
        run: Tools/Symbian/host/ci-windows.ps1 -Step inventory
```

- [ ] **Step 4: Коммит и push**

```bash
git add Tools/Symbian/host .github/workflows/symbian.yml
git commit -m "Add the Windows test host's CI job, listing the port's Symbian conditionals

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push fork HEAD:2.32.4-symbian
```

- [ ] **Step 5: Проверка**

Ожидается: job `windows-host` зелёный, в логе шага Inventory список `path:line:text` и строка `Microsoft (R) C/C++ Optimizing Compiler Version 19.4x for x86`. Job `build` зелёный. Скопировать список из лога в `Tools/Symbian/docs/WindowsHost-conditionals.txt` — он нужен в Task 2.

---

### Task 2: `PLATFORM(SYMBIAN)`

**Files:**
- Modify: `Source/WTF/wtf/PlatformLegacy.h` (блок `#if defined(BUILDING_GTK__)`, строки ~49–75)
- Modify: `Source/cmake/OptionsSymbian.cmake` (добавить определение)
- Modify: каждый файл из `WindowsHost-conditionals.txt`, где место — поведение порта
- Create: `Tools/Symbian/docs/WindowsHost-conditionals.txt` (из Task 1, с пометкой решения по каждой строке)

**Interfaces:**
- Produces: макрос `PLATFORM(SYMBIAN)`, истинный в обеих сборках при `-DBUILDING_SYMBIAN__`. На стенде `PLATFORM(WIN)` ложен.

- [ ] **Step 1: Макрос**

В `PlatformLegacy.h` вставить ветку сразу после ветки `BUILDING_WPE__`: она должна стоять до `#elif OS(WINDOWS)`, чтобы стенд не получил `PLATFORM(WIN)`.

```c
#elif defined(BUILDING_SYMBIAN__)
/* PLATFORM(SYMBIAN) - the Symbian port: on the phone, and on its Windows test host */
#define WTF_PLATFORM_SYMBIAN 1
```

В `OptionsSymbian.cmake` сразу после `find_package(Threads REQUIRED)`:

```cmake
add_definitions(-DBUILDING_SYMBIAN__)
```

(В Task 3 эта строка переедет в `OptionsSymbianCommon.cmake`.)

- [ ] **Step 2: Разметить места**

Для каждой строки `WindowsHost-conditionals.txt` решить и дописать в конец строки ` -> PLATFORM` или ` -> OS`:

- `-> OS`: ветка вызывает Symbian или Open C API (`User::`, `RChunk`, `e32*.h`, `Math::Random`, обход отсутствующих в Open C функций), включает Symbian-заголовок, касается ARM-тулчейна SDK или E32.
- `-> PLATFORM`: ветка задаёт поведение браузера (настройки, сеть, шрифты, WebGL без композитинга, `Accept-Language`, crypto через mbedTLS, выбор USE_/ENABLE_ флагов).

Уже известные места этой сессии, все `-> PLATFORM`:
- `Source/WebCore/html/HTMLCanvasElement.cpp` — `requiresAcceleratedCompositingForWebGL`;
- `Source/WebCore/platform/network/curl/CurlRequest.cpp` — `appendAcceptLanguageHeader`;
- `Source/WTF/wtf/Platform.h:151` — `(OS(SYMBIAN) && USE(MBEDTLS))`;
- `Source/WTF/wtf/PlatformUse.h:279`.

Сомнительное место отмечать `-> PLATFORM`: ошибку сборки стенда увидим в Task 5–6 и тогда перенесём.

- [ ] **Step 3: Заменить**

В каждом месте `-> PLATFORM` заменить `OS(SYMBIAN)` на `PLATFORM(SYMBIAN)`. `__SYMBIAN32__` в C++ коде WebKit заменить на `PLATFORM(SYMBIAN)`, если это поведение. В сторонних C-библиотеках (ANGLE, capstone) оставить `__SYMBIAN32__`: там нет WTF-макросов.

- [ ] **Step 4: Проверка, что телефон не сломан**

```bash
git add -A Source Tools/Symbian/docs/WindowsHost-conditionals.txt
git commit -m "[Symbian] Mark the port's behaviour PLATFORM(SYMBIAN), leaving OS(SYMBIAN) to Symbian API calls

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push fork HEAD:2.32.4-symbian
```

Ожидается: job `build` зелёный. Размер `wkengine.exe` в логе шага упаковки тот же ±1 КБ (поведение не изменилось).

---

### Task 3: CMake-порт `SymbianHost`

**Files:**
- Create: `Source/cmake/OptionsSymbianCommon.cmake`
- Modify: `Source/cmake/OptionsSymbian.cmake`
- Create: `Source/cmake/OptionsSymbianHost.cmake`
- Modify: `Source/cmake/WebKitCommon.cmake:33-44` (`ALL_PORTS`)
- Create: `Source/WTF/wtf/PlatformSymbianHost.cmake`, `Source/JavaScriptCore/PlatformSymbianHost.cmake`, `Source/bmalloc/PlatformSymbianHost.cmake`, `Source/WebCore/PAL/pal/PlatformSymbianHost.cmake`, `Source/WebCore/PlatformSymbianHost.cmake`, `Source/WebKitLegacy/PlatformSymbianHost.cmake`, `Source/ThirdParty/ANGLE/PlatformSymbianHost.cmake`
- Modify: `Tools/Symbian/host/ci-windows.ps1` (шаг `configure`)

**Interfaces:**
- Consumes: `BUILDING_SYMBIAN__` из Task 2.
- Produces: `cmake -DPORT=SymbianHost` конфигурируется; каталог сборки `$Work\build`.

- [ ] **Step 1: Вынести общее**

`OptionsSymbianCommon.cmake` получает из `OptionsSymbian.cmake` без изменений:
- `add_definitions(-DBUILDING_SYMBIAN__)`;
- блок `WEBKIT_OPTION_BEGIN()` … `WEBKIT_OPTION_END()` со всеми `WEBKIT_OPTION_DEFAULT_PORT_VALUE`;
- `set(ENABLE_WEBCORE ON)`, `set(ENABLE_WEBKIT_LEGACY ON)`, `set(ENABLE_WEBKIT OFF)`;
- все `SET_AND_EXPOSE_TO_BUILD(USE_*)`;
- переменные `*_FRAMEWORK_HEADERS_DIR`;
- все `find_package(...)`.

В `OptionsSymbian.cmake` на месте вынесенного остаётся `include(OptionsSymbianCommon)`. Только телефонное остаётся там:
- `CMAKE_*_VISIBILITY_PRESET`;
- `SYMBIAN_SPEED_FLAGS`;
- `*_LIBRARY_TYPE STATIC`;
- `ENABLE_WEBINSPECTORUI OFF`;
- блок `if (ENABLE_WEBGL)` с копированием Khronos-заголовков и `IMPORT_C=`.

Опции, которые стенд должен переопределить, задаются в Common через `WEBKIT_OPTION_DEFAULT_PORT_VALUE` только если ещё не заданы. Для этого перед `include(OptionsSymbianCommon)` порт ставит переменные `SYMBIAN_ENABLE_JIT` и `SYMBIAN_ENABLE_DFG_JIT`, а Common пишет:

```cmake
if (NOT DEFINED SYMBIAN_ENABLE_JIT)
    set(SYMBIAN_ENABLE_JIT ON)
endif ()
if (NOT DEFINED SYMBIAN_ENABLE_DFG_JIT)
    set(SYMBIAN_ENABLE_DFG_JIT ON)
endif ()
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_JIT PRIVATE ${SYMBIAN_ENABLE_JIT})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_DFG_JIT PRIVATE ${SYMBIAN_ENABLE_DFG_JIT})
```

- [ ] **Step 2: Опции стенда**

```cmake
# The Symbian port's Windows test host: the phone's engine (OptionsSymbianCommon)
# on 32-bit x86 Windows, for debugging pages with Web Inspector. WebKit 2.32 has
# no JIT on 32-bit x86 Windows, so JavaScript runs in the LLInt; values are
# JSVALUE32_64 as on the phone.
set(SYMBIAN_ENABLE_JIT OFF)
set(SYMBIAN_ENABLE_DFG_JIT OFF)
include(OptionsSymbianCommon)

add_definitions(-DBUILDING_SYMBIAN_HOST__ -DNOMINMAX -DWIN32_LEAN_AND_MEAN -D_WINDOWS
    -DWINVER=0x601 -D_WIN32_WINNT=0x601 -D_CRT_SECURE_NO_WARNINGS -D_UNICODE -DUNICODE)
add_compile_options(/utf-8 /Zc:__cplusplus /permissive- /bigobj /MP)

set(ENABLE_WEBINSPECTORUI ON)
set(USE_SYSTEM_MALLOC ON)
set(bmalloc_LIBRARY_TYPE STATIC)
set(WTF_LIBRARY_TYPE STATIC)
set(JavaScriptCore_LIBRARY_TYPE STATIC)
set(PAL_LIBRARY_TYPE STATIC)
set(WebCore_LIBRARY_TYPE STATIC)
set(WebKitLegacy_LIBRARY_TYPE STATIC)

if (ENABLE_WEBGL)
    # ANGLE's own EGL and GLES2 (D3D11) stand in for the phone's Broadcom
    # drivers; the WebGL code (GraphicsContextGLSymbian.cpp) is the same.
    SET_AND_EXPOSE_TO_BUILD(USE_OPENGL_ES TRUE)
    SET_AND_EXPOSE_TO_BUILD(USE_OPENGL FALSE)
    set(USE_ANGLE_EGL ON)
    set(USE_ANGLE_WEBGL OFF)
endif ()
```

`WebKitCommon.cmake`: в `ALL_PORTS` после `Symbian` добавить строку `SymbianHost`.

- [ ] **Step 3: Platform-файлы**

Каждый `PlatformSymbianHost.cmake` начинается с `include(PlatformSymbian.cmake)` своего каталога и правит только системную часть. WTF:

```cmake
include(${CMAKE_CURRENT_LIST_DIR}/PlatformSymbian.cmake)
# The same generic event loop as on the phone; files, threads, memory and
# time from Windows instead of Open C.
list(REMOVE_ITEM WTF_SOURCES
    posix/FileSystemPOSIX.cpp
    posix/ThreadingPOSIX.cpp
    symbian/CPUTimeSymbian.cpp
    symbian/LibcxxSupport.cpp
    symbian/OSAllocatorSymbian.cpp
    symbian/SymbianSystem.cpp
    text/unix/TextBreakIteratorInternalICUUnix.cpp
    unix/LanguageUnix.cpp
    unix/UniStdExtrasUnix.cpp
)
list(REMOVE_ITEM WTF_PUBLIC_HEADERS symbian/SymbianSystem.h)
list(APPEND WTF_SOURCES
    text/win/StringWin.cpp
    text/win/TextBreakIteratorInternalICUWin.cpp
    win/CPUTimeWin.cpp
    win/DbgHelperWin.cpp
    win/FileSystemWin.cpp
    win/LanguageWin.cpp
    win/OSAllocatorWin.cpp
    win/PathWalker.cpp
    win/SignalsWin.cpp
    win/ThreadingWin.cpp
)
list(APPEND WTF_LIBRARIES winmm shlwapi dbghelp)
```

Перед записью сверить имена файлов `win/*` и `text/win/*` с `git show HEAD:Source/WTF/wtf/PlatformWin.cmake`: брать оттуда всё, кроме `win/MainThreadWin.cpp`, `win/RunLoopWin.cpp`, `win/WorkQueueWin.cpp`, `win/MemoryFootprintWin.cpp` и `win/MemoryPressureHandlerWin.cpp`. Их заменяют generic-файлы из Symbian-списка.

Для JavaScriptCore, bmalloc, PAL и WebKitLegacy достаточно одной строки:

```cmake
include(${CMAKE_CURRENT_LIST_DIR}/PlatformSymbian.cmake)
```

Если `PlatformSymbian.cmake` в каталоге нет (bmalloc, PAL), файл пустой, с одним комментарием:

```cmake
# The Symbian port's Windows test host builds this project as the phone does.
```

WebCore:

```cmake
include(${CMAKE_CURRENT_LIST_DIR}/PlatformSymbian.cmake)
# Windows instead of Open C for files and memory-mapped buffers; the rest of
# the port's sources are the phone's.
list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES "SourcesSymbianHost.txt")
list(APPEND WebCore_LIBRARIES crypt32 ws2_32 winmm)
```

Create `Source/WebCore/SourcesSymbianHost.txt` — пустой, только с лицензионным заголовком из `SourcesSymbian.txt`. Его наполнит Task 6, когда станет видно, что из POSIX не собирается.

ANGLE: телефон собирает только транслятор; стенду нужны libEGL и libGLESv2. Сначала прочитать `git show HEAD:Source/ThirdParty/ANGLE/PlatformWin.cmake` и `git show HEAD:Source/ThirdParty/ANGLE/CMakeLists.txt`. Затем `PlatformSymbianHost.cmake` = содержимое `PlatformWin.cmake` (D3D11-бэкенд, `ANGLE_ENABLE_D3D11`) без D3D9.

- [ ] **Step 4: Шаг `configure` в CI**

В `ci-windows.ps1` добавить в `switch`:

```powershell
    'configure' {
        $prefix = "$Work\vcpkg_installed\x86-windows"
        $net = "$Work\net\install"
        Invoke-Native 'cmake' @('-S', $Repo, '-B', "$Work\build", '-G', 'Ninja',
            '-DPORT=SymbianHost', '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
            '-DCMAKE_C_COMPILER=cl', '-DCMAKE_CXX_COMPILER=cl',
            "-DCMAKE_PREFIX_PATH=$net;$prefix", '-DDEVELOPER_MODE=OFF')
    }
```

В workflow после `Inventory` добавить шаги `deps` (Task 4) и:

```yaml
      - name: Configure
        run: Tools/Symbian/host/ci-windows.ps1 -Step configure
```

- [ ] **Step 5: Коммит, проверка**

Коммит: `"Add the SymbianHost CMake port: the phone's options on 32-bit Windows"`. Ожидается: `build` зелёный (телефон собирается из Common без изменений). У `windows-host` шаг `Configure` пройдёт только после Task 4, поэтому эту задачу пушить вместе с Task 4.

---

### Task 4: Зависимости стенда

**Files:**
- Create: `Tools/Symbian/scripts/patch-curl.sh`
- Modify: `Tools/Symbian/scripts/build-deps.sh` (`build_curl` зовёт `patch-curl.sh`)
- Create: `Tools/Symbian/host/vcpkg.json`
- Create: `Tools/Symbian/host/build-net.ps1`
- Create: `Tools/Symbian/host/tests/curl-tls13.c`
- Modify: `Tools/Symbian/host/ci-windows.ps1` (шаги `deps`, `test-net`), workflow

**Interfaces:**
- Produces: `$Work\vcpkg_installed\x86-windows` и `$Work\net\install` (`include`, `lib\libcurl.lib`, `lib\mbedtls.lib`, `lib\mbedx509.lib`, `lib\mbedcrypto.lib`, `lib\nghttp2.lib`).
- Produces: `patch-curl.sh CURL_SOURCE_DIR`, идемпотентный, падает, если хоть один патч не лёг.

- [ ] **Step 1: Общий патч curl**

`patch-curl.sh` — это все команды `sed` и `perl` из `build_curl` в `build-deps.sh` (от `local vtls=` до последнего `sed ... urlapi.c`), дословно, с `$src/curl-7.88.1` → `$1`:

```bash
#!/usr/bin/env bash
# The port's changes to curl 7.88.1, for the phone and the Windows test host:
# patch-curl.sh CURL_SOURCE_DIR. Each change is checked; running it twice
# changes nothing.
set -euo pipefail
dir=${1:?usage: patch-curl.sh CURL_SOURCE_DIR}
vtls=$dir/lib/vtls/mbedtls.c
# ... the sed/perl lines and their checks from build_curl, with
#     "$src/curl-7.88.1" replaced by "$dir" ...
```

Многоточие в блоке выше обозначает перенос существующих строк из `build-deps.sh:269-293` без изменений. Это не новый код. В `build_curl` вместо них:

```bash
  "$REPO/scripts/patch-curl.sh" "$src/curl-7.88.1"
```

Хэш кэша `deps` в workflow считается от `build-deps.sh`; добавить туда `Tools/Symbian/scripts/patch-curl.sh`:

```yaml
          key: deps-${{ hashFiles('Tools/Symbian/scripts/build-deps.sh', 'Tools/Symbian/scripts/patch-curl.sh', 'Tools/Symbian/shims/**', 'Tools/Symbian/scripts/common.sh', 'Tools/Symbian/toolchain/**') }}
```

- [ ] **Step 2: vcpkg.json**

```json
{
  "name": "symbian-webkit-host",
  "version-string": "0",
  "builtin-baseline": "c9fa965c2a1b1334469b4539063f3ce95383653c",
  "dependencies": [
    "cairo", "pixman", "freetype", "harfbuzz", "fontconfig",
    { "name": "icu", "default-features": false },
    "libxml2", "libxslt", "sqlite3", "libjpeg-turbo", "libpng", "libwebp",
    "libpsl", "zlib"
  ]
}
```

`builtin-baseline` — коммит vcpkg, который стоит в образе `windows-2022` на момент работы: `git -C $env:VCPKG_INSTALLATION_ROOT rev-parse HEAD` в CI. Вывести его в шаге `deps` и вписать сюда, заменив значение выше.

- [ ] **Step 3: Сеть для стенда**

```powershell
# mbedTLS 3.4.1 with the phone's configuration, nghttp2 1.52.0 and curl 7.88.1
# with the port's patches, for the Windows test host.
param([Parameter(Mandatory)][string]$Work, [Parameter(Mandatory)][string]$SdkConfig)
$ErrorActionPreference = 'Stop'
$src = "$Work\net\src"; $out = "$Work\net\install"
New-Item -ItemType Directory -Force $src, $out | Out-Null
function Get-Source($url, $dir) {
    $archive = "$src\$(Split-Path $url -Leaf)"
    if (-not (Test-Path $archive)) { Invoke-WebRequest $url -OutFile $archive }
    if (-not (Test-Path "$src\$dir")) { tar -xf $archive -C $src; if ($LASTEXITCODE) { throw "tar $archive" } }
}
function Build-CMake($dir, [string[]]$options) {
    cmake -S "$src\$dir" -B "$src\$dir\build" -G Ninja -DCMAKE_BUILD_TYPE=Release `
        "-DCMAKE_INSTALL_PREFIX=$out" "-DCMAKE_PREFIX_PATH=$out" @options
    if ($LASTEXITCODE) { throw "configure $dir" }
    cmake --build "$src\$dir\build" --target install
    if ($LASTEXITCODE) { throw "build $dir" }
}

Get-Source 'https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v3.4.1.tar.gz' 'mbedtls-3.4.1'
Copy-Item $SdkConfig "$src\mbedtls-3.4.1\include\mbedtls\mbedtls_config.h" -Force
Build-CMake 'mbedtls-3.4.1' @('-DENABLE_PROGRAMS=OFF', '-DENABLE_TESTING=OFF', '-DUSE_STATIC_MBEDTLS_LIBRARY=ON')

Get-Source 'https://github.com/nghttp2/nghttp2/releases/download/v1.52.0/nghttp2-1.52.0.tar.xz' 'nghttp2-1.52.0'
Build-CMake 'nghttp2-1.52.0' @('-DENABLE_LIB_ONLY=ON', '-DENABLE_STATIC_LIB=ON', '-DENABLE_SHARED_LIB=OFF')

Get-Source 'https://curl.se/download/curl-7.88.1.tar.xz' 'curl-7.88.1'
bash "$PSScriptRoot/../scripts/patch-curl.sh" ("$src\curl-7.88.1" -replace '\\', '/')
if ($LASTEXITCODE) { throw 'patch-curl.sh' }
Build-CMake 'curl-7.88.1' @('-DBUILD_CURL_EXE=OFF', '-DBUILD_TESTING=OFF', '-DBUILD_SHARED_LIBS=OFF',
    '-DCURL_USE_SCHANNEL=OFF', '-DCURL_USE_OPENSSL=OFF', '-DCURL_USE_MBEDTLS=ON', '-DUSE_NGHTTP2=ON',
    '-DCMAKE_C_FLAGS=-DNGHTTP2_STATICLIB', '-DENABLE_THREADED_RESOLVER=ON', '-DENABLE_IPV6=OFF',
    '-DCURL_DISABLE_LDAP=ON', '-DCURL_USE_LIBSSH2=OFF', '-DUSE_LIBIDN2=OFF', '-DCURL_BROTLI=OFF',
    '-DCURL_ZSTD=OFF', '-DCURL_CA_BUNDLE=none', '-DCURL_CA_PATH=none',
    "-DMBEDTLS_INCLUDE_DIRS=$out\include", "-DMBEDTLS_LIBRARY=$out\lib\mbedtls.lib",
    "-DMBEDX509_LIBRARY=$out\lib\mbedx509.lib", "-DMBEDCRYPTO_LIBRARY=$out\lib\mbedcrypto.lib",
    "-DNGHTTP2_INCLUDE_DIR=$out\include", "-DNGHTTP2_LIBRARY=$out\lib\nghttp2.lib")
```

Конфиг mbedTLS берётся из SDK телефона: в шаге `deps` скачать `symbian-sdk-0.3.0-linux-x86_64.tar.gz` (`gh release download v0.3.0 --repo hpnkv/symbian-platform`, как в job `build`) и распаковать только `lib/armv6/mbedtls/include/mbedtls/mbedtls_config.h`. Если внутри SDK путь другой, найти его командой `tar -tzf ... | grep mbedtls_config.h`. После этого применить к нему `shims/mbedtls-config.h` тем же способом, что телефон: дописать в конец скопированного файла строку `#include "<абсолютный путь>/Tools/Symbian/shims/mbedtls-config.h"`.

- [ ] **Step 4: Тест сети (сначала падающий)**

`tests/curl-tls13.c`:

```c
// Fetches a page through the host's curl and prints what was negotiated:
// curl-tls13 URL CA_BUNDLE. Exits 0 only on HTTP/2 over TLS 1.3.
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>

static size_t discard(char* data, size_t size, size_t count, void* context)
{
    (void)data; (void)context;
    return size * count;
}

static int sawTLS13;

static int trace(CURL* handle, curl_infotype type, char* data, size_t size, void* context)
{
    (void)handle; (void)context;
    if (type == CURLINFO_TEXT && size > 0) {
        fwrite(data, 1, size, stdout);
        if (strstr(data, "TLS1-3"))
            sawTLS13 = 1;
    }
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 3)
        return 2;
    curl_global_init(CURL_GLOBAL_ALL);
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, argv[1]);
    curl_easy_setopt(curl, CURLOPT_CAINFO, argv[2]);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2 | CURL_SSLVERSION_MAX_TLSv1_3);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, trace);
    CURLcode result = curl_easy_perform(curl);
    long version = 0;
    curl_easy_getinfo(curl, CURLINFO_HTTP_VERSION, &version);
    printf("result %d, http version %ld, tls 1.3 %d\n", result, version, sawTLS13);
    curl_easy_cleanup(curl);
    return result == CURLE_OK && version == CURL_HTTP_VERSION_2_0 && sawTLS13 ? 0 : 1;
}
```

Шаги CI:

```powershell
    'deps' {
        Invoke-Native "$env:VCPKG_INSTALLATION_ROOT\vcpkg.exe" @('install', '--triplet', 'x86-windows',
            "--x-manifest-root=$Repo\Tools\Symbian\host", "--x-install-root=$Work\vcpkg_installed")
        & git -C $env:VCPKG_INSTALLATION_ROOT rev-parse HEAD
        & "$PSScriptRoot\build-net.ps1" -Work $Work -SdkConfig "$Work\sdk\mbedtls_config.h"
    }
    'test-net' {
        $net = "$Work\net\install"
        Invoke-Native 'cl' @('/nologo', "/I$net\include", '/DCURL_STATICLIB',
            "$PSScriptRoot\tests\curl-tls13.c", "/Fe$Work\curl-tls13.exe", '/link',
            "$net\lib\libcurl.lib", "$net\lib\nghttp2.lib", "$net\lib\mbedtls.lib",
            "$net\lib\mbedx509.lib", "$net\lib\mbedcrypto.lib",
            "$Work\vcpkg_installed\x86-windows\lib\zlib.lib", 'ws2_32.lib', 'crypt32.lib',
            'advapi32.lib', 'bcrypt.lib')
        Invoke-Native "$Work\curl-tls13.exe" @('https://www.google.com/', "$Work\cacert.pem")
    }
```

`$Work\cacert.pem` — тот же файл, что телефон кладёт в `C:/resource/webkit/cacert.pem`. Найти, откуда его берёт `package-sis.sh` (`grep -n cacert Tools/Symbian/scripts/*.sh`), и скопировать тем же способом в шаге `deps`.

Workflow:

```yaml
      - name: Restore the host dependencies
        uses: actions/cache@v4
        with:
          path: |
            WebKitBuild/SymbianHost/vcpkg_installed
            WebKitBuild/SymbianHost/net/install
          key: host-deps-${{ hashFiles('Tools/Symbian/host/vcpkg.json', 'Tools/Symbian/host/build-net.ps1', 'Tools/Symbian/scripts/patch-curl.sh', 'Tools/Symbian/shims/mbedtls-config.h') }}
      - name: Dependencies
        env:
          GH_TOKEN: ${{ github.token }}
        run: Tools/Symbian/host/ci-windows.ps1 -Step deps
      - name: Network test
        run: Tools/Symbian/host/ci-windows.ps1 -Step test-net
```

- [ ] **Step 5: Коммит Task 3 + Task 4, проверка**

```bash
git add -A Source/cmake Source/WTF Source/JavaScriptCore Source/bmalloc Source/WebCore Source/WebKitLegacy Source/ThirdParty/ANGLE Tools/Symbian .github
git commit -m "Add the SymbianHost port and its dependencies: the phone's curl, mbedTLS and nghttp2 on Windows

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push fork HEAD:2.32.4-symbian
```

Ожидается:
- `build` зелёный;
- в `windows-host` шаг `Network test` печатает `result 0, http version 3, tls 1.3 1` (`CURL_HTTP_VERSION_2_0` = 3), в трассе есть `TLS1-3-AES-256-GCM-SHA384`;
- `Configure` проходит до конца.

Если `Configure` падает, ошибки CMake чинить в Platform-файлах стенда, а не в файлах телефона.

---

### Task 5: WTF и JavaScriptCore на стенде

**Files:**
- Modify: `Tools/Symbian/host/ci-windows.ps1` (шаги `build-jsc`, `test-jsc`)
- Modify: исходники WTF/JSC — только то, что не собирается MSVC; правки под `PLATFORM(SYMBIAN)` или `COMPILER(MSVC)`
- Create: `Tools/Symbian/host/tests/expected-es2020.txt`

**Interfaces:**
- Produces: `$Work\build\bin32\jsc.exe`.

- [ ] **Step 1: Шаги CI**

```powershell
    'build-jsc' {
        Invoke-Native 'cmake' @('--build', "$Work\build", '--target', 'jsc')
    }
    'test-jsc' {
        $jsc = Get-ChildItem "$Work\build" -Recurse -Filter jsc.exe | Select-Object -First 1
        $out = & $jsc.FullName "$Repo\Tools\Symbian\tests\es2020.js" 2>&1 | Out-String
        $out
        $expected = Get-Content "$PSScriptRoot\tests\expected-es2020.txt" -Raw
        if ($out.Trim() -replace "`r", '' -ne $expected.Trim() -replace "`r", '') { throw 'es2020.js output differs' }
        & $jsc.FullName "$Repo\Tools\Symbian\tests\intl.js"
    }
```

- [ ] **Step 2: Ожидаемый вывод (падающий тест)**

`expected-es2020.txt` создать из вывода `jsc.exe` на телефоне или в EKA2L1 (`Tools/Symbian/eka2l1/run.sh C:/Data/es2020.js` → `C:\Data\jsc.log`). Если ни того, ни другого нет, первым вариантом взять вывод стенда из первого зелёного запуска. Тогда проверить вручную каждую строку по `es2020.js`, например `fib(24): 46368`, `allSettled: fulfilled,rejected`, `intl: 1,234,567.891`. Пока файла нет, шаг `test-jsc` падает.

- [ ] **Step 3: Сборка и починка**

Push, прочитать ошибки шага `Build JSC` в логе. Чинить по одной группе ошибок за коммит. Правила:
- код порта (`#if PLATFORM(SYMBIAN)`), который MSVC не понимает (атрибуты GCC, `__attribute__((format))`, VLA), переписать переносимо, для обеих сборок;
- если что-то ломается в апстримном коде WinCairo для 32 бит, сначала проверить, есть ли такая же правка в апстриме после 2.32: `git log` по файлу на github.com/WebKit/WebKit. Если есть, перенести её, указав в сообщении коммита апстримную ревизию.

- [ ] **Step 4: Проверка**

Ожидается: `test-jsc` зелёный, вывод совпадает с `expected-es2020.txt`, `intl.js` без исключений. Job `build` зелёный.

---

### Task 6: WebCore и WebKitLegacy на стенде

**Files:**
- Modify: `Tools/Symbian/host/ci-windows.ps1` (шаг `build`)
- Modify: `Source/WebCore/SourcesSymbianHost.txt`, `Source/WebCore/PlatformSymbianHost.cmake`
- Modify: файлы `Source/WebCore/platform/symbian/*`, где нужен `PLATFORM(SYMBIAN)` или `OS(SYMBIAN)`
- Modify: `Source/WebKitLegacy/symbian/WebViewImpl.cpp` (`initializeFonts`)

**Interfaces:**
- Produces: статические `WebCore.lib`, `WebKitLegacy.lib`, `PAL.lib`, `JavaScriptCore.lib`, `WTF.lib` в `$Work\build\lib32`.

- [ ] **Step 1: Шаг CI**

```powershell
    'build' {
        Invoke-Native 'cmake' @('--build', "$Work\build")
    }
```

Если нет вывода 40 минут, ninja на раннере не виснет — просто идёт тихо; не трогать. Таймаут job 350 минут.

- [ ] **Step 2: Известные системные места**

- `CurlSSLHandleSymbian.cpp:33` — `defaultCACertPath = "C:/resource/webkit/cacert.pem"`. Под `OS(SYMBIAN)` оставить этот путь. Иначе брать `cacert.pem` рядом с exe:

```cpp
#if OS(SYMBIAN)
static const char* defaultCACertPath = "C:/resource/webkit/cacert.pem";
#else
static String defaultCACertPath()
{
    wchar_t module[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, module, MAX_PATH);
    return FileSystem::pathByAppendingComponent(
        FileSystem::directoryName(String(module, length)), "cacert.pem");
}
#endif
```

  Места, где используется `defaultCACertPath`, привести к `String` в обеих ветках.

- `platform/posix/SharedBufferPOSIX.cpp` из `SourcesSymbian.txt` под MSVC не соберётся: нет `mmap`. Убрать его через `list(REMOVE_ITEM WebCore_SOURCES ...)` нельзя: unified-списки собираются из txt-файлов позже. Поэтому строку в `SourcesSymbian.txt` не трогать, а тело самого файла обернуть в `#if !OS(WINDOWS)` … `#endif`. В `SourcesSymbianHost.txt` добавить файл, который даёт `SharedBuffer::createFromReadingFile` в WinCairo. Найти её: `git show HEAD:Source/WebCore/SourcesWin.txt | grep -i sharedbuffer`.

- `initializeFonts` в `WebViewImpl.cpp`: каталоги `Z:/C:/E:/F:` нужны только телефону.

```cpp
    CString xml = makeString("<fontconfig>"
#if OS(SYMBIAN)
        "<dir>Z:/resource/fonts</dir><dir>C:/resource/fonts</dir>"
        "<dir>E:/resource/fonts</dir><dir>F:/resource/fonts</dir>"
#endif
        "<dir>", fontDirectory, "</dir><cachedir>", cacheDirectory, "</cachedir>"
```

- [ ] **Step 3: Цикл починки**

Как в Task 5: push, лог шага `Build`, одна группа ошибок за коммит. Каждый коммит должен оставлять `build` зелёным.

- [ ] **Step 4: Проверка**

Ожидается: шаг `Build` зелёный, все пять `.lib` на месте. `build` зелёный.

---

### Task 7: `wkhost.exe`

**Files:**
- Create: `Tools/Symbian/host/HostLog.h`, `HostLog.cpp`, `HostSettings.h`, `HostSettings.cpp`, `HostWindow.h`, `HostWindow.cpp`, `main.cpp`, `CMakeLists.txt`
- Create: `Tools/Symbian/host/tests/smoke.html`
- Modify: `Source/cmake/OptionsSymbianHost.cmake` (`add_subdirectory` хоста), `ci-windows.ps1` (шаг `smoke`)

**Interfaces:**
- Consumes: `WebKitSymbian::WebView` и `WebViewClient` из `Source/WebKitLegacy/symbian/WebKitSymbian.h` (сигнатуры там, менять их не нужно).
- Produces: `wkhost.exe [URL] [--data DIR] [--shot FILE.bmp] [--wait SECONDS] [--inspect]`.
  - `--shot`: ждать `--wait` секунд (по умолчанию 10), сохранить вид в BMP и выйти с кодом 0. Это для CI.
- Produces: `void HostLogOpen(const char* directory)`, `void HostLog(const char* format, ...)`.
- Produces: `struct HostSettings { std::string userAgent; std::vector<std::string> languages; }; HostSettings HostReadSettings(const std::string& directory);`.

- [ ] **Step 1: Тестовая страница и шаг (падающий тест)**

`tests/smoke.html`:

```html
<!doctype html>
<meta charset="utf-8">
<title>smoke</title>
<body style="margin:0;background:#00ff00">
<div id="out" style="font:20px sans-serif">...</div>
<script>
const results = [];
try { localStorage.setItem("k", "v"); results.push("localStorage " + localStorage.getItem("k")); }
catch (e) { results.push("localStorage " + e); }
const gl = document.createElement("canvas").getContext("webgl");
results.push("webgl " + (gl ? gl.getParameter(gl.VERSION) : "none"));
results.push("ua " + navigator.userAgent);
results.push("languages " + navigator.languages.join(","));
results.push("screen " + screen.width + "x" + screen.height);
console.log("smoke: " + results.join(" | "));
document.getElementById("out").textContent = results.join("\n");
</script>
```

Шаг CI:

```powershell
    'smoke' {
        $exe = Get-ChildItem "$Work\build" -Recurse -Filter wkhost.exe | Select-Object -First 1
        $data = "$Work\smoke-data"
        Remove-Item -Recurse -Force $data -ErrorAction SilentlyContinue
        $page = 'file:///' + ("$PSScriptRoot\tests\smoke.html" -replace '\\', '/')
        Invoke-Native $exe.FullName @($page, '--data', $data, '--shot', "$Work\smoke.bmp", '--wait', '15')
        $log = Get-Content "$data\log.txt" -Raw
        $log
        if ($log -notmatch 'console: .*smoke: localStorage v') { throw 'no localStorage' }
        if ($log -notmatch 'languages en-GB') { throw 'languages' }
        Add-Type -AssemblyName System.Drawing
        $bitmap = [System.Drawing.Bitmap]::FromFile("$Work\smoke.bmp")
        $pixel = $bitmap.GetPixel(350, 500)
        if ($pixel.G -lt 200 -or $pixel.R -gt 50) { throw "page not painted: $pixel" }
    }
```

- [ ] **Step 2: Лог**

`HostLog.cpp` — тот же формат, что `browser/BrowserLog.cpp`: `log.txt` с поворотом в `log.old.txt`, stderr перенаправлен в файл, построчный буфер, строки `HH:MM:SS.mmm сообщение`. `WTFLogAlways` пишет в stderr и попадает туда же, как на телефоне.

```cpp
#include "HostLog.h"

#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>

void HostLogOpen(const char* directory)
{
    std::string path = std::string(directory) + "/log.txt";
    std::string oldPath = std::string(directory) + "/log.old.txt";
    remove(oldPath.c_str());
    rename(path.c_str(), oldPath.c_str());
    if (!freopen(path.c_str(), "w", stderr))
        return;
    setvbuf(stderr, nullptr, _IOLBF, 0);
}

void HostLog(const char* format, ...)
{
    char message[512];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    SYSTEMTIME time;
    GetLocalTime(&time);
    fprintf(stderr, "%02d:%02d:%02d.%03d %s\n", time.wHour, time.wMinute, time.wSecond,
        time.wMilliseconds, message);
}
```

`HostLog.h`:

```cpp
#pragma once
void HostLogOpen(const char* directory);
void HostLog(const char* format, ...);
```

- [ ] **Step 3: Настройки**

Те же файлы, что читает `EngineServer.cpp` (`SetUserAgent`, `SetLanguages`). Язык на N8 сейчас `en-GB`, поэтому по умолчанию языки `en-GB`, `en-US`.

```cpp
#include "HostSettings.h"

#include <fstream>

static std::string firstLine(const std::string& path)
{
    std::ifstream file(path);
    std::string line;
    if (file && std::getline(file, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
    }
    return line;
}

HostSettings HostReadSettings(const std::string& directory)
{
    HostSettings settings;
    settings.userAgent = firstLine(directory + "/useragent.txt");
    std::string language = firstLine(directory + "/language.txt");
    settings.languages = { language.empty() ? "en-GB" : language, "en-US" };
    return settings;
}
```

`HostSettings.h`:

```cpp
#pragma once
#include <string>
#include <vector>

struct HostSettings {
    std::string userAgent;
    std::vector<std::string> languages;
};

HostSettings HostReadSettings(const std::string& directory);
```

- [ ] **Step 4: Окно**

Размер вида как на телефоне — 360x510 (`view 360x510` в `log.txt`). Над видом поле адреса высотой 28 пикселей.

`HostWindow.h`:

```cpp
#pragma once
#include <WebKitSymbian.h>
#include <windows.h>
#include <memory>
#include <vector>

class HostWindow final : public WebKitSymbian::WebViewClient {
public:
    static constexpr int viewWidth = 360;
    static constexpr int viewHeight = 510;
    static constexpr int addressHeight = 28;

    HostWindow();
    ~HostWindow();
    void load(const char* url);
    bool saveBitmap(const wchar_t* path);
    HWND handle() const { return m_window; }

    // WebViewClient
    void didInvalidate(const WebKitSymbian::Rect&) override;
    void didChangeTitle(WebKitSymbian::Text) override;
    void didChangeURL(WebKitSymbian::Text) override;
    void runJavaScriptAlert(WebKitSymbian::Text) override;
    bool runJavaScriptConfirm(WebKitSymbian::Text) override;

private:
    static LRESULT CALLBACK windowProcedure(HWND, UINT, WPARAM, LPARAM);
    LRESULT handleMessage(UINT, WPARAM, LPARAM);
    void paint();
    void paintView(const WebKitSymbian::Rect&);

    HWND m_window { nullptr };
    HWND m_address { nullptr };
    std::unique_ptr<WebKitSymbian::WebView> m_view;
    std::vector<uint32_t> m_pixels;
    RECT m_dirty { };
    bool m_pointerDown { false };
};
```

`HostWindow.cpp`:

```cpp
#include "HostWindow.h"

#include "HostLog.h"
#include <string>
#include <windowsx.h>

using namespace WebKitSymbian;

static const wchar_t className[] = L"WebKitSymbianHost";
static const int addressId = 1;

static std::wstring toWide(Text text)
{
    return std::wstring(reinterpret_cast<const wchar_t*>(text.characters), text.length);
}

HostWindow::HostWindow()
    : m_pixels(viewWidth * viewHeight, 0xffffffff)
{
    WNDCLASSW windowClass { };
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = className;
    RegisterClassW(&windowClass);

    RECT frame { 0, 0, viewWidth, viewHeight + addressHeight };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&frame, style, FALSE);
    m_window = CreateWindowW(className, L"WebKit Symbian host", style, CW_USEDEFAULT, CW_USEDEFAULT,
        frame.right - frame.left, frame.bottom - frame.top, nullptr, nullptr, windowClass.hInstance, this);
    m_address = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        0, 0, viewWidth, addressHeight, m_window, reinterpret_cast<HMENU>(addressId), windowClass.hInstance, nullptr);
    m_view.reset(WebView::create(*this, viewWidth, viewHeight));
    m_view->setFocused(true);
    HostLog("view %dx%d", viewWidth, viewHeight);
    ShowWindow(m_window, SW_SHOW);
}

HostWindow::~HostWindow() = default;

void HostWindow::load(const char* url)
{
    HostLog("load %s", url);
    m_view->load(url);
}

void HostWindow::didInvalidate(const Rect& area)
{
    RECT rect { area.x, area.y + addressHeight, area.x + area.width, area.y + area.height + addressHeight };
    InvalidateRect(m_window, &rect, FALSE);
}

void HostWindow::didChangeTitle(Text title)
{
    SetWindowTextW(m_window, toWide(title).c_str());
}

void HostWindow::didChangeURL(Text url)
{
    SetWindowTextW(m_address, toWide(url).c_str());
}

void HostWindow::runJavaScriptAlert(Text message)
{
    MessageBoxW(m_window, toWide(message).c_str(), L"Alert", MB_OK);
}

bool HostWindow::runJavaScriptConfirm(Text message)
{
    return MessageBoxW(m_window, toWide(message).c_str(), L"Confirm", MB_OKCANCEL) == IDOK;
}

void HostWindow::paintView(const Rect& area)
{
    m_view->paint(reinterpret_cast<uint8_t*>(m_pixels.data()), viewWidth * 4, area);
}

void HostWindow::paint()
{
    PAINTSTRUCT paintStruct;
    HDC context = BeginPaint(m_window, &paintStruct);
    RECT dirty = paintStruct.rcPaint;
    Rect area { dirty.left, dirty.top - addressHeight, dirty.right - dirty.left, dirty.bottom - dirty.top };
    if (area.y < 0) {
        area.height += area.y;
        area.y = 0;
    }
    if (area.width > 0 && area.height > 0) {
        paintView(area);
        BITMAPINFO info { };
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = viewWidth;
        info.bmiHeader.biHeight = -viewHeight;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        SetDIBitsToDevice(context, 0, addressHeight, viewWidth, viewHeight, 0, 0, 0, viewHeight,
            m_pixels.data(), &info, DIB_RGB_COLORS);
    }
    EndPaint(m_window, &paintStruct);
}

bool HostWindow::saveBitmap(const wchar_t* path)
{
    paintView({ 0, 0, viewWidth, viewHeight });
    BITMAPFILEHEADER file { };
    BITMAPINFOHEADER info { };
    info.biSize = sizeof(info);
    info.biWidth = viewWidth;
    info.biHeight = -viewHeight;
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;
    DWORD size = static_cast<DWORD>(m_pixels.size() * 4);
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + size;
    FILE* out = _wfopen(path, L"wb");
    if (!out)
        return false;
    fwrite(&file, sizeof(file), 1, out);
    fwrite(&info, sizeof(info), 1, out);
    fwrite(m_pixels.data(), 1, size, out);
    fclose(out);
    return true;
}

LRESULT CALLBACK HostWindow::windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* self = reinterpret_cast<HostWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (!self || !self->m_window)
        return DefWindowProcW(window, message, wParam, lParam);
    return self->handleMessage(message, wParam, lParam);
}

static unsigned modifiers()
{
    unsigned result = 0;
    if (GetKeyState(VK_SHIFT) < 0)
        result |= ShiftKey;
    if (GetKeyState(VK_CONTROL) < 0)
        result |= ControlKey;
    if (GetKeyState(VK_MENU) < 0)
        result |= AltKey;
    return result;
}

LRESULT HostWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam) - addressHeight;
    switch (message) {
    case WM_PAINT:
        paint();
        return 0;
    case WM_LBUTTONDOWN:
        if (y < 0)
            break;
        SetFocus(m_window);
        SetCapture(m_window);
        m_pointerDown = true;
        m_view->handlePointer(PointerEvent::Down, x, y, 0);
        return 0;
    case WM_MOUSEMOVE:
        if (m_pointerDown)
            m_view->handlePointer(PointerEvent::Move, x, y, 0);
        return 0;
    case WM_LBUTTONUP:
        if (!m_pointerDown)
            break;
        m_pointerDown = false;
        ReleaseCapture();
        m_view->handlePointer(PointerEvent::Up, x, y, 0);
        return 0;
    case WM_MOUSEWHEEL:
        m_view->scrollBy(0, -GET_WHEEL_DELTA_WPARAM(wParam) / 2);
        return 0;
    case WM_KEYDOWN:
    case WM_KEYUP:
        m_view->handleKey(message == WM_KEYDOWN, static_cast<int>(wParam), { }, modifiers());
        return 0;
    case WM_CHAR: {
        uint16_t character = static_cast<uint16_t>(wParam);
        if (character >= 0x20)
            m_view->confirmComposition({ &character, 1 });
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == addressId && HIWORD(wParam) == EN_KILLFOCUS)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(m_window, message, wParam, lParam);
}
```

Ввод адреса: Enter в поле `EDIT` не приходит в `WM_COMMAND`. В `main.cpp` цикл сообщений ловит `WM_KEYDOWN` с `VK_RETURN`, у которого `msg.hwnd` — поле адреса, читает текст через `GetWindowTextW` и вызывает `load()`, переведя текст в UTF-8 (`WideCharToMultiByte(CP_UTF8, ...)`).

- [ ] **Step 5: main.cpp — цикл событий как у `wkengine.exe`**

Движок будит приложение из любого потока через `setWakeUpCallback`: колбэк только шлёт сообщение. Работа идёт в `runPendingWork()` на главном потоке, таймер ставится по `microsecondsUntilNextTimer()`.

```cpp
#include "HostLog.h"
#include "HostSettings.h"
#include "HostWindow.h"

#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>

using namespace WebKitSymbian;

static const UINT wakeUpMessage = WM_APP + 1;
static const UINT_PTR engineTimer = 1;
static DWORD mainThread;

static void wakeUp(void*)
{
    PostThreadMessageW(mainThread, wakeUpMessage, 0, 0);
}

static void runEngine()
{
    WebView::runPendingWork();
    int64_t next = WebView::microsecondsUntilNextTimer();
    if (next >= 0)
        SetTimer(nullptr, engineTimer, static_cast<UINT>(next / 1000) + 1, nullptr);
}

static std::string toUTF8(const std::wstring& text)
{
    int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(length > 0 ? length - 1 : 0, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), length, nullptr, nullptr);
    return result;
}

static std::string defaultDataDirectory()
{
    wchar_t* local = nullptr;
    SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local);
    std::wstring path = std::wstring(local) + L"\\WebKitQt";
    CoTaskMemFree(local);
    CreateDirectoryW(path.c_str(), nullptr);
    return toUTF8(path);
}

int wmain(int argc, wchar_t** argv)
{
    mainThread = GetCurrentThreadId();
    std::string url = "https://duckduckgo.com/lite/";
    std::string data;
    std::wstring shot;
    int wait = 10;
    for (int i = 1; i < argc; ++i) {
        std::wstring argument = argv[i];
        if (argument == L"--data" && i + 1 < argc)
            data = toUTF8(argv[++i]);
        else if (argument == L"--shot" && i + 1 < argc)
            shot = argv[++i];
        else if (argument == L"--wait" && i + 1 < argc)
            wait = _wtoi(argv[++i]);
        else if (argument.rfind(L"--", 0) != 0)
            url = toUTF8(argument);
    }
    if (data.empty())
        data = defaultDataDirectory();
    int wideLength = MultiByteToWideChar(CP_UTF8, 0, data.c_str(), -1, nullptr, 0);
    std::wstring wideData(wideLength > 0 ? wideLength - 1 : 0, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, data.c_str(), -1, wideData.data(), wideLength);
    CreateDirectoryW(wideData.c_str(), nullptr);

    HostLogOpen(data.c_str());
    HostLog("WebKit engine starting");
    WebView::initializeProcess(data.c_str());
    HostSettings settings = HostReadSettings(data);
    std::vector<const char*> languages;
    for (auto& language : settings.languages)
        languages.push_back(language.c_str());
    WebView::setLanguages(languages.data(), static_cast<int>(languages.size()));
    HostLog("languages: %s, %s", languages[0], languages[1]);
    if (!settings.userAgent.empty()) {
        WebView::setUserAgent(settings.userAgent.c_str());
        HostLog("user agent: %s", settings.userAgent.c_str());
    }
    WebView::setWakeUpCallback(wakeUp, nullptr);
    HostLog("engine initialized");

    HostWindow window;
    window.load(url.c_str());
    DWORD deadline = GetTickCount() + wait * 1000;

    MSG message;
    for (;;) {
        DWORD timeout = shot.empty() ? INFINITE : (deadline > GetTickCount() ? deadline - GetTickCount() : 0);
        if (MsgWaitForMultipleObjects(0, nullptr, FALSE, timeout, QS_ALLINPUT) == WAIT_TIMEOUT) {
            runEngine();
            window.saveBitmap(shot.c_str());
            HostLog("screenshot saved, exiting");
            return 0;
        }
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT)
                return 0;
            if (message.message == wakeUpMessage || (message.message == WM_TIMER && !message.hwnd)) {
                runEngine();
                continue;
            }
            if (message.message == WM_KEYDOWN && message.wParam == VK_RETURN
                && GetDlgCtrlID(message.hwnd) == 1) {
                wchar_t text[2048];
                GetWindowTextW(message.hwnd, text, 2048);
                window.load(toUTF8(text).c_str());
                SetFocus(window.handle());
                continue;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}
```

- [ ] **Step 6: CMakeLists.txt**

```cmake
add_executable(wkhost WIN32
    HostLog.cpp
    HostSettings.cpp
    HostWindow.cpp
    main.cpp
)
set_target_properties(wkhost PROPERTIES LINK_FLAGS "/ENTRY:wmainCRTStartup")
target_include_directories(wkhost PRIVATE "${WEBKITLEGACY_DIR}/symbian")
target_link_libraries(wkhost PRIVATE WebKitLegacy)
```

В конце `OptionsSymbianHost.cmake` подключить хост нельзя: Options читаются до проектов. Поэтому добавить в корневой `Source/CMakeLists.txt` после `add_subdirectory(WebKitLegacy)`:

```cmake
if (PORT STREQUAL "SymbianHost")
    add_subdirectory(${CMAKE_SOURCE_DIR}/Tools/Symbian/host ${CMAKE_BINARY_DIR}/wkhost)
endif ()
```

Перед правкой проверить, как подключается WebKitLegacy: `git show HEAD:Source/CMakeLists.txt | grep -n add_subdirectory`.

В workflow после `Build`:

```yaml
      - name: Smoke test
        run: Tools/Symbian/host/ci-windows.ps1 -Step smoke
```

- [ ] **Step 7: Проверка и коммит**

Коммит: `"Add wkhost.exe, the Windows test host's window around the port's WebView"`. Ожидается: шаг `Smoke test` зелёный. В логе:
- `view 360x510`;
- `languages: en-GB, en-US`;
- `console: ... smoke: localStorage v | webgl ...`;
- снимок зелёный в точке (350, 500).

Строка `webgl` на этом этапе может быть `none`: WebGL включается в Task 9.

---

### Task 8: Шрифты как на N8

**Files:**
- Modify: `Tools/Symbian/host/ci-windows.ps1` (шаг `smoke`: проверка счётчика шрифтов)
- Create: `Tools/Symbian/host/README.txt`

**Interfaces:**
- Consumes: `initializeFonts` из Task 6 (на стенде читает только `<data>\fonts`).

- [ ] **Step 1: Тест**

В конец шага `smoke` добавить:

```powershell
        if ($log -notmatch 'fonts: 0') { throw 'the host must see no fonts but its own directory' }
```

На раннере каталог `fonts` пустой. Если fontconfig нашёл системные шрифты Windows, тест упадёт — значит, конфигурация протекает.

- [ ] **Step 2: README.txt**

```text
WebKit Symbian test host (wkhost.exe)

The Symbian port's engine on Windows: the same WebKit code, settings and
network stack (curl 7.88.1, mbedTLS 3.4.1, nghttp2 1.52.0) as on the Nokia N8.

Data directory: %LOCALAPPDATA%\WebKitQt (or --data DIR). It holds log.txt,
log.old.txt, cookies.db, LocalStorage and the files below, as
C:\Data\WebKitQt does on the phone.

  fonts\          copy the phone's fonts here: everything in Z:\resource\fonts
                  (20 files on the N8). The host sees no other fonts.
  useragent.txt   first line replaces the User-Agent string
  language.txt    first line is the preferred language (en-GB by default)
  curl-verbose    while it exists, curl's trace goes to curl.txt

Usage: wkhost.exe [URL] [--data DIR] [--inspect]
```

- [ ] **Step 3: Проверка**

Ожидается: `smoke` зелёный, в логе `fonts: 0`. Ручная проверка пользователем: он кладёт 20 файлов N8 в `%LOCALAPPDATA%\WebKitQt\fonts`, в логе появляется `fonts: 19` (ZDigi отброшен), имена совпадают с `log.txt` телефона.

---

### Task 9: WebGL через ANGLE

**Files:**
- Modify: `Source/ThirdParty/ANGLE/PlatformSymbianHost.cmake` (если Task 3 не собрал libEGL/libGLESv2)
- Modify: `Source/WebCore/PlatformSymbianHost.cmake` (линковка с `ANGLE::EGL`, `ANGLE::GLES`)
- Modify: `Source/WebCore/platform/symbian/GraphicsContextGLSymbian.cpp` — только если код не переносим; поведение не менять
- Modify: `ci-windows.ps1` (`smoke`: проверка `webgl WebGL 1.0`), `package` (DLL ANGLE)

**Interfaces:**
- Consumes: `GraphicsContextGLSymbian.cpp` (EGL pbuffer 1x1, ES2-контекст, `paintRenderingResultsToCanvas`).

- [ ] **Step 1: Тест**

В шаг `smoke`:

```powershell
        if ($log -notmatch 'smoke: .*webgl WebGL 1\.0') { throw 'no WebGL' }
        if ($log -notmatch 'webgl: EGL 1\.\d+') { throw 'no EGL' }
```

- [ ] **Step 2: Сборка ANGLE и линковка**

`libEGL.dll` и `libGLESv2.dll` из сборки должны лежать рядом с `wkhost.exe`. В `Tools/Symbian/host/CMakeLists.txt`:

```cmake
add_custom_command(TARGET wkhost POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:EGL> $<TARGET_FILE:GLESv2> $<TARGET_FILE_DIR:wkhost>)
```

Имена целей ANGLE (`EGL`, `GLESv2`) сверить с `git show HEAD:Source/ThirdParty/ANGLE/CMakeLists.txt | grep -n "add_library"`.

На раннере нет GPU. ANGLE D3D11 сам переходит на WARP, если аппаратного устройства нет. Если `eglInitialize` всё же падает, задать при старте хоста переменную окружения `ANGLE_DEFAULT_PLATFORM=warp`: проверить её в `git show HEAD:Source/ThirdParty/ANGLE/src/libANGLE/renderer/d3d/DisplayD3D.cpp`.

- [ ] **Step 3: Проверка и коммит**

Коммит: `"[SymbianHost] WebGL through ANGLE's EGL and GLES2, with the phone's WebGL code"`. Ожидается: `smoke` зелёный, в логе `webgl: EGL 1.4, Google Inc.` (или `ANGLE`), `smoke: ... webgl WebGL 1.0 ...`. Job `build` зелёный.

---

### Task 10: Web Inspector

**Files:**
- Create: `Source/WebKitLegacy/symbian/SymbianInspectorClient.h`, `SymbianInspectorClient.cpp`
- Modify: `Source/WebKitLegacy/PlatformSymbian.cmake` (новый файл в `WebKitLegacy_SOURCES`)
- Modify: `Source/WebKitLegacy/symbian/WebKitSymbian.h` (колбэк открытия окна инспектора)
- Modify: `Source/WebKitLegacy/symbian/WebViewImpl.cpp` (`configuration.inspectorClient`, `WebView::setInspectorHost`)
- Modify: `Tools/Symbian/host/HostWindow.*`, `main.cpp` (`--inspect`, второе окно)
- Modify: `ci-windows.ps1` (шаг `inspector`), `package` (ресурсы WebInspectorUI)

**Interfaces:**
- Produces в `WebKitSymbian.h`:

```cpp
// Web Inspector. The application that sets a host can open the inspector for
// a view; the inspector's own page shows in a second view, which the host
// creates in a window of its own. Without a host (the phone) there is none.
class InspectorHost {
public:
    // A window for the inspector's page: its client, and the size of its view.
    virtual WebViewClient* openInspectorWindow(int& width, int& height) = 0;
    // The inspector's view was created in that window; it is the host's to show.
    virtual void inspectorViewCreated(WebView&) = 0;
    virtual void closeInspectorWindow() = 0;

protected:
    ~InspectorHost() = default;
};
```

В классе `WebView`:

```cpp
    // The inspector's files (WebInspectorUI's Main.html and the rest),
    // UTF-8, '/' separators.
    static void setInspectorHost(InspectorHost*, const char* resourcesDirectory);
    virtual void showInspector() = 0;
```

- [ ] **Step 1: Тест**

Шаг `inspector`: `wkhost.exe smoke.html --inspect --data $data --shot $Work\inspector.bmp --wait 30`. Затем проверить в `log.txt` строку `inspector: frontend loaded`: её пишет `SymbianInspectorFrontendClient::frontendLoaded()`. Пока клиента нет, шаг падает.

```powershell
    'inspector' {
        $exe = Get-ChildItem "$Work\build" -Recurse -Filter wkhost.exe | Select-Object -First 1
        $data = "$Work\inspector-data"
        Remove-Item -Recurse -Force $data -ErrorAction SilentlyContinue
        $page = 'file:///' + ("$PSScriptRoot\tests\smoke.html" -replace '\\', '/')
        Invoke-Native $exe.FullName @($page, '--inspect', '--data', $data, '--shot', "$Work\inspector.bmp", '--wait', '30')
        $log = Get-Content "$data\log.txt" -Raw
        $log
        if ($log -notmatch 'inspector: frontend loaded') { throw 'the inspector did not load' }
    }
```

- [ ] **Step 2: Клиенты инспектора**

Образец — WebKitLegacy для Windows: `git show HEAD:Source/WebKitLegacy/win/WebCoreSupport/WebInspectorClient.cpp` и `.h`. Перенести оттуда схему, заменив Win-окна на `InspectorHost`:

- `SymbianInspectorClient : public WebCore::InspectorClient, public Inspector::FrontendChannel`:
  - `openLocalFrontend(InspectorController*)` вызывает `InspectorHost::openInspectorWindow`, создаёт `WebViewImpl` фронтенда с этим клиентом и ставит ему `SymbianInspectorFrontendClient`: `frontendPage->inspectorController().setInspectorFrontendClient(...)`. Затем грузит `file:///<resources>/Main.html`, вызывает `inspectorViewCreated` и возвращает `this` как канал.
  - `sendMessageToFrontend(const String&)` — `m_frontendClient->frontendAPIDispatcher().dispatchMessageAsync(message)`, как в Win-образце (сверить имя метода с `git show HEAD:Source/WebCore/inspector/InspectorFrontendAPIDispatcher.h`).
  - `connectionType()` — `ConnectionType::Local`.
  - `bringFrontendToFront()`, `highlight()`, `hideHighlight()` — пустые.
  - `inspectedPageDestroyed()` — закрыть фронтенд и удалить себя.
- `SymbianInspectorFrontendClient : public WebCore::InspectorFrontendClientLocal`:
  - конструктор `(InspectorController* inspected, Page* frontendPage, SymbianInspectorClient&)` передаёт в базу `std::make_unique<InspectorFrontendClientLocal::Settings>()`;
  - `localizedStringsURL()` — `file:///<resources>/Localizations/en.lproj/localizedStrings.js`;
  - `frontendLoaded()` пишет `WTFLogAlways("inspector: frontend loaded")` и вызывает базовую;
  - `bringToFront()`, `closeWindow()` → `InspectorHost::closeInspectorWindow()`;
  - `attachWindow`, `detachWindow`, `setAttachedWindowHeight`, `setAttachedWindowWidth`, `setSheetRect`, `inspectedURLChanged`, `showCertificate` — пустые: окно всегда отдельное.

Компилятор сам покажет оставшиеся чистые виртуальные методы. Для каждого брать поведение из Win-образца, без окон.

В `WebViewImpl.cpp`: `configuration.inspectorClient = new SymbianInspectorClient(*this)` только если хост задан. Без хоста оставить то, что там сейчас. Сверить, что там: `grep -n inspectorClient Source/WebKitLegacy/symbian/WebViewImpl.cpp`. Также задать `settings.setDeveloperExtrasEnabled(hostIsSet)`.

`showInspector()` — `m_page->inspectorController().show()`.

- [ ] **Step 3: Хост**

`InspectorWindow` — копия схемы `HostWindow`: тот же класс окна, своя пара клиент/вид, размер 900x700, без поля адреса. В `main.cpp` по `--inspect` после `load()` вызвать `WebView::setInspectorHost(&inspectorHost, resources)` и `view.showInspector()`. Ресурсы — каталог `inspector` рядом с exe. Хосту нужен доступ к виду, поэтому в `HostWindow` добавить `WebKitSymbian::WebView& view() { return *m_view; }`.

`setInspectorHost` вызвать до создания первого `WebView`: клиент инспектора ставится в `PageConfiguration` при создании страницы. Поэтому в `main.cpp` порядок такой: `initializeProcess` → `setInspectorHost` (если `--inspect`) → `HostWindow`.

Ресурсы WebInspectorUI собирает цель `WebInspectorUI` при `ENABLE_WEBINSPECTORUI ON` (Task 3). Узнать, куда она их кладёт: `git show HEAD:Source/WebInspectorUI/CMakeLists.txt | grep -n "DESTINATION\|OUTPUT"`. Затем скопировать в `$<TARGET_FILE_DIR:wkhost>/inspector` командой `POST_BUILD`.

- [ ] **Step 4: Проверка и коммит**

Коммит: `"[Symbian] Web Inspector for hosts that open a window for it"`. Ожидается: шаг `inspector` зелёный. Job `build` зелёный, на телефоне хост не задан, поведение прежнее.

---

### Task 11: Артефакт

**Files:**
- Modify: `ci-windows.ps1` (шаг `package`), workflow (upload-artifact)

- [ ] **Step 1: Шаг package**

```powershell
    'package' {
        $exe = Get-ChildItem "$Work\build" -Recurse -Filter wkhost.exe | Select-Object -First 1
        $out = "$Work\package\wkhost"
        Remove-Item -Recurse -Force "$Work\package" -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force $out | Out-Null
        Copy-Item "$($exe.DirectoryName)\*" $out -Recurse
        Copy-Item "$Work\vcpkg_installed\x86-windows\bin\*.dll" $out
        Copy-Item "$Work\cacert.pem", "$PSScriptRoot\README.txt" $out
        Get-ChildItem $out -Recurse -Include '*.ttf', '*.otf' | ForEach-Object { throw "font in the package: $_" }
    }
```

Workflow:

```yaml
      - name: Package
        run: Tools/Symbian/host/ci-windows.ps1 -Step package
      - uses: actions/upload-artifact@v4
        with:
          name: wkhost
          path: WebKitBuild/SymbianHost/package
```

ICU-данные vcpkg может положить отдельным файлом `icudt*.dat`, и тогда `bin\*.dll` его не захватит. Проверить командой `Get-ChildItem $Work\vcpkg_installed -Recurse -Filter 'icudt*'`; если файл есть, добавить в шаг `package` его копирование рядом с exe.

- [ ] **Step 2: Проверка и коммит**

Коммит: `"Package wkhost as a CI artifact"`. Ожидается: артефакт `wkhost` в запуске, в нём `wkhost.exe`, DLL, `inspector\Main.html`, `cacert.pem`, `README.txt`, без `.ttf`.

- [ ] **Step 3: Приёмка у пользователя (этапы 1–4 спека)**

Пользователь скачивает `wkhost`, кладёт шрифты N8 в `%LOCALAPPDATA%\WebKitQt\fonts` и проверяет:
1. duckduckgo.com/lite и google.com открываются, в `log.txt` формат как на телефоне;
2. `fonts: 19`;
3. 4chan без ошибок `localStorage`, Google-вход переживает перезапуск, get.webgl.org показывает куб;
4. `wkhost.exe https://www.ozon.ru/ --inspect`: в инспекторе видны запрос `/abt/result` и консоль.

Этапы 5–6 спека (поиск ошибок движка на челлендже Ozon и перенос на телефон) — следующий план. Он пишется по тому, что покажет инспектор.

---

## Вне этого плана

- Выравнивание версий cairo/harfbuzz/fontconfig/ICU с телефонными (1.16.0 / 2.7.4 / 2.13.94 / 63.2). Нужно, если метрики текста на стенде и телефоне разойдутся.
- EKA2L1 как второй стенд (см. спек).
- Этапы 5–6 спека.
