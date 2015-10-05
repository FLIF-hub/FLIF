CXXFLAGS := $(shell pkg-config --cflags zlib libpng)
LDFLAGS := $(shell pkg-config --libs zlib libpng)

flif: maniac/*.h maniac/*.cpp image/*.h image/*.cpp transform/*.h transform/*.cpp flif.cpp flif-enc.cpp flif-dec.cpp common.cpp flif.h flif-enc.h flif-dec.h common.h flif_config.h fileio.h
	$(CXX) -std=gnu++11 $(CXXFLAGS) -DNDEBUG -O3 -g0 -Wall maniac/util.cpp maniac/chance.cpp image/crc32k.cpp image/image.cpp image/image-png.cpp image/image-pnm.cpp image/image-pam.cpp image/color_range.cpp transform/factory.cpp flif.cpp common.cpp flif-enc.cpp flif-dec.cpp -o flif $(LDFLAGS)

flif.prof: maniac/*.h maniac/*.cpp image/*.h image/*.cpp transform/*.h transform/*.cpp flif.cpp flif.h flif_config.h
	$(CXX) -std=gnu++11 $(CXXFLAGS) $(LDFLAGS) -DNDEBUG -O3 -g0 -pg -Wall maniac/util.cpp maniac/chance.cpp image/crc32k.cpp image/image.cpp image/image-png.cpp image/image-pnm.cpp image/image-pam.cpp image/color_range.cpp transform/factory.cpp flif.cpp common.cpp flif-enc.cpp flif-dec.cpp -o flif.prof

flif.dbg: maniac/*.h maniac/*.cpp image/*.h image/*.cpp transform/*.h transform/*.cpp flif.cpp flif.h flif_config.h
	$(CXX) -std=gnu++11 $(CXXFLAGS) $(LDFLAGS) -O0 -ggdb3 -Wall maniac/util.cpp maniac/chance.cpp image/crc32k.cpp image/image.cpp image/image-png.cpp image/image-pnm.cpp image/image-pam.cpp image/color_range.cpp transform/factory.cpp flif.cpp common.cpp flif-enc.cpp flif-dec.cpp -o flif.dbg

test: flif
	mkdir -p testFiles
	IN=benchmark/input/webp_gallery/2_webp_ll.png;		\
	OUTF=testFiles/2_webp_ll.flif;				\
	OUTP=testFiles/decoded_2_webp_ll.png;			\
	./flif "$${IN}" "$${OUTF}";				\
	./flif -d $${OUTF} $${OUTP};				\
	test "`compare -metric AE $${IN} $${OUTP} null 2>&1`" = "0"
	IN=benchmark/input/kodak/kodim01.png ;		\
	OUTF=testFiles/kodim01.flif;				\
	OUTP=testFiles/decoded_kodim01.png;			\
	./flif "$${IN}" "$${OUTF}";				\
	./flif -d $${OUTF} $${OUTP};				\
	test "`compare -metric AE $${IN} $${OUTP} null 2>&1`" = "0"

