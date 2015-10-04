CXXFLAGS := $(shell pkg-config --cflags zlib libpng) 
LDFLAGS := $(shell pkg-config --libs zlib libpng)
UNAME := $(shell uname)

#OSX detection
ifeq ($(UNAME), Darwin)
HOMEBREW := -I/usr/local/Cellar/libpng/1.6.18/include -L/usr/local/Cellar/libpng/1.6.18/lib
else
HOMEBREW :=
endif

flif: maniac/*.h maniac/*.cpp image/*.h image/*.cpp transform/*.h transform/*.cpp flif.cpp flif.h flif_config.h
	$(CXX) -std=gnu++11 $(CXXFLAGS) $(HOMEBREW) $(LDFLAGS) -DNDEBUG -O3 -g0 -Wall maniac/util.cpp maniac/chance.cpp image/crc32k.cpp image/image.cpp image/image-png.cpp image/image-pnm.cpp image/image-pam.cpp image/color_range.cpp transform/factory.cpp flif.cpp common.cpp flif-enc.cpp flif-dec.cpp -lpng -o flif

flif.prof: maniac/*.h maniac/*.cpp image/*.h image/*.cpp transform/*.h transform/*.cpp flif.cpp flif.h flif_config.h
	$(CXX) -std=gnu++11 $(CXXFLAGS) $(HOMEBREW) $(LDFLAGS) -DNDEBUG -O3 -g0 -pg -Wall maniac/util.cpp maniac/chance.cpp image/crc32k.cpp image/image.cpp image/image-png.cpp image/image-pnm.cpp image/image-pam.cpp image/color_range.cpp transform/factory.cpp flif.cpp -lpng -o flif.prof

flif.dbg: maniac/*.h maniac/*.cpp image/*.h image/*.cpp transform/*.h transform/*.cpp flif.cpp flif.h flif_config.h
	$(CXX) -std=gnu++11 $(CXXFLAGS) $(HOMEBREW) $(LDFLAGS) -O0 -ggdb3 -Wall maniac/util.cpp maniac/chance.cpp image/crc32k.cpp image/image.cpp image/image-png.cpp image/image-pnm.cpp image/image-pam.cpp image/color_range.cpp transform/factory.cpp flif.cpp -lpng -o flif.dbg


# installed homebrew and used keg named "libpng" - paths may need to be updated if versions change