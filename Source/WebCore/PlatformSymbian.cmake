include(platform/Cairo.cmake)
include(platform/Curl.cmake)
include(platform/FreeType.cmake)
include(platform/ImageDecoders.cmake)

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesSymbian.txt"
)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/crypto/mbedtls"
    "${WEBCORE_DIR}/platform/adwaita"
    "${WEBCORE_DIR}/platform/generic"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/mediacapabilities"
    "${WEBCORE_DIR}/platform/symbian"
)

# The curl backend's certificate handling is written against OpenSSL; the
# Symbian port uses curl's mbedTLS backend instead.
list(REMOVE_ITEM WebCore_SOURCES
    platform/network/curl/CurlSSLVerifier.cpp
    platform/network/curl/OpenSSLHelper.cpp
)
list(APPEND WebCore_SOURCES
    platform/network/curl/CurlSSLVerifierMbedTLS.cpp
    platform/network/curl/MbedTLSHelper.cpp
)
list(REMOVE_ITEM WebCore_LIBRARIES
    OpenSSL::SSL
)

# WebGL on the phone's OpenGL ES 2.0; the rest of the GLES path is common
# (CMakeLists.txt, platform/SourcesCairo.txt).
if (ENABLE_WEBGL)
    list(APPEND WebCore_SOURCES
        platform/symbian/GraphicsContextGLSymbian.cpp
    )
endif ()

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/themeAdwaita.css
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsAdwaita.css
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsAdwaita.js
)
set(WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES ${WEBCORE_DIR}/rendering/RenderThemeAdwaita.cpp)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    ${MBEDTLS_INCLUDE_DIR}
)

list(APPEND WebCore_LIBRARIES
    ${MBEDTLS_LIBRARY}
    ${MBEDX509_LIBRARY}
    ${MBEDCRYPTO_LIBRARY}
)

# Speed where pages spend their time: style, layout, the DOM and its
# JavaScript bindings are built for speed (SYMBIAN_SPEED_FLAGS, set in
# OptionsSymbian.cmake), the rest of WebCore for size. Unified bundles hold
# the files of one top-level directory, so a bundle's first include names its
# directory. The bundles are written while WebCore is configured; this runs
# once that is done.
function(SYMBIAN_BUILD_WEBCORE_HOT_PATHS_FOR_SPEED)
    get_target_property(_sources WebCore SOURCES)
    set(_hot "css|style|rendering|layout|dom|bindings/js")
    set(_fast "")
    foreach (_source IN LISTS _sources)
        if (_source MATCHES "UnifiedSource-[0-9a-f]+-[0-9]+\.cpp$")
            if (EXISTS "${_source}")
                file(STRINGS "${_source}" _first LIMIT_COUNT 1)
                if (_first MATCHES "^#include \"(${_hot})/")
                    list(APPEND _fast "${_source}")
                endif ()
            endif ()
        elseif (_source MATCHES "(^|/)(${_hot})/[^/]+\.cpp$")
            list(APPEND _fast "${_source}")
        endif ()
    endforeach ()
    list(LENGTH _fast _count)
    message(STATUS "Symbian: ${_count} WebCore sources built with ${SYMBIAN_SPEED_FLAGS}")
    if (_fast)
        set_source_files_properties(${_fast} PROPERTIES COMPILE_OPTIONS "${SYMBIAN_SPEED_FLAGS}")
    endif ()
endfunction()
cmake_language(DEFER CALL SYMBIAN_BUILD_WEBCORE_HOT_PATHS_FOR_SPEED)
