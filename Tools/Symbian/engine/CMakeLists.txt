# wkengine.exe: the Symbian port's WebView as a server process, for front
# ends built with another toolchain (../qtbrowser, the Qt 4.7 browser). It
# shares the AVKON browser's run loop driver, network connection, log and
# audio player, and links the same libraries.
#
#   -DWEBKIT_SOURCE_DIR=…    WebKit checkout (for WebKitSymbian.h)
#   -DWEBKIT_BUILD_DIR=…     PORT=Symbian build with WebKitLegacy
#   -DDEPS_DIR=…             scripts/build-deps.sh's install directory
#   -DICU_LIBRARY_DIR=…      cross-built ICU libraries
cmake_minimum_required(VERSION 3.28)
project(wkengine_e32 LANGUAGES C CXX ASM)
include(SymbianApp)

set(WEBKIT_SOURCE_DIR "" CACHE PATH "WebKit source checkout")
set(WEBKIT_BUILD_DIR "" CACHE PATH "PORT=Symbian build directory")
set(DEPS_DIR "" CACHE PATH "Installed WebCore dependencies")
set(ICU_LIBRARY_DIR "" CACHE PATH "Cross-built ICU libraries")
set(ENGINE_UID3 0xE1A5C003 CACHE STRING "UID3 of wkengine.exe (unprotected range)")

set(B "${CMAKE_CURRENT_SOURCE_DIR}/../browser")
set(engine_sources
  EngineServer.cpp
  "${B}/BrowserLog.cpp"
  "${B}/MediaPlayers.cpp"
  "${B}/NetworkConnection.cpp"
  "${B}/PageList.cpp"
  "${B}/RunLoopDriver.cpp")

symbian_add_executable(wkengine SOURCES ${engine_sources})

target_include_directories(wkengine PRIVATE "${B}" "${WEBKIT_SOURCE_DIR}/Source/WebKitLegacy/symbian")
target_compile_features(wkengine PRIVATE cxx_std_17)
# Leaves cross the server framework (CServer2 calls ServiceL); no typeinfo,
# as in the browser.
symbian_enable_native_leaves(wkengine SOURCES ${engine_sources})
set_property(SOURCE ${engine_sources} APPEND PROPERTY COMPILE_OPTIONS -fno-rtti)

target_link_libraries(wkengine PRIVATE SymbianNativeAbi Symbian::EUser)
target_include_directories(wkengine SYSTEM PRIVATE "${B}/include")
# The original Nokia headers use redundant member qualifications. Microsoft
# extensions make __wchar_t a keyword, which Open C's sys/_types.h declares.
target_compile_options(wkengine PRIVATE -fms-extensions -Wno-microsoft-extra-qualification)
target_compile_definitions(wkengine PRIVATE __wchar_t=__symbian_wchar_t)
# cone, ws32 and gdi only for the video player, which the engine never makes.
foreach (dll cone ws32 gdi fbscli bitgdi bafl efsrv esock mediaclientaudio mediaclientvideo)
  target_link_libraries(wkengine PRIVATE "${SYMBIAN_SDK_PREFIX}/proxies/${dll}/${dll}.dso")
endforeach ()

set(L "${WEBKIT_BUILD_DIR}/lib")
set(D "${DEPS_DIR}/lib")
set(M "${SYMBIAN_SDK_PREFIX}/lib/armv6/mbedtls/lib")
set(S "${SYMBIAN_SDK_PREFIX}/lib/armv6")
target_link_libraries(wkengine PRIVATE
  --start-group
  "${L}/libWebKitLegacy.a"
  "${L}/libWebCore.a"
  "${L}/libPAL.a"
  "${L}/libJavaScriptCore.a"
  "${L}/libWTF.a"
  "${D}/libcairo.a" "${D}/libpixman-1.a" "${D}/libharfbuzz-icu.a" "${D}/libharfbuzz.a"
  "${D}/libfontconfig.a" "${D}/libxslt.a" "${D}/libxml2.a" "${D}/libsqlite3.a" "${D}/libpsl.a"
  "${D}/libwebpdemux.a" "${D}/libwebp.a" "${D}/libcurl.a" "${D}/libmbedtls_fs.a"
  "${M}/libmbedtls.a" "${M}/libmbedx509.a" "${M}/libmbedcrypto.a"
  "${ICU_LIBRARY_DIR}/libicui18n.a" "${ICU_LIBRARY_DIR}/libicuuc.a" "${ICU_LIBRARY_DIR}/libicudata.a"
  "${S}/libsymbian_portable_freetype.a" "${S}/libsymbian_portable_png.a"
  "${S}/libsymbian_portable_jpeg.a" "${S}/libsymbian_portable_zlib.a"
  --end-group
  Symbian::Streams
  "${SYMBIAN_SDK_PREFIX}/proxies/euser/euser.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/hal/hal.dso"
  "${SYMBIAN_SDK_PREFIX}/proxies/libdl/libdl.dso")
# As for the browser: thunks for calls beyond a BL's reach.
target_link_options(wkengine PRIVATE -znocopyreloc -znotext --icf=all --pic-veneer)

# The browser's image layout, which puts the shell's code first; here the
# engine's.
file(READ "${B}/browser_image.ld" layout)
string(REPLACE "/browser.dir/" "/wkengine.dir/" layout "${layout}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/wkengine_image.ld" "${layout}")
function(engine_use_image_layout target)
  get_target_property(script ${target} SYMBIAN_IMAGE_LAYOUT)
  get_target_property(options ${target} LINK_OPTIONS)
  set(layout "${CMAKE_CURRENT_BINARY_DIR}/wkengine_image.ld")
  list(TRANSFORM options REPLACE "^${script}$" "${layout}")
  set_target_properties(${target} PROPERTIES LINK_OPTIONS "${options}"
    LINK_DEPENDS "${layout}" SYMBIAN_IMAGE_LAYOUT "${layout}")
endfunction()
cmake_language(DEFER CALL engine_use_image_layout wkengine)

# NetworkServices (bit 13) for Open C's sockets.
symbian_publish_executable(wkengine UID3 ${ENGINE_UID3} CAPABILITIES 0x2000)
