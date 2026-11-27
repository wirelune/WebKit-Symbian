WebKit Symbian test host (wkhost.exe)

The Symbian port's engine on Windows: the same WebKit code, settings and
network stack (curl 7.88.1, mbedTLS 3.4.1 with the phone's configuration,
nghttp2 1.52.0) as on the Nokia N8, in a window the size of the phone's view.
JavaScript runs in the C++ interpreter (WebKit 2.32 has neither a JIT nor an
assembly interpreter for 32-bit x86), with the phone's 32-bit values, and
WebGL goes through ANGLE instead of the phone's Broadcom driver.

Data directory: %LOCALAPPDATA%\WebKitQt (or --data DIR). It holds log.txt,
log.old.txt, cookies.db, LocalStorage and the files below, as
C:\Data\WebKitQt does on the phone. Create it before the first run and put the
phone's fonts in it:

  %LOCALAPPDATA%\WebKitQt\fonts\   everything in Z:\resource\fonts (20 files on
                                   the N8). The host sees no other fonts, so
                                   without them pages come out blank.

The files below also live in that directory:

  fonts\          the phone's fonts, as above
  useragent.txt   first line replaces the User-Agent string
  language.txt    first line is the preferred language (en-GB by default)
  curl-verbose    while it exists, curl's trace goes to curl.txt
  crash.dmp       written if the host crashes, for a debugger

Usage: wkhost.exe [URL] [--data DIR]
       wkhost.exe [URL] [--data DIR] --shot FILE.bmp [--wait SECONDS]

--shot loads the page, waits (10 seconds unless --wait says otherwise), writes
the view to the BMP file and exits with 0 when it did: this is what the CI's
smoke test runs.

The mouse acts as a finger: press, drag and release. The wheel scrolls.
Type an address in the field at the top and press Enter.
