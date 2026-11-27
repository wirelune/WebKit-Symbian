# Состояние Windows-стенда

Дата: 2026-11-28. Ветка: `2.32.4-symbian` (форк `wirelune/WebKit-Symbian`).
Замысел — [WindowsHost-design.md](WindowsHost-design.md), план — [WindowsHost-plan.md](WindowsHost-plan.md).

## Что уже работает

Job `windows-host` в Actions собирает стенд целиком: конфигурация, JavaScriptCore
(с `jsc.exe` и его тестом), WebCore (2902 объекта), PAL, WebKitLegacy,
`wkhost.exe`, smoke-тест и упаковка артефакта.

На smoke-странице (`Tools/Symbian/host/tests/smoke.html`) `wkhost.exe` сообщает
в `log.txt`:

* `fonts: 2` — только шрифты из `<данные>\fonts`, как задумано;
* `languages: en-GB, en-US` и `screen 360x640` — те же значения, что на N8;
* `localStorage v` — хранилище работает с `file://`;
* `webgl WebGL 1.0` — через ANGLE (EGL 1.5, D3D9), то есть на раннере без GPU;
* скриншот вида 360x510 сохраняется и страница на нём отрисована.

## Как запустить у себя

1. Взять артефакт `wkhost` из последнего зелёного рана job'а `windows-host`
   (Actions → Symbian → artifacts).
2. Распаковать. Рядом с `wkhost.exe` лежат нужные DLL, `cacert.pem` и `README.txt`.
3. Скопировать шрифты телефона — всё содержимое `Z:\resource\fonts` (на N8 их 20) —
   в папку `fonts\` рядом с `wkhost.exe`. Стенд видит **только** их; без шрифтов
   страницы будут пустыми.
4. Запустить: `wkhost.exe https://duckduckgo.com/lite/`.
   Данные (`log.txt`, `log.old.txt`, `cookies.db`, `LocalStorage`, `crash.dmp`,
   `curl.txt` при файле `curl-verbose`) лежат в `%LOCALAPPDATA%\WebKitQt` или в
   каталоге, указанном `--data DIR`.

## Известные дефекты

* **Падение при завершении.** `--shot` (режим CI) завершает процесс сразу после
  записи снимка, не разворачивая WebKit. Обычное закрытие окна идёт через
  `~HostWindow` → `~WebViewImpl` (`mainFrame().loader().detachFromParent()`) →
  `~Page` и падает с `0xC0000005`. На телефоне этот путь не выполняется: процесс
  Qt-приложения завершается, не разрушая WebView, поэтому порт его никогда не
  проходил. Разбирать по `crash.dmp` (пишется `HostCrash.cpp`) и `wkhost.map`
  (пишется при линковке); при падении smoke-теста CI кладёт их в артефакт
  `host-crash`.

## Чего ещё нет

* Web Inspector (Task 10 плана): его нет ни в слое порта, ни в хосте.
* Отдельного шага, который в CI закрывает окно штатно (чтобы ловить дефекты
  завершения, а не обходить их): нужен ключ вроде `--close SECONDS`.
* Проверка WebGL в smoke-тесте пока только предупреждение.

## Правила, которые уже сложились при починке сборки

* **Поведение порта — под `PLATFORM(SYMBIAN)`**, вызовы Symbian API — под
  `OS(SYMBIAN)`. Стенд собран на Windows, но он **не** `PLATFORM(WIN)`, поэтому
  всё, что апстрим делает «для Windows», надо проверять отдельно: так нашлись
  `MainThreadSharedTimer`, `MemoryPressureHandler`, WinCairo-овские prefix header
  и `.rc` в WebKitLegacy.
* **Платформенный файл порта нужен каждой подсистеме.** `PlatformSymbianHost.cmake`
  должен быть у WebCore, WebKitLegacy, WTF *и PAL* — у PAL его не было, и хост
  остался без CryptoDigest, KillRing, Clock и systemBeep.
* **Генераторы не должны зависеть от внешнего препроцессора.** `cl` из Perl мог
  печатать пустоту, из-за чего CSS-листы получались нулевой длины (MSVC отвергает
  такие массивы, GCC прощает). Теперь `make-css-file-arrays.pl` разбирает
  директивы сам, и телефон с хостом получают одни и те же байты.
* **Проверять, что попало в коммит.** Один ран ушёл в CI со старым шагом
  проверки генератора (файл не был добавлен в коммит) и вместо секунды собирал
  весь проект, пока его не убило по таймауту.
