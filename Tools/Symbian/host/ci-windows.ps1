# The Windows test host's CI, one step per call: ci-windows.ps1 -Step NAME.
# See Tools/Symbian/docs/WindowsHost-design.md.
param([Parameter(Mandatory)][string]$Step)
$ErrorActionPreference = 'Stop'
$Repo = (Resolve-Path "$PSScriptRoot\..\..\..").Path
$Work = if ($env:HOST_WORK) { $env:HOST_WORK } else { "$Repo\WebKitBuild\SymbianHost" }
New-Item -ItemType Directory -Force $Work | Out-Null

function Invoke-Native([string]$exe, [string[]]$arguments) {
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "$exe failed with $LASTEXITCODE" }
}

switch ($Step) {
    'inventory' {
        Invoke-Native 'bash' @("$Repo/Tools/Symbian/host/list-conditionals.sh")
        & cl.exe 2>&1 | Select-Object -First 1
    }
    default { throw "unknown step $Step" }
}
