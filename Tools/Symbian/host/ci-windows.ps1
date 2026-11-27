# The Windows test host's CI, one step per call: ci-windows.ps1 -Step NAME.
# See Tools/Symbian/docs/WindowsHost-design.md and WindowsHost-plan.md.
param([Parameter(Mandatory)][string]$Step)
$ErrorActionPreference = 'Stop'
$Repo = (Resolve-Path "$PSScriptRoot\..\..\..").Path
$Work = if ($env:HOST_WORK) { $env:HOST_WORK } else { "$Repo\WebKitBuild\SymbianHost" }
$Triplet = 'x86-windows-rel'
$Vcpkg = "$Work\vcpkg_installed\$Triplet"
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
    # Two steps, so that the CI caches vcpkg's long build as soon as it is done.
    'vcpkg' {
        & git -C $env:VCPKG_INSTALLATION_ROOT log -1 --format=vcpkg-%H-%cd
        if (-not (Test-Path "$Vcpkg\include\cairo")) {
            Invoke-Native "$env:VCPKG_INSTALLATION_ROOT\vcpkg.exe" @('install', '--triplet', $Triplet,
                '--host-triplet', $Triplet, "--overlay-triplets=$PSScriptRoot\triplets",
                "--x-manifest-root=$PSScriptRoot", "--x-install-root=$Work\vcpkg_installed", '--clean-after-build')
        }
    }
    'net' {
        if (-not (Test-Path "$Work\cacert.pem")) {
            Invoke-Native 'curl.exe' @('--fail', '--location', '--retry', '3', '--max-time', '300', '--silent',
                '--show-error', '--output', "$Work\cacert.pem", "https://curl.se/ca/$CACert")
            if ((Get-FileHash "$Work\cacert.pem" -Algorithm SHA256).Hash -ne $CACertSha256) { throw 'cacert.pem checksum' }
        }
        if (-not (Test-Path "$Net\lib\mbedtls_fs.lib")) {
            # mbedTLS's configuration from the phone's SDK: only its headers are needed.
            $sdk = "$Work\sdk"
            if (-not (Test-Path "$sdk\mbedtls\mbedtls_config.h")) {
                Write-Host '== mbedTLS configuration from the phone SDK'
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
        # zlib.lib, or libz.lib from vcpkg's zlib-ng.
        $zlib = (Get-ChildItem "$Vcpkg\lib\*" -Include 'zlib.lib', 'libz.lib' | Select-Object -First 1).FullName
        Invoke-Native 'cl' @('/nologo', '/MD', "/I$Net\include", '/DCURL_STATICLIB',
            "$PSScriptRoot\tests\curl-tls13.c", "/Fe$Work\curl-tls13.exe", "/Fo$Work\curl-tls13.obj", '/link',
            $curl, $nghttp2, "$Net\lib\mbedtls.lib", "$Net\lib\mbedx509.lib", "$Net\lib\mbedcrypto.lib",
            "$Net\lib\mbedtls_fs.lib", $zlib, "$Vcpkg\lib\psl.lib", 'ws2_32.lib', 'crypt32.lib', 'advapi32.lib',
            'bcrypt.lib', 'wldap32.lib', 'normaliz.lib')
        $env:PATH = "$Vcpkg\bin;$env:PATH"
        Invoke-Native "$Work\curl-tls13.exe" @('https://www.google.com/', "$Work\cacert.pem")
    }
    'configure' {
        # gperf and bison from the runner's MSYS2, by path: its usr\bin on PATH
        # would put its link.exe before MSVC's. A machine that keeps MSYS2
        # elsewhere sets HOST_MSYS, and HOST_PERL for a native Windows Perl
        # (Cygwin and MSYS Perls report a different osname to the generators).
        $msys = if ($env:HOST_MSYS) { $env:HOST_MSYS } else { 'C:\msys64\usr\bin' }
        if (-not (Test-Path "$msys\gperf.exe") -or -not (Test-Path "$msys\bison.exe")) {
            Invoke-Native "$msys\pacman.exe" @('-Sy', '--noconfirm', '--needed', 'gperf', 'bison')
        }
        $perl = if ($env:HOST_PERL) { @("-DPERL_EXECUTABLE=$($env:HOST_PERL -replace '\\', '/')") } else { @() }
        Invoke-Native 'cmake' (@('-S', $Repo, '-B', "$Work\build", '-G', 'Ninja',
            '-DPORT=SymbianHost', '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
            '-DCMAKE_C_COMPILER=cl', '-DCMAKE_CXX_COMPILER=cl',
            "-DGPERF_EXECUTABLE=$($msys -replace '\\', '/')/gperf.exe",
            "-DBISON_EXECUTABLE=$($msys -replace '\\', '/')/bison.exe") + $perl + @(
            "-DCMAKE_PREFIX_PATH=$Net;$Vcpkg", '-DDEVELOPER_MODE=OFF'))
    }
    'generators' {
        # make-css-file-arrays.pl evaluates the sheets' directives itself, so it
        # can be checked in a second: run it on real CSS files with the build's
        # own Perl and look at the arrays it wrote. Without this, a generator
        # that writes zero-length arrays shows up as C2466 in five unified
        # sources twenty minutes into the build, with nothing about the cause.
        $perl = (Select-String -Path "$Work\build\CMakeCache.txt" -Pattern '^PERL_EXECUTABLE:').Line -replace '^[^=]*=', ''
        $header = "$env:TEMP\UserAgentStyleSheets.h"
        $source = "$env:TEMP\UserAgentStyleSheets.cpp"
        Invoke-Native $perl @("$Repo\Source\WebCore\css\make-css-file-arrays.pl",
            '--defines', 'ENABLE_FULLSCREEN_API=1 ENABLE_MATHML=1 ENABLE_VIDEO=1 HAVE_OS_DARK_MODE_SUPPORT=0',
            $header, $source,
            "$Repo\Source\WebCore\css\dialog.css", "$Repo\Source\WebCore\css\fullscreen.css",
            "$Repo\Source\WebCore\css\mathml.css")
        Get-Content $header
        if ((Get-Content $header -Raw) -match '\[0\]') { throw 'the generator wrote a zero-length array' }
        $bytes = (Get-Item $source).Length
        Write-Host "the generator wrote $bytes bytes of arrays"
        if ($bytes -lt 1000) { throw "the generated arrays are only $bytes bytes" }
    }
    'build-jsc' {
        # -k 0: every error in one run, as a CI round trip is long.
        Invoke-Native 'cmake' @('--build', "$Work\build", '--target', 'jsc', '--', '-k', '0')
    }
    'test-jsc' {
        $jsc = Get-ChildItem "$Work\build" -Recurse -Filter jsc.exe | Select-Object -First 1
        $env:PATH = "$Vcpkg\bin;$env:PATH"
        $out = (& $jsc.FullName "$Repo\Tools\Symbian\tests\es2020.js" 2>&1 | Out-String) -replace "`r", ''
        $out
        $expected = (Get-Content "$PSScriptRoot\tests\expected-es2020.txt" -Raw) -replace "`r", ''
        if ($out.Trim() -ne $expected.Trim()) { throw 'es2020.js printed something else than tests/expected-es2020.txt' }
        $intl = & $jsc.FullName "$Repo\Tools\Symbian\tests\intl.js" 2>&1 | Out-String
        $intl
        if ($LASTEXITCODE -ne 0 -or $intl -match ' ERR ') { throw 'intl.js failed' }
    }
    'build' {
        Invoke-Native 'cmake' @('--build', "$Work\build", '--', '-k', '0')
    }
    'smoke' {
        $exe = Get-ChildItem "$Work\build" -Recurse -Filter wkhost.exe | Select-Object -First 1
        Copy-Item "$Work\cacert.pem" $exe.DirectoryName -Force
        $env:PATH = "$Vcpkg\bin;$env:PATH"
        $data = "$Work\smoke-data"
        Remove-Item -Recurse -Force $data -ErrorAction SilentlyContinue
        # Arial, which the N8 has too, from the runner's Windows: the host sees
        # no fonts but the ones in its data directory.
        New-Item -ItemType Directory -Force "$data\fonts" | Out-Null
        Copy-Item "$env:SystemRoot\Fonts\arial.ttf", "$env:SystemRoot\Fonts\arialbd.ttf" "$data\fonts"
        $page = 'file:///' + ("$PSScriptRoot\tests\smoke.html" -replace '\\', '/')
        $process = Start-Process $exe.FullName -ArgumentList @($page, '--data', $data, '--shot', "$Work\smoke.bmp", '--wait', '20') -PassThru -Wait
        $log = Get-Content "$data\log.txt" -Raw
        $log
        if (Test-Path "$data\crash.dmp") { Write-Host "crash dump: $data\crash.dmp" }
        if ($process.ExitCode -ne 0) { throw "wkhost exited with $($process.ExitCode)" }
        if ($log -notmatch 'console: .*smoke: localStorage v') { throw 'no localStorage' }
        # WebKit's NavigatorBase::languages() reports only the primary language
        # "for privacy reasons", so that is what the phone says too; the second
        # language still reaches servers through Accept-Language.
        if ($log -notmatch 'smoke: languages en-GB') { throw 'not the phone language' }
        if ($log -notmatch 'smoke: .*screen 360x640') { throw 'not the phone screen' }
        if ($log -notmatch 'fonts: 2') { throw 'the host must see the fonts in its data directory and no others' }
        if ($log -notmatch 'smoke: .*webgl WebGL 1\.0') { Write-Warning 'no WebGL yet' }
        Add-Type -AssemblyName System.Drawing
        $bitmap = [System.Drawing.Bitmap]::FromFile("$Work\smoke.bmp")
        $pixel = $bitmap.GetPixel(350, 500)
        $bitmap.Dispose()
        if ($pixel.G -lt 200 -or $pixel.R -gt 50) { throw "the page did not paint: $pixel" }

        # The window's own close path, which is what a user does and the one
        # path that still crashes in WebKit's teardown
        # (docs/WindowsHost-status.md). Running it here leaves crash.dmp for the
        # artifacts, but only as a warning: the stand's answer is the screenshot
        # above, not whether WebKit can be destroyed. log.txt is renamed to
        # log.old.txt by the second run, so both stay.
        $closing = Start-Process $exe.FullName -ArgumentList @($page, '--data', $data) -PassThru
        Start-Sleep -Seconds 5
        [void]$closing.CloseMainWindow()
        if (-not $closing.WaitForExit(20000)) {
            $closing.Kill()
            Write-Warning 'the host did not exit when its window was closed'
        } elseif ($closing.ExitCode -ne 0) {
            Write-Warning "closing the window exited with $($closing.ExitCode); see crash.dmp"
        } else
            Write-Host 'closing the window exited with 0'
    }
    'package' {
        # What the user runs: wkhost.exe with the DLLs it needs beside it, the
        # phone's CA bundle, and README.txt about copying the fonts.
        $exe = Get-ChildItem "$Work\build" -Recurse -Filter wkhost.exe | Select-Object -First 1
        if (-not $exe) { throw 'wkhost.exe was not built' }
        $dist = "$Work\dist\wkhost"
        Remove-Item $dist -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force $dist | Out-Null
        Copy-Item $exe.FullName $dist
        # vcpkg builds the triplet's libraries as DLLs; they are all next to each
        # other, and Windows finds them beside the executable.
        Get-ChildItem "$Vcpkg\bin\*.dll" | Copy-Item -Destination $dist
        if (Test-Path "$Net\bin") { Get-ChildItem "$Net\bin\*.dll" | Copy-Item -Destination $dist }
        # ICU, if this build keeps its data in a file; the library packaging in
        # the DLL needs nothing here.
        Get-ChildItem "$Vcpkg\bin" -Filter 'icudt*.dat' -ErrorAction SilentlyContinue |
            Select-Object -First 1 | Copy-Item -Destination $dist
        Copy-Item "$Work\cacert.pem" $dist
        Copy-Item "$PSScriptRoot\README.txt" $dist
        $zip = "$Work\dist\wkhost.zip"
        Remove-Item $zip -Force -ErrorAction SilentlyContinue
        Compress-Archive -Path "$dist\*" -DestinationPath $zip -Force
        Write-Host "packaged $((Get-ChildItem $dist | Measure-Object).Count) files"
        Write-Host "zip: $((Get-Item $zip).Length) bytes"

        # The archive has to work on its own: unpack it somewhere else and run
        # that copy with a PATH of nothing but the system directories, so a DLL
        # that was left out shows up here rather than on the user's machine.
        $check = "$Work\dist\check"
        Remove-Item $check -Recurse -Force -ErrorAction SilentlyContinue
        Expand-Archive $zip -DestinationPath $check -Force
        New-Item -ItemType Directory -Force "$check\fonts" | Out-Null
        Copy-Item "$env:SystemRoot\Fonts\arial.ttf", "$env:SystemRoot\Fonts\arialbd.ttf" "$check\fonts"
        $env:PATH = "$env:SystemRoot\system32;$env:SystemRoot"
        $page = 'file:///' + ("$PSScriptRoot\tests\smoke.html" -replace '\\', '/')
        $packaged = Start-Process "$check\wkhost.exe" -ArgumentList @($page, '--data', "$check\data",
            '--shot', "$check\check.bmp", '--wait', '10') -PassThru -Wait
        $log = Get-Content "$check\data\log.txt" -Raw
        if ($packaged.ExitCode -ne 0) { throw "the packaged wkhost exited with $($packaged.ExitCode)" }
        if ($log -notmatch 'fonts: 2') { throw 'the packaged wkhost did not read its fonts' }
        if ($log -notmatch 'smoke: languages en-GB') { throw 'the packaged wkhost did not run the page' }
        Write-Host 'the packaged copy runs'
    }
    default { throw "unknown step $Step" }
}
