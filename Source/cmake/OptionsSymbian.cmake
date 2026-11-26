# Symbian^3 (S60 5.2, ARM11, EKA2) with the symbian-platform SDK: WebCore on
# cairo's image surface with FreeType and HarfBuzz text, curl networking with
# mbedTLS, and the ARMv6 JIT. Everything is static; images are linked by the
# SDK's E32 flow.
find_package(Threads REQUIRED)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

include(OptionsSymbianCommon)

set(ENABLE_WEBINSPECTORUI OFF)

# The build is for size (-Os, scripts/common.sh); JavaScriptCore and WTF,
# where scripts spend their time, are built for speed, and so are WebCore's
# style, layout and DOM (PlatformSymbian.cmake there). The flag comes after
# the build type's, so it wins.
set(SYMBIAN_SPEED_FLAGS "-O2" CACHE STRING "Flags for the parts of the engine built for speed")
add_compile_options("$<$<OR:$<STREQUAL:$<TARGET_PROPERTY:NAME>,JavaScriptCore>,$<STREQUAL:$<TARGET_PROPERTY:NAME>,WTF>>:${SYMBIAN_SPEED_FLAGS}>")

set(bmalloc_LIBRARY_TYPE STATIC)
set(WTF_LIBRARY_TYPE STATIC)
set(JavaScriptCore_LIBRARY_TYPE STATIC)
set(PAL_LIBRARY_TYPE STATIC)
set(WebCore_LIBRARY_TYPE STATIC)
set(WebKitLegacy_LIBRARY_TYPE STATIC)

if (ENABLE_WEBGL)
    # The system's OpenGL ES 2.0 and EGL (libGLESv2.dll and libEGL.dll, linked
    # by the engine), with ANGLE only translating shaders to ESSL.
    SET_AND_EXPOSE_TO_BUILD(USE_OPENGL_ES TRUE)
    SET_AND_EXPOSE_TO_BUILD(USE_OPENGL FALSE)
    set(USE_ANGLE_WEBGL OFF)
    set(USE_ANGLE_EGL OFF)
    # The standard Khronos headers that come with ANGLE. The SDK's include
    # Symbian's e32def.h, whose macros clash with WTF's; the functions and
    # the sizes of the types (int handles there, pointers here) are the same.
    # On __SYMBIAN32__ the headers mark functions IMPORT_C, which the .dso
    # import libraries make unnecessary.
    set(_khronos "${CMAKE_BINARY_DIR}/SymbianGL/include")
    foreach (_dir EGL GLES2 GLES3 KHR)
        file(COPY "${CMAKE_SOURCE_DIR}/Source/ThirdParty/ANGLE/include/${_dir}" DESTINATION "${_khronos}")
    endforeach ()
    set(OPENGLES2_INCLUDE_DIRS "${_khronos}")
    set(OPENGLES2_LIBRARIES "")
    set(OPENGLES2_DEFINITIONS -DIMPORT_C=)
endif ()

# TLS and digests: the SDK's mbedTLS (static libraries).
set(MBEDTLS_INCLUDE_DIR "${SYMBIAN_SDK_PREFIX}/lib/armv6/mbedtls/include")
set(MBEDTLS_LIBRARY "${SYMBIAN_SDK_PREFIX}/lib/armv6/mbedtls/lib/libmbedtls.a")
set(MBEDX509_LIBRARY "${SYMBIAN_SDK_PREFIX}/lib/armv6/mbedtls/lib/libmbedx509.a")
set(MBEDCRYPTO_LIBRARY "${SYMBIAN_SDK_PREFIX}/lib/armv6/mbedtls/lib/libmbedcrypto.a")
# The definitions the libraries were built with: they change the layout of
# mbedTLS's structures (see Tools/Symbian/shims/mbedtls-config.h).
add_definitions(-DMBEDTLS_STATIC "-DMBEDTLS_USER_CONFIG_FILE=\"symbian_mbedtls/config.h\""
    -DMBEDTLS_SSL_PROTO_TLS1_3 -DSYMBIAN_MBEDTLS_GUEST=1)
include_directories(SYSTEM "${SYMBIAN_SDK_PREFIX}/include/abseil")
