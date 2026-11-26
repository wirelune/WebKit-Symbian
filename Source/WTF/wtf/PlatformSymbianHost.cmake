# The Symbian port's Windows test host: the phone's WTF (PlatformSymbian.cmake)
# with the same generic event loop, and files, threads, memory and time from
# Windows (as PlatformWin.cmake) instead of Open C.
include(${CMAKE_CURRENT_LIST_DIR}/PlatformSymbian.cmake)

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
list(REMOVE_ITEM WTF_PUBLIC_HEADERS
    symbian/SymbianSystem.h
)

list(APPEND WTF_PUBLIC_HEADERS
    text/win/WCharStringExtras.h

    win/DbgHelperWin.h
    win/GDIObject.h
    win/SoftLinking.h
    win/Win32Handle.h
)
list(APPEND WTF_SOURCES
    text/win/StringWin.cpp
    text/win/TextBreakIteratorInternalICUWin.cpp

    win/CPUTimeWin.cpp
    win/DbgHelperWin.cpp
    win/FileSystemWin.cpp
    win/LanguageWin.cpp
    win/OSAllocatorWin.cpp
    win/PathWalker.cpp
    win/ThreadingWin.cpp
)
list(APPEND WTF_LIBRARIES
    DbgHelp
    shlwapi
    winmm
)
