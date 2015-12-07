@echo off

set lpngurl=https://github.com/glennrp/libpng/archive/libpng-1.6.20-master-signed.zip
set lpngzip=libpng-1.6.20-master-signed.zip
set lpngdir=libpng-libpng-1.6.20-master-signed

set zliburl=http://sourceforge.net/projects/libpng/files/zlib/1.2.8/zlib128.zip/download
set zlibzip=zlib128.zip
set zlibdir=zlib-1.2.8

set sdl2url=https://www.libsdl.org/release/SDL2-devel-2.0.3-VC.zip
set sdl2zip=SDL2-devel-2.0.3-VC.zip
set sdl2dir=SDL2-2.0.3


if not defined VISUALSTUDIOVERSION (
	CALL "%VS140COMNTOOLS%..\..\VC\vcvarsall.bat" %1
)
if not defined VISUALSTUDIOVERSION (
	echo need VS2015
	goto end
)
if not defined PLATFORM set PLATFORM=x86

cd /d %~dp0

if not exist "%lpngzip%" ( 
	echo download %lpngzip%
	PowerShell "(New-Object System.Net.WebClient).DownloadFile('%lpngurl%', '%lpngzip%')"
)
if not exist "%lpngdir%\\" ( 
	echo unzip %lpngzip%
	PowerShell "(New-Object -ComObject shell.application).NameSpace('%CD%').CopyHere((New-Object -ComObject shell.application).NameSpace('%CD%\%lpngzip%').Items())"
)

if not exist "%zlibzip%" ( 
	echo download %zlibzip%
	PowerShell "(New-Object System.Net.WebClient).DownloadFile('%zliburl%', '%zlibzip%')"
)
if not exist "%zlibdir%\\" ( 
	echo unzip %zlibzip%
	PowerShell "$s=New-Object -Com shell.application;$s.NameSpace('%CD%').CopyHere($s.NameSpace('%CD%\%zlibzip%').Items())"
)

if not exist "%sdl2zip%" ( 
	echo download %sdl2zip%
	PowerShell "(New-Object System.Net.WebClient).DownloadFile('%sdl2url%', '%sdl2zip%')"
)
if not exist "%sdl2dir%\\" ( 
	echo unzip %sdl2zip%
	PowerShell "$s=New-Object -Com shell.application;$s.NameSpace('%CD%').CopyHere($s.NameSpace('%CD%\%sdl2zip%').Items())"
)

nmake Rt=MD
nmake Rt=MD test
rem nmake Rt=MT flif.exe
rem debug MTd|MDd
rem nmake Rt=MDd all

:end
cmd
