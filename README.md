# FLIF: Free Lossless Image Format

[![Join the chat at https://gitter.im/jonsneyers/FLIF](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/jonsneyers/FLIF?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)


FLIF is a lossless image format based on MANIAC compression. MANIAC (Meta-Adaptive Near-zero Integer Arithmetic Coding) is a variant of CABAC (context-adaptive binary arithmetic coding), where the contexts are nodes of decision trees which are dynamically learned at encode time.

FLIF outperforms PNG, FFV1, lossless WebP, lossless BPG and lossless JPEG2000 in terms of compression ratio.

Moreover, FLIF supports a form of progressive interlacing (essentially a generalization/improvement of PNG's Adam7) which means that any prefix (e.g. partial download) of a compressed file can be used as a reasonable lossy encoding of the entire image.

For more information on FLIF, visit http://flif.info

* * *

###Build Instructions

**Windows**

* Install Visual Studio
  ([VS Community 2015](https://www.visualstudio.com/en-us/products/free-developer-offers-vs.aspx)
  is free for open source projects)
* Click Start > All Programs > Visual Studio > Tools > `Developer Command Prompt`
* In the command prompt, navigate to the FLIF repo and run `make_vs.bat`

**OS X**

* Install [homebrew](http://brew.sh)
* Install kegs named `pkg-config` and `libpng`
* Run `make` in the FLIF directory

**Ubuntu**

* Install libpng-dev: `sudo apt-get install libpng-dev`
* Navigate to the FLIF directory and run `make`

* * *

###Pre-Built Binaries

Windows, OSX, and Ubuntu prebuilt binaries packaged together in a single zip:

* [flif_cli.zip](https://github.com/FLIF-hub/UGUI_FLIF/releases/download/v0.3.5/flif_cli.zip) - V0.1 (Built on October 15th, 2015)

* * *

###Other Projects

* **[Poly-FLIF](https://uprootlabs.github.io/poly-flif)** - A javascript polyfill that allows you to use FLIF files in the browser.
* **[UGUI: FLIF](https://github.com/FLIF-hub/UGUI_FLIF/releases)** - A GUI that allows you to convert and view FLIF files.
