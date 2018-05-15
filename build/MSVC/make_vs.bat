echo release
cl /nologo /DFLIF_USE_STB_IMAGE /Feflif.exe -I getopt\ /DSTATIC_GETOPT ..\..\src\flif.cpp ..\..\src\flif-dec.cpp ..\..\src\flif-enc.cpp ..\..\src\common.cpp ..\..\src\io.cpp ..\..\src\maniac\*.c* ..\..\src\transform\*.cpp ..\..\src\image\*.c* getopt/getopt.c ..\..\extern\lodepng.cpp /WX /EHsc /Ox /Oy /MT /DNDEBUG

echo debug
cl /nologo /DFLIF_USE_STB_IMAGE /Feflif-d.exe -I getopt\ /DSTATIC_GETOPT ..\..\src\flif.cpp ..\..\src\flif-dec.cpp ..\..\src\flif-enc.cpp ..\..\src\common.cpp ..\..\src\io.cpp ..\..\src\maniac\*.c* ..\..\src\transform\*.cpp ..\..\src\image\*.c* getopt/getopt.c ..\..\extern\lodepng.cpp /WX /EHsc /Zi /Oy- /MDd /DDEBUG

del *.obj

echo test
if exist output.flif del output.flif
flif ..\..\tools/kodim01.png output.flif
if exist output.png  del output.png
flif output.flif output.png
start output.png