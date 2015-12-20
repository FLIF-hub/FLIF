# FLIF: Free Lossless Image Format

[![Build Status](https://travis-ci.org/FLIF-hub/FLIF.svg?branch=master)](https://travis-ci.org/FLIF-hub/FLIF)
[![Join the chat at https://gitter.im/jonsneyers/FLIF](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/jonsneyers/FLIF?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![Donate via Gratipay](https://img.shields.io/gratipay/flif.svg)](https://gratipay.com/flif/)

FLIF is a lossless image format based on MANIAC compression. MANIAC (Meta-Adaptive Near-zero Integer Arithmetic Coding)
is a variant of CABAC (context-adaptive binary arithmetic coding), where the contexts are nodes of decision trees
which are dynamically learned at encode time.

FLIF outperforms PNG, FFV1, lossless WebP, lossless BPG and lossless JPEG2000 in terms of compression ratio.

Moreover, FLIF supports a form of progressive interlacing (essentially a generalization/improvement of PNG's Adam7)
which means that any prefix (e.g. partial download) of a compressed file can be used as a reasonable lossy encoding of the entire image.

For more information on FLIF, visit http://flif.info

* * *

###License

FLIF is copylefted free/libre software: you can redistribute it and/or modify it, provided that you share back.

The reference implementation of FLIF is released under the terms of the GNU General Public License version 3 or later (GPLv3+).

The decoder library `libflif_dec` is released under the terms of the GNU **Lesser** General Public License (LGPLv3+).

The example application `viewflif` illustrates how to use the decode library.
The example code in `viewflif.c` is in the public domain (Creative Commons CC0 1.0 Universal).

* * *

###Build Instructions

**GNU/Linux**

* Install the dependencies:
  * for the encoder/decoder: `sudo apt-get install libpng-dev`
  * for the viewer: `sudo apt-get install libsdl2-dev`
* Navigate to the FLIF/src directory and run `make` to compile everything, or
  * `make flif` to build just the `flif` command line tool
  * `make libflif.so` to build the GPL'ed shared library
  * `make libflif_dec.so` to build the LGPL'ed decode-only shared library
  * `make viewflif` to build the example viewer (it depends on the decode library)
* `sudo make install` if you want to install it globally

**Windows**

* Install Visual Studio
  ([VS Community 2015](https://www.visualstudio.com/en-us/products/free-developer-offers-vs.aspx)
  is free for open source projects)
* Open the `build\MSVC` folder and Double click the `dl_make_vs.bat`. This will download required libraries and run `nmake` to build `flif.exe`
  * `nmake libflif.dll` to build the shared library
  * `nmake viewflif.exe` to build the example viewer

**OS X**

* Install [homebrew](http://brew.sh)
* `brew install pkg-config libpng sdl2`
* `make` in the FLIF/src directory


* * *

###Pre-Built Binaries

These will be available on the Release page

* https://github.com/FLIF-hub/FLIF/releases

* * *

###Related Projects

* **[Poly FLIF](https://github.com/UprootLabs/poly-flif)** - A javascript polyfill that allows you to use FLIF files in the browser. ([Demo](https://uprootlabs.github.io/poly-flif))
* **[UGUI: FLIF](http://flif.info/UGUI_FLIF)** - A GUI that allows you to convert and view FLIF files.
* **[flifcrush](https://github.com/FLIF-hub/flifcrush)** - A brute-force FLIF optimizer.
