PREFIX := $(DESTDIR)/usr/local
CXXFLAGS := $(shell pkg-config --cflags zlib libpng) -DLODEPNG_NO_COMPILE_PNG -DLODEPNG_NO_COMPILE_DISK
LDFLAGS := $(shell pkg-config --libs libpng)

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
export LD_LIBRARY_PATH=$(shell pwd):/usr/local/lib:$LD_LIBRARY_PATH

FILES_H := maniac/*.hpp maniac/*.cpp image/*.hpp transform/*.hpp flif-enc.hpp flif-dec.hpp common.hpp flif_config.h fileio.hpp io.hpp io.cpp config.h compiler-specific.hpp ../extern/lodepng.h
FILES_CPP := maniac/chance.cpp maniac/symbol.cpp image/crc32k.cpp image/image.cpp image/image-png.cpp image/image-pnm.cpp image/image-pam.cpp image/image-rggb.cpp image/image-metadata.cpp image/color_range.cpp transform/factory.cpp common.cpp flif-enc.cpp flif-dec.cpp io.cpp ../extern/lodepng.cpp

all: flif libflif$(LIBEXT)
decoder: libflif_dec$(LIBEXT) dflif

# options to consider: -fvect-cost-model=unlimited -funroll-all-loops
OPTIMIZATIONS := -DNDEBUG -O2 -ftree-vectorize
# there are often problems with clang and lto also it doesn't seem to know -fwhole-program
ifeq ($(CXX), g++)
	OPTIMIZATIONS := $(OPTIMIZATIONS) -flto -fwhole-program
endif

LIB_OPTIMIZATIONS := -DNDEBUG -O2
ifeq ($(CXX), g++)
	LIB_OPTIMIZATIONS := $(LIB_OPTIMIZATIONS) -flto
endif

# Command-line FLIF encoding/decoding tool - LGPLv3
flif: $(FILES_H) $(FILES_CPP) flif.cpp
	$(CXX) -std=gnu++11 $(CXXFLAGS) $(OPTIMIZATIONS) -g0 -Wall $(FILES_CPP) flif.cpp $(LDFLAGS) -o flif

# Command-line FLIF decoding tool - Apache2 (not built by default)
dflif: $(FILES_H) $(FILES_CPP) flif.cpp
	$(CXX) -std=gnu++11 $(CXXFLAGS) $(OPTIMIZATIONS) -DDECODER_ONLY -g0 -Wall $(FILES_CPP) flif.cpp $(LDFLAGS) -o dflif

# Decoder-only library - Apache2 (not built by default)
libflif_dec$(LIBEXT): $(FILES_H) $(FILES_CPP) library/flif_dec.h library/flif-interface-private_dec.hpp library/flif-interface_dec.cpp
	echo $(OS)
	$(CXX) -std=gnu++11 $(CXXFLAGS) $(LIB_OPTIMIZATIONS) -DDECODER_ONLY -g0 -Wall -shared -fPIC $(FILES_CPP) library/flif-interface_dec.cpp $(LDFLAGS) -Wl,$(SONAME),libflif_dec$(LIBEXTV) -o libflif_dec$(LIBEXTV)
	ln -sf libflif_dec$(LIBEXTV) libflif_dec$(LIBEXT)

# Decoder + encoder library - LGPL
libflif$(LIBEXT): $(FILES_H) $(FILES_CPP) library/*.h library/*.hpp library/*.cpp
	$(CXX) -std=gnu++11 $(CXXFLAGS) $(LIB_OPTIMIZATIONS) -g0 -Wall -shared -fPIC $(FILES_CPP) library/flif-interface.cpp $(LDFLAGS) -Wl,$(SONAME),libflif$(LIBEXTV) -o libflif$(LIBEXTV)
	ln -sf libflif$(LIBEXTV) libflif$(LIBEXT)

libflif.dbg$(LIBEXT): $(FILES_H) $(FILES_CPP) library/*.h library/*.hpp library/*.cpp
	$(CXX) -std=gnu++11 $(CXXFLAGS) -DDEBUG -O1 -ggdb3 -Wall -shared -fPIC $(FILES_CPP) library/flif-interface.cpp $(LDFLAGS) -Wl,$(SONAME),libflif$(LIBEXTV) -o libflif.dbg$(LIBEXTV)
	ln -sf libflif.dbg$(LIBEXTV) libflif.dbg$(LIBEXT)

# Example application: simple FLIF viewer - public domain
viewflif: libflif$(LIBEXT) viewflif.c
	$(CC) -std=gnu11 -O3 -ggdb3 $(shell sdl2-config --cflags) -Wall -Ilibrary/ viewflif.c -L. -lflif $(shell sdl2-config --libs) -o viewflif


install: flif libflif$(LIBEXT)
	install -d $(PREFIX)/bin $(PREFIX)/lib $(PREFIX)/share/man/man1
	install -m 755 flif $(PREFIX)/bin
	strip -x $(PREFIX)/bin/flif
	install -m 755 libflif$(LIBEXT) libflif$(LIBEXTV) $(PREFIX)/lib
	strip -x $(PREFIX)/lib/libflif$(LIBEXT) \
	  $(PREFIX)/lib/libflif$(LIBEXTV)
	install -m 644 ../doc/flif.1 $(PREFIX)/share/man/man1
	install -m 755 ../tools/gif2flif $(PREFIX)/bin
	install -m 755 ../tools/apng2flif $(PREFIX)/bin

install-viewflif:
	install -m 755 viewflif $(PREFIX)/bin
	strip -x $(PREFIX)/bin/viewflif

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

clean:
	rm -f flif dflif libflif*$(LIBEXT)* viewflif flif.asan flif.dbg flif.prof flif.stats test-interface


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
