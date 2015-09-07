flif: maniac/*.h maniac/*.cpp image/*.h image/*.cpp transform/*.h transform/*.cpp flif.cpp flif.h flif_config.h
	g++ -std=gnu++11 -DNDEBUG -O3 -g0 -Wall maniac/util.c maniac/chance.cpp image/crc32k.c image/image.cpp image/image-png.cpp image/image-pnm.cpp image/color_range.cpp transform/factory.cpp flif.cpp -lpng -o flif

flif.prof: maniac/*.h maniac/*.cpp image/*.h image/*.cpp transform/*.h transform/*.cpp flif.cpp flif.h flif_config.h
	g++ -std=gnu++11 -DNDEBUG -O3 -g0 -pg -Wall maniac/util.c maniac/chance.cpp image/crc32k.c image/image.cpp image/image-png.cpp image/image-pnm.cpp image/color_range.cpp transform/factory.cpp flif.cpp -lpng -o flif.prof

flif.dbg: maniac/*.h maniac/*.cpp image/*.h image/*.cpp transform/*.h transform/*.cpp flif.cpp flif.h flif_config.h
	g++ -std=gnu++11 -O0 -ggdb3 -Wall maniac/util.c maniac/chance.cpp image/crc32k.c image/image.cpp image/image-png.cpp image/image-pnm.cpp image/color_range.cpp transform/factory.cpp flif.cpp -lpng -o flif.dbg
