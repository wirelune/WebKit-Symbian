# The Symbian port's Windows test host builds JavaScriptCore as the phone does.
include(${CMAKE_CURRENT_LIST_DIR}/PlatformSymbian.cmake)

list(APPEND JavaScriptCore_LIBRARIES
    winmm
)
