# Build Nextbot on Windows x64 with Docker Desktop (Linux containers / WSL2).
#
#   .\build.cmd                      -> eboot.bin, sce_sys\param.sfo, pkg.gp4, pkg-orbis-pub-gen.gp4
#   .\build.cmd pkg                  -> also the fake-signed .pkg via PkgTool
#   .\build.cmd clean
#
# The image "oo-ps4-build" is loaded from ..\oo-ps4-build.tar.gz if present,
# otherwise it is built from docker\Dockerfile.
param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Targets)
$ErrorActionPreference = 'Stop'

$Project = $PSScriptRoot
$Image = 'oo-ps4-build'

# Toolchain: OO_PS4_TOOLCHAIN only if it contains the Linux tools, else ..\PS4Toolchain
$Toolchain = $null
if ($env:OO_PS4_TOOLCHAIN -and (Test-Path (Join-Path $env:OO_PS4_TOOLCHAIN 'bin\linux\create-fself'))) {
    $Toolchain = $env:OO_PS4_TOOLCHAIN
} elseif (Test-Path (Join-Path $Project '..\PS4Toolchain\bin\linux\create-fself')) {
    $Toolchain = (Resolve-Path (Join-Path $Project '..\PS4Toolchain')).Path
} else {
    throw "OpenOrbis toolchain (with bin\linux) not found: put it in ..\PS4Toolchain or set OO_PS4_TOOLCHAIN"
}

# "docker images -q" prints nothing (and no stderr) when the image is missing; redirecting
# stderr of "docker image inspect" would raise a terminating error under 'Stop' in PS 5.1.
$have = docker images -q $Image
if (-not $have) {
    $tar = Join-Path $Project '..\oo-ps4-build.tar.gz'
    if (Test-Path $tar) {
        Write-Host "Loading $Image from $tar"
        docker load -i $tar
    } else {
        Write-Host "Building $Image from docker\Dockerfile"
        docker build -t $Image (Join-Path $Project 'docker')
    }
    if ($LASTEXITCODE -ne 0) { throw "could not prepare the $Image image" }
}

docker run --rm `
    -v "${Toolchain}:/toolchain:ro" -v "${Project}:/project" -w /project `
    -e OO_PS4_TOOLCHAIN=/toolchain -e DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 `
    $Image make @Targets
exit $LASTEXITCODE
