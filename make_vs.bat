echo release
cl /nologo /DFLIF_USE_STB_IMAGE /Feflif.exe flif.cpp -I win32 maniac\*.c* getopt\*.c* transform\*.cpp image\*.c*  /WX /EHsc /DGETOPT_API= /Ox /Oy /MT /DNDEBUG

echo debug
cl /nologo /DFLIF_USE_STB_IMAGE /Feflif-d.exe flif.cpp -I win32 maniac\*.c* getopt\*.c* transform\*.cpp image\*.c*  /WX /EHsc /DGETOPT_API= /Zi /Oy- /MDd /DDEBUG

echo test
flif benchmark\compression-Waterloo.png output.flif 
flif -d output.flif output.png

