lpngdir = libpng-libpng-1.6.20-master-signed
zlibdir = zlib-1.2.8
sdl2dir = SDL2-2.0.3

Incdir = include 

!ifndef PLATFORM
PLATFORM=x86
!endif

Rt = MD
LibDir = $(PLATFORM)_$(Rt)
SrcDir = ..\..\src
ExtDir = ..\..\extern

!if "$(Rt)" == "MT" || "$(Rt)" == "MD"
CFLAGS = /nologo /WX /EHsc /GL /Ox /Oy /Gy /DNDEBUG /$(Rt)
LDFLAGS= /nologo /LTCG
!elseif  "$(Rt)" == "MTd" || "$(Rt)" == "MDd"
CFLAGS = /nologo /WX /EHsc /Zi /DDEBUG /$(Rt)
LDFLAGS= /nologo /DEBUG
!else
!ERROR Rt=[MT|MD|MTd|MDd]
!endif

OBJDIR = $(LibDir)\Obj
O = $(OBJDIR)\\

OBJ1 = $Oflif-dec.obj $Oflif-enc.obj $Oio.obj $Ocommon.obj $Ocolor_range.obj $Ocrc32k.obj $Obit.obj $Ochance.obj $Ofactory.obj $Oimage-metadata.obj $Olodepng.obj
OBJ2 = $Oimage.obj $Oimage-pam.obj $Oimage-png.obj $Oimage-pnm.obj $Oimage-rggb.obj
LIBOBJS  = $(OBJ1)
FLIFOBJS = $(OBJ1) $(OBJ2)

flif.exe: $(OBJDIR) $(LibDir)\zlib.lib $(LibDir)\libpng.lib $(FLIFOBJS)
	cl $(CFLAGS) /I$(Incdir) /Igetopt $(SrcDir)\flif.cpp getopt\getopt.c /DSTATIC_GETOPT /Fd$O /Fo$O -c
	link /out:$(LibDir)\$@ $Oflif.obj $Ogetopt.obj $(FLIFOBJS) $(LibDir)\zlib.lib $(LibDir)\libpng.lib $(LDFLAGS)
	copy "$(LibDir)\$@" $@

libflif.dll: $(OBJDIR) $(LIBOBJS)
	cl $(CFLAGS) $(SrcDir)\library\flif-interface.cpp /DFLIF_BUILD_DLL /Fd$O /Fo$O -c
	link /dll /out:"$(LibDir)\$@" $Oflif-interface.obj $(LIBOBJS) $(LDFLAGS)
	copy "$(LibDir)\$@" $@

libflif_dec.dll: $(OBJDIR) $(LIBOBJS)
	cl $(CFLAGS) $(SrcDir)\library\flif-interface_dec.cpp /DFLIF_BUILD_DLL /Fd$O /Fo$O -c
	link /dll /out:"$(LibDir)\$@" $Oflif-interface_dec.obj $(LIBOBJS) $(LDFLAGS)
	copy "$(LibDir)\$@" $@

viewflif.exe: libflif_dec.dll
	cl $(CFLAGS) /I$(SrcDir)\ /I$(SrcDir)\library\ /I$(sdl2dir)\include /Tp $(SrcDir)\viewflif.c /DFLIF_USE_DLL /Fd$O /Fo$O -c
	link /out:$(LibDir)\$@ $Oviewflif.obj libflif_dec.lib SDL2.lib SDL2main.lib $(LDFLAGS) /SUBSYSTEM:CONSOLE /LIBPATH:$(LibDir) /LIBPATH:$(sdl2dir)\lib\$(PLATFORM)\ 
	copy "$(LibDir)\$@" $@
	copy $(sdl2dir)\lib\$(PLATFORM)\SDL2.dll SDL2.dll

test: flif.exe test-interface.exe
	if exist output.flif del output.flif
	@echo ./flif.exe ../../tools/kodim01.png output.flif
	@powershell "Measure-Command{ ./flif.exe ../../tools/kodim01.png output.flif}"
	if exist output.png  del output.png
	flif output.flif output.png
	@if exist "viewflif.exe" viewflif.exe output.flif
	@if not exist "..\tmp-test\" mkdir ..\tmp-test
	if exist "test-interface.exe" test-interface.exe ..\tmp-test\dummy.flif
	start output.png

test-interface.exe: libflif.dll
	cl $(CFLAGS) /I$(Incdir) /I$(SrcDir)\ /I$(SrcDir)\library\ ..\..\tools\test.c /DFLIF_USE_DLL /Fd$O /Fo$O -c
	link /out:$(LibDir)\$@ $Otest.obj libflif.lib $(LDFLAGS) /LIBPATH:$(LibDir) 
	copy "$(LibDir)\$@" $@

$(OBJDIR):
	@if not exist "$(@)/" mkdir "$(@)"

$(LibDir)\zlib.lib: $(OBJDIR)
	@if not exist include mkdir include
	cd $(zlibdir)
	@nmake -f win32\Makefile.msc clean
	nmake -f win32\Makefile.msc zlib.lib CFLAGS="$(CFLAGS) /TC /Fdzlib" ARFLAGS="$(LDFLAGS)"
	copy zlib.lib ..\$(LibDir)\\
	copy zlib.h ..\$(Incdir)\\
	copy zconf.h ..\$(Incdir)\\
	cd ..

$(LibDir)\libpng.lib: $(LibDir)\zlib.lib
	@if not exist include mkdir include
	cd $(lpngdir)
	@nmake -f scripts\Makefile.vcwin32 clean
	nmake -f scripts\Makefile.vcwin32 libpng.lib CPPFLAGS="/I..\$(Incdir) " CFLAGS="$(CFLAGS) /TC /D_CRT_SECURE_NO_DEPRECATE /D_CRT_SECURE_NO_WARNINGS" ARFLAGS="$(LDFLAGS)"
	copy libpng.lib ..\$(LibDir)\\
	copy png.h ..\$(Incdir)\\
	copy pngconf.h ..\$(Incdir)\\
	copy pnglibconf.h ..\$(Incdir)\\
	cd ..

{$(SrcDir)\}.cpp{$(O)}.obj:
	cl $(CFLAGS) /Fd$O /Fo$O -c $<
{getopt\}.c{$(O)}.obj:
	cl $(CFLAGS) /DSTATIC_GETOPT /Fd$O /Fo$O -c $<
{$(SrcDir)\image\}.cpp{$(O)}.obj:
	cl $(CFLAGS) /I$(Incdir) /Fd$O /Fo$O -c $<
{$(SrcDir)\maniac\}.cpp{$(O)}.obj:
	cl $(CFLAGS) /Fd$O /Fo$O -c $<
{$(SrcDir)\transform\}.cpp{$(O)}.obj:
	cl $(CFLAGS) /Fd$O /Fo$O -c $<
{$(ExtDir)\}.cpp{$(O)}.obj:
	cl $(CFLAGS) /Fd$O /Fo$O -c $<

all: flif.exe libflif_dec.dll viewflif.exe test-interface.exe

clean:
	-del flif.exe
	-del test-interface.exe
	-del viewflif.exe
	-del SDL2.dll
	-del libflif.dll
	-del libflif_dec.dll
	-del output.png
	-del output.flif
	-rd /s /q ..\tmp-test
	-rd /s /q $(LibDir)
