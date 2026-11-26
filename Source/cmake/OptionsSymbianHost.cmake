# The Symbian port's Windows test host: the phone's engine (OptionsSymbianCommon)
# on 32-bit x86 Windows, for debugging pages with Web Inspector. WebKit 2.32 has
# no JIT on 32-bit x86 Windows, so JavaScript runs in the LLInt; values are
# JSVALUE32_64 as on the phone. See Tools/Symbian/docs/WindowsHost-design.md.
set(SYMBIAN_ENABLE_JIT OFF)
set(SYMBIAN_ENABLE_DFG_JIT OFF)
find_package(Threads REQUIRED)
include(OptionsSymbianCommon)

# As OptionsWin.cmake: Windows 7 and later, Unicode.
add_definitions(-DBUILDING_SYMBIAN_HOST__ -D_WINDOWS -DWINVER=0x601 -D_WIN32_WINNT=0x601
    -DNOMINMAX -DUNICODE -D_UNICODE -D_WINSOCKAPI_= -D_CRT_SECURE_NO_WARNINGS)
if (MSVC)
    include(OptionsMSVC)
endif ()

set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB32_PATHS ON)
set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS OFF)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib32)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib32)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin32)
foreach (_config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_config} "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_config} "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_config} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endforeach ()

set(ENABLE_WEBINSPECTORUI ON)
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
    set(OPENGLES2_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/Source/ThirdParty/ANGLE/include")
    set(OPENGLES2_LIBRARIES GLESv2 EGL)
    set(OPENGLES2_DEFINITIONS "")
endif ()

# TLS: mbedTLS 3.4.1 with the phone's configuration (Tools/Symbian/host/build-net.ps1).
find_path(MBEDTLS_INCLUDE_DIR mbedtls/ssl.h REQUIRED)
find_library(MBEDTLS_LIBRARY mbedtls REQUIRED)
find_library(MBEDX509_LIBRARY mbedx509 REQUIRED)
find_library(MBEDCRYPTO_LIBRARY mbedcrypto REQUIRED)
find_library(MBEDTLS_FS_LIBRARY mbedtls_fs REQUIRED)
list(APPEND MBEDCRYPTO_LIBRARY ${MBEDTLS_FS_LIBRARY} bcrypt)

# The network libraries are static (build-net.ps1).
add_definitions(-DCURL_STATICLIB -DNGHTTP2_STATICLIB)
