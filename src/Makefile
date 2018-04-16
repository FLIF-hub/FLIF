PREFIX := $(DESTDIR)/usr/local
CXXFLAGS := $(CXXFLAGS) $(shell pkg-config --cflags zlib libpng) -DLODEPNG_NO_COMPILE_PNG -DLODEPNG_NO_COMPILE_DISK
CFLAGS := $(CFLAGS) $(shell pkg-config --cflags zlib libpng) -DLODEPNG_NO_COMPILE_PNG -DLODEPNG_NO_COMPILE_DISK
LDFLAGS := $(LDFLAGS) $(shell pkg-config --libs libpng)

OSNAME := $(shell uname -s)
SONAME = -soname
LIBEXT = .so
SOVERSION = 0
LIBEXTV = $(LIBEXT).$(SOVERSION)
ifeq ($(OSNAME),Darwin)
  SONAME = -install_name
  LIBEXT = .dylib
  LIBEXTV = .$(SOVERSION)$(LIBEXT)
endif

# for running interface-test
export LD_LIBRARY_PATH := $(shell pwd):/usr/local/lib:$(LD_LIBRARY_PATH)

FILES_H := maniac/*.hpp maniac/*.cpp image/*.hpp transform/*.hpp flif-enc.hpp flif-dec.hpp common.hpp flif_config.h fileio.hpp io.hpp io.cpp config.h compiler-specific.hpp ../extern/lodepng.h
FILES_CPP := maniac/chance.cpp maniac/symbol.cpp image/crc32k.cpp image/image.cpp image/image-png.cpp image/image-pnm.cpp image/image-pam.cpp image/image-rggb.cpp image/image-metadata.cpp image/color_range.cpp transform/factory.cpp common.cpp flif-enc.cpp flif-dec.cpp io.cpp ../extern/lodepng.cpp
FILES_O := maniac/chance.o maniac/symbol.o image/crc32k.o image/image.o image/image-png.o image/image-pnm.o image/image-pam.o image/image-rggb.o image/image-metadata.o image/color_range.o transform/factory.o common.o flif-enc.o flif-dec.o io.o ../extern/lodepng.o

all: flif libflif$(LIBEXT)
decoder: libflif_dec$(LIBEXT) dflif

# options to consider: -fvect-cost-model=unlimited -funroll-all-loops
OPTIMIZATIONS := -DNDEBUG -O2 -ftree-vectorize
# there are often problems with clang and lto also it doesn't seem to know -fwhole-program
ifeq ($(CXX), g++)
	OPTIMIZATIONS := $(OPTIMIZATIONS) -flto
endif

LIB_OPTIMIZATIONS := -DNDEBUG -O2
ifeq ($(CXX), g++)
	LIB_OPTIMIZATIONS := $(LIB_OPTIMIZATIONS) -flto
endif

# This is how .cpp files are compiled to .o files
.cpp.o:
	$(CXX) -c -std=gnu++11 $(CPPFLAGS) $(CXXFLAGS) $(OPTIMIZATIONS) -Wall -fPIC -o $*.o $*.cpp

# Command-line FLIF encoding/decoding tool - LGPLv3
flif: $(FILES_O) flif.o
	$(CXX) -std=gnu++11 $(CXXFLAGS) $(CPPFLAGS) $(LIB_OPTIMIZATIONS) -Wall -fPIC -o flif flif.o $(FILES_O) $(LDFLAGS)

# Command-line FLIF decoding tool - Apache2 (not built by default)
dflif: $(FILES_H) libflif_dec$(LIBEXT) flif.cpp
	$(CXX) -std=gnu++11 $(CXXFLAGS) $(OPTIMIZATIONS) -DDECODER_ONLY -g0 -Wall flif.cpp $(LDFLAGS) -L. -lflif_dec -o dflif

# Decoder-only library - Apache2 (not built by default)
libflif_dec$(LIBEXT): $(FILES_H) $(FILES_CPP) library/flif_dec.h library/flif-interface-private_dec.hpp library/flif-interface_dec.cpp
	echo $(OS)
	$(CXX) -std=gnu++11 $(CXXFLAGS) $(LIB_OPTIMIZATIONS) -DDECODER_ONLY -g0 -Wall -shared -fPIC $(FILES_CPP) library/flif-interface_dec.cpp $(LDFLAGS) -Wl,$(SONAME),libflif_dec$(LIBEXTV) -o libflif_dec$(LIBEXTV)
	ln -sf libflif_dec$(LIBEXTV) libflif_dec$(LIBEXT)

# Decoder + encoder library - LGPL
libflif$(LIBEXT): $(FILES_O) library/flif-interface.o
	$(CXX) -shared -std=gnu++11 $(CPPFLAGS) $(CXXFLAGS) $(LIB_OPTIMIZATIONS) -Wall -fPIC -o libflif$(LIBEXTV) $(FILES_O) library/flif-interface.o -Wl,$(SONAME),libflif$(LIBEXTV) $(LDFLAGS)
	ln -sf libflif$(LIBEXTV) libflif$(LIBEXT)

libflif.dbg$(LIBEXT): $(FILES_H) $(FILES_CPP) library/*.h library/*.hpp library/*.cpp
	$(CXX) -std=gnu++11 $(CXXFLAGS) -DDEBUG -O0 -ggdb3 -Wall -shared -fPIC $(FILES_CPP) library/flif-interface.cpp $(LDFLAGS) -Wl,$(SONAME),libflif$(LIBEXTV) -o libflif.dbg$(LIBEXTV)
	ln -sf libflif.dbg$(LIBEXTV) libflif.dbg$(LIBEXT)

# Example application: simple FLIF viewer - public domain
viewflif: libflif$(LIBEXT) viewflif.c
	$(CC) -std=gnu11 $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -O3 $(shell sdl2-config --cflags) -Wall -Ilibrary/ viewflif.c -L. -lflif $(shell sdl2-config --libs) -o viewflif

# gdk-pixbuf loader, enabling FLIF-viewing in pixbuf applications like Eye of Gnome
# The loader is LGPLed so there's no reason to use libflif_dec here
# -Werror=implicit-function-declaration is useful here to check for preprocessor dependency errors
libpixbufloader-flif$(LIBEXT): libflif$(LIBEXT) flif-pixbuf-loader.c
	$(CC) -shared $(CFLAGS) -Ilibrary/ -fPIC -Wall -Werror=implicit-function-declaration $(shell pkg-config --cflags gdk-pixbuf-2.0) -o libpixbufloader-flif$(LIBEXT) flif-pixbuf-loader.c $(LDFLAGS) $(shell pkg-config --libs gdk-pixbuf-2.0) -L. -lflif

pixbufloader: libpixbufloader-flif$(LIBEXT)

install: flif libflif$(LIBEXT) install-libflif$(LIBEXT)
	install -d $(PREFIX)/bin $(PREFIX)/share/man/man1
	install -m 755 flif $(PREFIX)/bin
	strip -x $(PREFIX)/bin/flif
	install -m 644 ../doc/flif.1 $(PREFIX)/share/man/man1
	install -m 755 ../tools/gif2flif $(PREFIX)/bin
	install -m 755 ../tools/apng2flif $(PREFIX)/bin

install-viewflif:
	install -m 755 viewflif $(PREFIX)/bin
	strip -x $(PREFIX)/bin/viewflif

install-libflif$(LIBEXT): libflif$(LIBEXT)
	install -d $(PREFIX)/lib
	install -m 755 libflif$(LIBEXT) libflif$(LIBEXTV) $(PREFIX)/lib
	strip -x $(PREFIX)/lib/libflif$(LIBEXT) \
	  $(PREFIX)/lib/libflif$(LIBEXTV)

# There is no prefix here because gdk-pixbuf will not search /usr/local for loadrs
# while it sucks, it's the truth
install-libpixbufloader-flif$(LIBEXT): libpixbufloader-flif$(LIBEXT) install-libflif$(LIBEXT)
	install -c -d /usr/lib/gdk-pixbuf-2.0/2.10.0/loaders
	install -c -m 755 -s libpixbufloader-flif$(LIBEXT) /usr/lib/gdk-pixbuf-2.0/2.10.0/loaders/
	gdk-pixbuf-query-loaders --update-cache
	xdg-mime install --novendor flif-mime.xml

install-pixbufloader: install-libpixbufloader-flif$(LIBEXT)

install-dev:
	install -d $(PREFIX)/include
	install -m 644 library/*.h $(PREFIX)/include

install-decoder: decoder
	install -m 755 dflif $(PREFIX)/bin
	strip -x $(PREFIX)/bin/dflif
	install -m 755 libflif_dec$(LIBEXT) libflif_dec$(LIBEXTV) $(PREFIX)/lib
	strip -x $(PREFIX)/lib/libflif_dec$(LIBEXT) \
	  $(PREFIX)/lib/libflif_dec$(LIBEXTV)

magic:
	if ! grep -q FLIF /etc/magic; then cat ../doc/flif.magic >> /etc/magic; fi

uninstall:
	rm -f $(PREFIX)/bin/flif
	rm -f $(PREFIX)/bin/viewflif
	rm -f $(PREFIX)/bin/gif2flif
	rm -f $(PREFIX)/bin/apng2flif
	rm -f $(PREFIX)/include/flif.h
	rm -f $(PREFIX)/include/flif_common.h
	rm -f $(PREFIX)/include/flif_dec.h
	rm -f $(PREFIX)/include/flif_enc.h
	rm -f $(PREFIX)/lib/libflif$(LIBEXT)
	rm -f $(PREFIX)/lib/libflif_dec$(LIBEXT)
	rm -f $(PREFIX)/lib/libflif$(LIBEXTV)
	rm -f $(PREFIX)/lib/libflif_dec$(LIBEXTV)
	rm -f $(PREFIX)/share/man/man1/flif.1
	rm -f /usr/lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-flif$(LIBEXT)

clean:
	rm -f flif dflif lib*flif*$(LIBEXT)* viewflif flif.asan flif.dbg flif.prof flif.stats test-interface $(FILES_O) flif.o library/flif-interface.o


# The targets below are only meant for developers

test-interface: libflif.dbg$(LIBEXT) libflif$(LIBEXT) ../tools/test.c
	$(CC) -O0 -ggdb3 -Wall -Ilibrary/ ../tools/test.c -L. -lflif.dbg  -o test-interface

test: flif test-interface
	mkdir -p ../tmp-test
	./test-interface ../tmp-test/dummy.flif
	../tools/test-roundtrip.sh ./flif ../tools/2_webp_ll.png ../tmp-test/2_webp_ll.flif ../tmp-test/decoded_2_webp_ll.png
	../tools/test-roundtrip.sh ./flif ../tools/kodim01.png ../tmp-test/kodim01.flif ../tmp-test/decoded_kodim01.png
	../tools/test-lossy.sh ./flif ../tools/kodim01.png ../tmp-test/kodim01-lossy.flif ../tmp-test/decoded_kodim01-lossy.png
	../tools/test-roundtrip_anim.sh ./flif ../tools/endless_war.gif ../tmp-test/endless_war.flif
	../tools/test-roundtrip_anim_framedir.sh ./flif ../tools/bouncing_ball_frames ../tmp-test/bouncing_ball.flif
	../tools/test-metadata.sh ./flif ../testFiles/sig05-014.png ../tmp-test/out-meta.flif ../tmp-test/out-meta.png


flif.stats: $(FILES_H) $(FILES_CPP) flif.cpp
	$(CXX) -std=gnu++11 $(CXXFLAGS) -DSTATS $(OPTIMIZATIONS) -g0 -Wall $(FILES_CPP) flif.cpp $(LDFLAGS) -o flif.stats

flif.prof: $(FILES_H) $(FILES_CPP) flif.cpp
	$(CXX) -std=gnu++11 $(CXXFLAGS) $(OPTIMIZATIONS) -g0 -pg -Wall $(FILES_CPP) flif.cpp $(LDFLAGS) -o flif.prof

flif.dbg: $(FILES_H) $(FILES_CPP) flif.cpp
	$(CXX) -std=gnu++11 $(CXXFLAGS) -Og -ggdb3 -Wall $(FILES_CPP) flif.cpp $(LDFLAGS) -o flif.dbg

flif.dbg0: $(FILES_H) $(FILES_CPP) flif.cpp
	$(CXX) -std=gnu++11 $(CXXFLAGS) -O0 -ggdb3 -Wall $(FILES_CPP) flif.cpp $(LDFLAGS) -o flif.dbg

flif.asan: $(FILES_H) $(FILES_CPP) flif.cpp
	$(CXX) -std=gnu++11 $(CXXFLAGS) $(OPTIMIZATIONS) -DDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer -g3 -Wall $(FILES_CPP) flif.cpp $(LDFLAGS) -o flif.asan

viewflif.asan: libflif$(LIBEXT) viewflif.c
	$(CC) -std=gnu11 -O0 -ggdb3 -fsanitize=address,undefined $(shell sdl2-config --cflags) -Wall -Ilibrary/ viewflif.c -L. -lflif $(shell sdl2-config --libs) -o viewflif.asan

# make UBSAN print stacktrace
test.asan: export UBSAN_OPTIONS = print_stacktrace=1
test.asan: flif.asan test-interface viewflif.asan
	mkdir -p ../tmp-test
	./test-interface ../tmp-test/dummy.flif
# ASAN may find leaks in code which is not ours and in that case it will throw bad exitcodes on first occurence
# but we want or tests to continue running, so ignore exit code for now
	../tools/test-roundtrip.sh ./flif.asan ../tools/2_webp_ll.png ../tmp-test/2_webp_ll.flif ../tmp-test/decoded_2_webp_ll.png || true
	../tools/test-roundtrip.sh ./flif.asan ../tools/kodim01.png ../tmp-test/kodim01.flif ../tmp-test/decoded_kodim01.png  || true
	../tools/test-lossy.sh ./flif.asan ../tools/kodim01.png ../tmp-test/kodim01-lossy.flif ../tmp-test/decoded_kodim01-lossy.png || true
	../tools/test-roundtrip_anim.sh ./flif.asan ../tools/endless_war.gif ../tmp-test/endless_war.flif || true
	../tools/test-roundtrip_anim_framedir.sh ./flif.asan ../tools/bouncing_ball_frames ../tmp-test/bouncing_ball.flif || true
