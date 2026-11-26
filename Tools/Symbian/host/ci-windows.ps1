# The Windows test host's CI, one step per call: ci-windows.ps1 -Step NAME.
# See Tools/Symbian/docs/WindowsHost-design.md and WindowsHost-plan.md.
param([Parameter(Mandatory)][string]$Step)
$ErrorActionPreference = 'Stop'
$Repo = (Resolve-Path "$PSScriptRoot\..\..\..").Path
$Work = if ($env:HOST_WORK) { $env:HOST_WORK } else { "$Repo\WebKitBuild\SymbianHost" }
$Vcpkg = "$Work\vcpkg_installed\x86-windows"
$Net = "$Work\net\install"
New-Item -ItemType Directory -Force $Work | Out-Null

function Invoke-Native([string]$exe, [string[]]$arguments) {
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "$exe failed with $LASTEXITCODE" }
}

# The phone's CA bundle (scripts/package-sis.sh), next to wkhost.exe.
$CACert = 'cacert-2026-09-25.pem'
$CACertSha256 = 'a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505'

switch ($Step) {
    'inventory' {
        Invoke-Native 'bash' @("$Repo/Tools/Symbian/host/list-conditionals.sh")
        & cl.exe 2>&1 | Select-Object -First 1
    }
    'deps' {
        if (-not (Test-Path "$Work\cacert.pem")) {
            Invoke-WebRequest "https://curl.se/ca/$CACert" -OutFile "$Work\cacert.pem"
            if ((Get-FileHash "$Work\cacert.pem" -Algorithm SHA256).Hash -ne $CACertSha256) { throw 'cacert.pem checksum' }
        }
        & git -C $env:VCPKG_INSTALLATION_ROOT log -1 --format=vcpkg-%H-%cd
        if (-not (Test-Path "$Vcpkg\include\cairo")) {
            Invoke-Native "$env:VCPKG_INSTALLATION_ROOT\vcpkg.exe" @('install', '--triplet', 'x86-windows',
                "--x-manifest-root=$PSScriptRoot", "--x-install-root=$Work\vcpkg_installed", '--clean-after-build')
        }
        if (-not (Test-Path "$Net\lib\mbedtls_fs.lib")) {
            # mbedTLS's configuration from the phone's SDK: only its headers are needed.
            $sdk = "$Work\sdk"
            if (-not (Test-Path "$sdk\mbedtls\mbedtls_config.h")) {
                New-Item -ItemType Directory -Force $sdk | Out-Null
                Invoke-Native 'gh' @('release', 'download', 'v0.3.0', '--repo', 'hpnkv/symbian-platform',
                    '--pattern', 'symbian-sdk-0.3.0-linux-x86_64.tar.gz', '--dir', $sdk, '--clobber')
                Invoke-Native "$env:SystemRoot\System32\tar.exe" @('-xzf', "$sdk\symbian-sdk-0.3.0-linux-x86_64.tar.gz",
                    '-C', $sdk, '*lib/armv6/mbedtls/include/mbedtls/mbedtls_config.h',
                    '*lib/armv6/mbedtls/include/symbian_mbedtls/*')
                $headers = Get-ChildItem $sdk -Recurse -Filter mbedtls_config.h | Select-Object -First 1
                if (-not $headers) { throw 'mbedtls_config.h is not in the SDK' }
                Copy-Item $headers.DirectoryName $sdk -Recurse -Force
                Copy-Item (Join-Path $headers.Directory.Parent.FullName 'symbian_mbedtls') $sdk -Recurse -Force
                Remove-Item "$sdk\symbian-sdk-0.3.0-linux-x86_64.tar.gz"
            }
            & "$PSScriptRoot\build-net.ps1" -Work $Work -SdkHeaders $sdk -Prefix $Vcpkg
        }
    }
    'test-net' {
        $nghttp2 = (Get-ChildItem "$Net\lib" -Filter 'nghttp2*.lib' | Select-Object -First 1).FullName
        $curl = (Get-ChildItem "$Net\lib" -Filter 'libcurl*.lib' | Select-Object -First 1).FullName
        Invoke-Native 'cl' @('/nologo', '/MD', "/I$Net\include", '/DCURL_STATICLIB',
            "$PSScriptRoot\tests\curl-tls13.c", "/Fe$Work\curl-tls13.exe", "/Fo$Work\curl-tls13.obj", '/link',
            $curl, $nghttp2, "$Net\lib\mbedtls.lib", "$Net\lib\mbedx509.lib", "$Net\lib\mbedcrypto.lib",
            "$Net\lib\mbedtls_fs.lib", "$Vcpkg\lib\zlib.lib", 'ws2_32.lib', 'crypt32.lib', 'advapi32.lib',
            'bcrypt.lib', 'wldap32.lib', 'normaliz.lib')
        $env:PATH = "$Vcpkg\bin;$env:PATH"
        Invoke-Native "$Work\curl-tls13.exe" @('https://www.google.com/', "$Work\cacert.pem")
    }
    'configure' {
        Invoke-Native 'cmake' @('-S', $Repo, '-B', "$Work\build", '-G', 'Ninja',
            '-DPORT=SymbianHost', '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
            '-DCMAKE_C_COMPILER=cl', '-DCMAKE_CXX_COMPILER=cl',
            "-DCMAKE_PREFIX_PATH=$Net;$Vcpkg", '-DDEVELOPER_MODE=OFF')
    }
    default { throw "unknown step $Step" }
}
