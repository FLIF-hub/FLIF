cd /d %~dp0..\..\

echo release
cl /nologo /DFLIF_USE_STB_IMAGE /Feflif.exe flif.cpp build\MSVC\getopt\*.c* -I build\MSVC\getopt\ /DSTATIC_GETOPT flif-dec.cpp flif-enc.cpp common.cpp io.cpp maniac\*.c* transform\*.cpp image\*.c*  /WX /EHsc /Ox /Oy /MT /DNDEBUG

echo debug
cl /nologo /DFLIF_USE_STB_IMAGE /Feflif-d.exe flif.cpp build\MSVC\getopt\*.c* -I build\MSVC\getopt\ /DSTATIC_GETOPT flif-dec.cpp flif-enc.cpp common.cpp io.cpp maniac\*.c* transform\*.cpp image\*.c*  /WX /EHsc /Zi /Oy- /MDd /DDEBUG

echo test
if exist output.flif del output.flif
flif tools/kodim01.png output.flif
if exist output.png  del output.png
flif output.flif output.png
start output.png


