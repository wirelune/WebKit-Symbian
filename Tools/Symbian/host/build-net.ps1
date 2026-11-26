# The Windows test host's network libraries, as on the phone: mbedTLS 3.4.1
# with the phone's configuration (mbedtls_config.h and symbian_mbedtls/ from
# the symbian-platform SDK, then mbedtls-host.h), the port's mbedTLS file
# functions (shims/mbedtls-fs.c), nghttp2 1.52.0, and curl 7.88.1 with the
# port's patches (scripts/patch-curl.sh). Installs into WORK\net\install.
#
#   build-net.ps1 -Work DIR -SdkHeaders DIR -Prefix VCPKG_INSTALLED_TRIPLET_DIR
param(
    [Parameter(Mandatory)][string]$Work,
    [Parameter(Mandatory)][string]$SdkHeaders,
    [Parameter(Mandatory)][string]$Prefix
)
$ErrorActionPreference = 'Stop'
$Symbian = (Resolve-Path "$PSScriptRoot\..").Path
$src = "$Work\net\src"
$out = "$Work\net\install"
New-Item -ItemType Directory -Force $src, "$out\include", "$out\lib" | Out-Null

function Invoke-Native([string]$exe, [string[]]$arguments) {
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "$exe $($arguments -join ' ') failed with $LASTEXITCODE" }
}
function Get-Source([string]$url, [string]$dir) {
    $archive = "$src\$(Split-Path $url -Leaf)"
    Write-Host "== $dir"
    if (-not (Test-Path $archive)) {
        Invoke-Native 'curl.exe' @('--fail', '--location', '--retry', '3', '--max-time', '600', '--silent',
            '--show-error', '--output', $archive, $url)
    }
    if (-not (Test-Path "$src\$dir")) { Invoke-Native "$env:SystemRoot\System32\tar.exe" @('-xf', $archive, '-C', $src) }
}
function Build-CMake([string]$dir, [string[]]$options) {
    Invoke-Native 'cmake' (@('-S', "$src\$dir", '-B', "$src\$dir\build", '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_INSTALL_PREFIX=$out", "-DCMAKE_PREFIX_PATH=$out;$Prefix",
        '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL') + $options)
    Invoke-Native 'cmake' @('--build', "$src\$dir\build", '--target', 'install')
}

# mbedTLS: the phone's configuration, then the host's few differences.
Get-Source 'https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v3.4.1.tar.gz' 'mbedtls-3.4.1'
$mbedInclude = "$src\mbedtls-3.4.1\include"
Copy-Item "$SdkHeaders\mbedtls\mbedtls_config.h" "$mbedInclude\mbedtls\mbedtls_config.h" -Force
Add-Content "$mbedInclude\mbedtls\mbedtls_config.h" "`n#include `"mbedtls-host.h`""
Copy-Item "$SdkHeaders\symbian_mbedtls" $mbedInclude -Recurse -Force
Copy-Item "$PSScriptRoot\mbedtls-host.h" $mbedInclude -Force
Build-CMake 'mbedtls-3.4.1' @('-DENABLE_PROGRAMS=OFF', '-DENABLE_TESTING=OFF', '-DUSE_STATIC_MBEDTLS_LIBRARY=ON',
    '-DUSE_SHARED_MBEDTLS_LIBRARY=OFF', '-DMBEDTLS_FATAL_WARNINGS=OFF')
Copy-Item "$mbedInclude\symbian_mbedtls" "$out\include" -Recurse -Force
Copy-Item "$mbedInclude\mbedtls-host.h" "$out\include" -Force

Write-Host '== mbedtls_fs'
# The file functions the phone's mbedTLS lacks; curl reads its CA bundle through them.
Copy-Item "$Symbian\shims\mbedtls-fs.h" "$out\include" -Force
Invoke-Native 'cl' @('/nologo', '/c', '/O2', '/MD', "/I$out\include", "$Symbian\shims\mbedtls-fs.c", "/Fo$src\mbedtls-fs.obj")
Invoke-Native 'lib' @('/nologo', "/OUT:$out\lib\mbedtls_fs.lib", "$src\mbedtls-fs.obj")

Get-Source 'https://github.com/nghttp2/nghttp2/releases/download/v1.52.0/nghttp2-1.52.0.tar.gz' 'nghttp2-1.52.0'
Build-CMake 'nghttp2-1.52.0' @('-DENABLE_LIB_ONLY=ON', '-DENABLE_STATIC_LIB=ON', '-DENABLE_SHARED_LIB=OFF',
    '-DENABLE_DOC=OFF', '-DENABLE_EXAMPLES=OFF', '-DENABLE_HPACK_TOOLS=OFF', '-DWITH_LIBXML2=OFF', '-DWITH_JEMALLOC=OFF')
$nghttp2 = Get-ChildItem "$out\lib" -Filter 'nghttp2*.lib' | Select-Object -First 1
if (-not $nghttp2) { throw 'nghttp2 did not install a library' }

Get-Source 'https://curl.se/download/curl-7.88.1.tar.gz' 'curl-7.88.1'
Invoke-Native 'bash' @("$Symbian/scripts/patch-curl.sh", ("$src\curl-7.88.1" -replace '\\', '/'))
$cflags = "/DWIN32 /D_WINDOWS /DNGHTTP2_STATICLIB /DSYMBIAN_MBEDTLS_FS /I$($out -replace '\\', '/')/include"
Build-CMake 'curl-7.88.1' @('-DBUILD_CURL_EXE=OFF', '-DBUILD_TESTING=OFF', '-DBUILD_SHARED_LIBS=OFF',
    "-DCMAKE_C_FLAGS=$cflags", '-DCURL_USE_SCHANNEL=OFF', '-DCURL_USE_OPENSSL=OFF', '-DCURL_USE_MBEDTLS=ON',
    '-DUSE_NGHTTP2=ON', '-DENABLE_THREADED_RESOLVER=ON', '-DCURL_DISABLE_SOCKETPAIR=ON','-DENABLE_IPV6=OFF', '-DCURL_ZLIB=ON',
    '-DCURL_DISABLE_DICT=ON', '-DCURL_DISABLE_FTP=ON', '-DCURL_DISABLE_GOPHER=ON', '-DCURL_DISABLE_IMAP=ON',
    '-DCURL_DISABLE_LDAP=ON', '-DCURL_DISABLE_LDAPS=ON', '-DCURL_DISABLE_MQTT=ON', '-DCURL_DISABLE_POP3=ON',
    '-DCURL_DISABLE_RTSP=ON', '-DCURL_DISABLE_SMB=ON', '-DCURL_DISABLE_SMTP=ON', '-DCURL_DISABLE_TELNET=ON',
    '-DCURL_DISABLE_TFTP=ON', '-DCURL_USE_LIBSSH2=OFF', '-DUSE_LIBIDN2=OFF', '-DCURL_BROTLI=OFF',
    '-DCURL_ZSTD=OFF', '-DCURL_CA_BUNDLE=none', '-DCURL_CA_PATH=none',
    "-DMBEDTLS_INCLUDE_DIRS=$out\include", "-DMBEDTLS_LIBRARY=$out\lib\mbedtls.lib",
    "-DMBEDX509_LIBRARY=$out\lib\mbedx509.lib", "-DMBEDCRYPTO_LIBRARY=$out\lib\mbedcrypto.lib",
    "-DNGHTTP2_INCLUDE_DIR=$out\include", "-DNGHTTP2_LIBRARY=$($nghttp2.FullName)")
Get-ChildItem "$out\lib" -Filter '*.lib' | ForEach-Object { $_.Name }
