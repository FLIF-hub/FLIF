CXXFLAGS := $(shell pkg-config --cflags zlib libpng)
LDFLAGS := $(shell pkg-config --libs zlib libpng)

# for running interface-test
export LD_LIBRARY_PATH=$(shell pwd):$LD_LIBRARY_PATH

FILES_H := maniac/*.hpp maniac/*.cpp image/*.hpp transform/*.hpp flif-enc.hpp flif-dec.hpp common.hpp flif_config.h fileio.hpp io.hpp io.cpp config.h
FILES_CPP := maniac/chance.cpp image/crc32k.cpp image/image.cpp image/image-png.cpp image/image-pnm.cpp image/image-pam.cpp image/image-rggb.cpp image/color_range.cpp transform/factory.cpp common.cpp flif-enc.cpp flif-dec.cpp io.cpp

flif: $(FILES_H) $(FILES_CPP) flif.cpp
	$(CXX) -std=gnu++11 $(CXXFLAGS) -DNDEBUG -O3 -g0 -Wall $(FILES_CPP) flif.cpp -o flif $(LDFLAGS)

flif.prof: $(FILES_H) $(FILES_CPP) flif.cpp
	$(CXX) -std=gnu++11 $(CXXFLAGS) -DNDEBUG -O3 -g0 -pg -Wall $(FILES_CPP) flif.cpp -o flif.prof $(LDFLAGS)

flif.dbg: $(FILES_H) $(FILES_CPP) flif.cpp
	$(CXX) -std=gnu++11 $(CXXFLAGS) -O0 -ggdb3 -Wall $(FILES_CPP) flif.cpp -o flif.dbg $(LDFLAGS)

flif.asan: $(FILES_H) $(FILES_CPP) flif.cpp
	$(CXX) -std=gnu++11 $(CXXFLAGS) -O3 -fsanitize=address,undefined -fno-omit-frame-pointer -g3 -Wall $(FILES_CPP) flif.cpp -o flif.asan $(LDFLAGS)

libflif.so: $(FILES_H) $(FILES_CPP) flif.h flif-interface-private.h flif-interface.cpp
	$(CXX) -std=gnu++11 $(CXXFLAGS) -DNDEBUG -O3 -g0 -Wall -shared -fPIC $(FILES_CPP) flif-interface.cpp -o libflif.so $(LDFLAGS)

libflifd.so: $(FILES_H) $(FILES_CPP) flif.h flif-interface-private.h flif-interface.cpp
	$(CXX) -std=gnu++11 $(CXXFLAGS) -O0 -ggdb3 -Wall -shared -fPIC $(FILES_CPP) flif-interface.cpp -o libflifd.so $(LDFLAGS)

viewflif: libflif.so flif.h tools/viewer.c
	gcc -O2 -ggdb3 $(shell sdl2-config --cflags) $(shell sdl2-config --libs) -Wall -I. tools/viewer.c -o viewflif -L. -lflif

all: flif libflif.so viewflif

test-interface: libflifd.so flif.h tools/test.c
	gcc -O0 -ggdb3 -Wall -I. tools/test.c -o test-interface -L. -lflifd


test: flif test-interface
	mkdir -p testFiles
	./test-interface
	./tools/test-roundtrip.sh tools/2_webp_ll.png testFiles/2_webp_ll.flif testFiles/decoded_2_webp_ll.png
	./tools/test-roundtrip.sh tools/kodim01.png testFiles/kodim01.flif testFiles/decoded_kodim01.png

