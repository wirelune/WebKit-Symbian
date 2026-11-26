# wkhost.exe, the Symbian port's Windows test host (Source/CMakeLists.txt adds
# this directory for PORT=SymbianHost): a window around the phone's WebView.
add_executable(wkhost WIN32
    HostLog.cpp
    HostSettings.cpp
    HostWindow.cpp
    main.cpp
)
set_target_properties(wkhost PROPERTIES LINK_FLAGS "/ENTRY:wmainCRTStartup")
target_include_directories(wkhost PRIVATE "${WEBKITLEGACY_DIR}/symbian")
target_link_libraries(wkhost PRIVATE WebKitLegacy shell32 ole32)
