param($platform = "x86")

$lpngurl = "https://github.com/glennrp/libpng/archive/libpng-1.6.20-master-signed.zip"
$lpngzip = "libpng-1.6.20-master-signed.zip"
$lpngdir = "libpng-libpng-1.6.20-master-signed"

$zliburl = "http://sourceforge.net/projects/libpng/files/zlib/1.2.8/zlib128.zip/download"
$zlibzip = "zlib128.zip"
$zlibdir = "zlib-1.2.8"

$sdl2url = "https://www.libsdl.org/release/SDL2-devel-2.0.3-VC.zip"
$sdl2zip = "SDL2-devel-2.0.3-VC.zip"
$sdl2dir = "SDL2-2.0.3"

Push-Location (split-path -parent $PSCommandPath)

if (!$env:VISUALSTUDIOVERSION) {
    $vswhereurl = "https://github.com/Microsoft/vswhere/releases/download/1.0.62/vswhere.exe"
    $vswhereexe = "vswhere-1.0.62.exe"
    if (-Not (Test-Path "$vswhereexe")) {
        Write-Output "download vswhere"
        Invoke-Webrequest $vswhereurl -OutFile $vswhereexe
    }

    $path = & "./$vswhereexe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-Not $path) {
        Write-Output "need VS 2017"
        exit
    }
    $vcvarspath = Join-Path $path 'VC\Auxiliary\Build\vcvarsall.bat'
    if (-Not (Test-Path $vcvarspath)) {
        Write-Output "need MSVC 2017"
    }

    cmd /c "`"$vcvarspath`" $platform & set" | Foreach-Object {
        if ($_ -Match "=") {
            $v = $_.Split("="); Set-Item "ENV:\$($v[0])" -Value "$($v[1])"
        }
    }
}

if (-Not (Test-Path $lpngzip)) { 
    Write-Output "download $lpngzip"
    Invoke-WebRequest $lpngurl -OutFile $lpngzip
    
}
if (-Not (Test-Path $lpngdir)) {
    Write-Output "unzip $lpngzip"
    Expand-Archive $lpngzip -DestinationPath '.'
}

if (-Not (Test-Path $zlibzip)) {
    Write-Output "download $zlibzip"
    Invoke-WebRequest $zliburl -OutFile $zlibzip -UserAgent "NativeHost"
}
if (-Not (Test-Path $zlibdir)) {
    Write-Output "unzip $zlibzip"
    Expand-Archive $zlibzip -DestinationPath '.'
}

if (-Not (Test-Path $sdl2zip)) {
    Write-Output "download $sdl2zip"
    Invoke-WebRequest $sdl2url -OutFile $sdl2zip
}
if (-Not (Test-Path $sdl2dir)) {
    Write-Output "unzip $sdl2zip"
    Expand-Archive $sdl2zip -DestinationPath '.'
}

nmake Rt=MD
nmake Rt=MD test

Pop-Location

powershell
