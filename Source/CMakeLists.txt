# -----------------------------------------------------------------------------
# Add module directories
# -----------------------------------------------------------------------------
if (NOT USE_SYSTEM_MALLOC)
    add_subdirectory(bmalloc)
endif ()

add_subdirectory(WTF)

if (USE_CAPSTONE)
    add_subdirectory(ThirdParty/capstone)
endif ()

add_subdirectory(JavaScriptCore)

if (ENABLE_WEBGL)
    add_subdirectory(ThirdParty/ANGLE)
endif ()

if (ENABLE_API_TESTS)
    add_subdirectory(ThirdParty/gtest)
endif ()

if (USE_XDGMIME)
    add_subdirectory(ThirdParty/xdgmime)
endif ()

if (USE_LIBWEBRTC)
    add_subdirectory(ThirdParty/libwebrtc)
endif ()

if (ENABLE_WEBINSPECTORUI)
    add_subdirectory(WebInspectorUI)
endif ()

if (ENABLE_WEBCORE)
    add_subdirectory(WebCore)
endif ()

if (ENABLE_WEBKIT_LEGACY)
    add_subdirectory(WebKitLegacy)
endif ()

# The Symbian port's Windows test host (Tools/Symbian/host), around WebKitLegacy.
if (PORT STREQUAL "SymbianHost")
    add_subdirectory(${CMAKE_SOURCE_DIR}/Tools/Symbian/host ${CMAKE_BINARY_DIR}/wkhost)
endif ()

if (ENABLE_WEBKIT)
    add_subdirectory(WebKit)
endif ()

if (ENABLE_WEBDRIVER)
    add_subdirectory(WebDriver)
endif ()

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()
