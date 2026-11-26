# The Symbian port's Windows test host: the phone's WebCore (PlatformSymbian.cmake),
# with Windows' sockets and certificate store linked in; SourcesSymbianHost.txt
# has the few files that stand in for Open C ones.
include(${CMAKE_CURRENT_LIST_DIR}/PlatformSymbian.cmake)

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesSymbianHost.txt"
)
list(APPEND WebCore_LIBRARIES
    crypt32
    winmm
    ws2_32
)
