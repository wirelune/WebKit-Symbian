# wkhost.exe, the Symbian port's Windows test host (Source/CMakeLists.txt adds
# this directory for PORT=SymbianHost): a window around the phone's WebView.
add_executable(wkhost WIN32
    HostCrash.cpp
    HostLog.cpp
    HostSettings.cpp
    HostWindow.cpp
    main.cpp
)
# A map file names the addresses in crash.dmp without a debugger. It has to be
# an explicit path: LINK_FLAGS is not a place where generator expressions are
# evaluated, and the linker cannot create a file whose name contains them.
set_target_properties(wkhost PROPERTIES LINK_FLAGS "/ENTRY:wmainCRTStartup")
target_link_options(wkhost PRIVATE "/MAP:${CMAKE_BINARY_DIR}/bin32/wkhost.map")
target_include_directories(wkhost PRIVATE "${WEBKITLEGACY_DIR}/symbian")
target_link_libraries(wkhost PRIVATE WebKitLegacy shell32 ole32 dbghelp)
