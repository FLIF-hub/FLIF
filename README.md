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

## License

FLIF is copylefted free/libre software: you can redistribute it and/or modify it, provided that you share back.

The reference implementation of FLIF is released under the terms of the GNU Lesser General Public License version 3 or later (LGPLv3+).

The decoder library `libflif_dec` is released under a weaker, non-copyleft free software license: the Apache 2.0 license.

The example application `viewflif` illustrates how to use the decode library.
The example code in `viewflif.c` is in the public domain (Creative Commons CC0 1.0 Universal).

* * *

## Build Instructions

### GNU/Linux

#### Install the dependencies

On Debian:

  * for the encoder/decoder: `sudo apt-get install libpng-dev`
  * for the viewer: `sudo apt-get install libsdl2-dev`

On Fedora:

  * for the encoder/decoder: `sudo dnf install libpng-devel`
  * for the viewer: `sudo dnf install SDL-devel`

#### Compile

  * Navigate to the FLIF/src directory and run `make` to compile everything, or
    * `make flif` to build just the `flif` command line tool
    * `make libflif.so` to build the LGPL'ed shared library
    * `make libflif_dec.so` to build the Apache licensed decode-only shared library
    * `make viewflif` to build the example viewer (it depends on the decode library)

#### Install

* `sudo make install` if you want to install it globally

### Windows

* Install Visual Studio
  ([VS Community 2015](https://www.visualstudio.com/en-us/products/free-developer-offers-vs.aspx)
  is free for open source projects)
* Open the `build\MSVC` folder and double-click the `dl_make_vs.bat` file.
  This will download required libraries and run `nmake` to build `flif.exe`.
  Then, run in the command line:
  * `nmake libflif.dll` to build the shared library
  * `nmake viewflif.exe` to build the example viewer

### OS X

* Install [homebrew](http://brew.sh)
* Install the dependencies: `brew install pkg-config libpng sdl2`
* Run `make` in the FLIF/src directory


* * *

## Pre-Built Binaries

These will be available on the Release page

* https://github.com/FLIF-hub/FLIF/releases

* * *

## Related Projects

* **[Poly FLIF](https://github.com/UprootLabs/poly-flif)** - A javascript polyfill that allows you to use FLIF files in the browser. ([Demo](https://uprootlabs.github.io/poly-flif))
* **[UGUI: FLIF](http://flif.info/UGUI_FLIF)** - A GUI that allows you to convert and view FLIF files.
* **[Ivy, the Taggable Image Viewer](https://github.com/lehitoskin/ivy)** – An image viewer that supports FLIF via [riff](https://github.com/lehitoskin/riff)
* **[flifcrush](https://github.com/FLIF-hub/flifcrush)** - A brute-force FLIF optimizer.
* **[libflif.js](https://github.com/saschanaz/libflif.js/)** – A javascript FLIF encoder and decoder. ([Demo](https://saschanaz.github.io/libflif.js/)
* **[flif-rs](https://github.com/panicbit/flif-rs)** – A work-in-progress implementation of FLIF in Rust.
* **[FLIF Windows Codec](https://github.com/peirick/FlifWICCodec)** – A plugin that allows you to decode and encode FLIF files in Windows aplications using the Windows Imaging Component (WIC) API. That allows e.g., to see the files in Windows PhotoViewer and Windows Explorer.
* **[FLIF Windows Plugin](https://github.com/fherzog2/flif_windows_plugin)** – This plugin enables decoding of FLIF images in applications which use the Windows Imaging Component API. In this way, FLIF images can be viewed in Windows Explorer like other common image formats.
* **[qt-flif-plugin](https://github.com/spillerrec/qt-flif-plugin)** – Enables Qt4 and Qt5 applications to load images in the FLIF image format.
* **[FLIFSharp](https://github.com/purenewman/FLIFSharp)** – C# FLIF Library Bindings.
* **[go-flif](https://github.com/chrisfelesoid/go-flif)** – Go FLIF Library Bindings.
