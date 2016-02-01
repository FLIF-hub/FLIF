import os
import sys
import fnmatch


__doc__ = """Configurator for FLIF build.

Usage:
    configure.py [options] [-D var[=val]]...

Options:
  --native       Enable native optimizations.
  -d, --debug    Compile with debug.
  -D var=val     Define variable.
  -h, --help     Show this screen.
"""


def write_ninja(n, args):
    ########################
    # Generic build rules. #
    ########################
    n.variable("cc", "gcc")
    n.variable("cxx", "g++")

    if args["--debug"]:
        n.variable("dbgflags", "-ggdb")
        debug_flag = ""
    else:
        n.variable("dbgflags", "")
        debug_flag = " -DNDEBUG"

    native_flag = " -march=native" if args["--native"] else ""
    n.variable("optflags", "-O2 -ftree-vectorize" + native_flag + debug_flag)

    defines = [" -D" + d for d in args["-D"]]

    cxxflags = "-std=c++11 -Wall -pedantic `pkg-config --cflags zlib libpng`"
    n.variable("cxxflags", cxxflags + "".join(defines))
    n.variable("cxxlinkflags", "`pkg-config --libs libpng`")

    n.rule("cxx",
           "$cxx $xtype -MMD -MF $out.d $optflags $dbgflags $cxxflags -c $in -o $out",
           depfile="$out.d")

    n.rule("cxxlink", "$cxx $optflags $dbgflags $in $cxxlinkflags -o $out")

    ###################
    # Build commands. #
    ###################

    # Precompiled header.
    n.build("src/precompile.h.gch", "cxx", "src/precompile.h", variables={"xtype": "-x c++-header"})

    objects = []
    for root, dirnames, filenames in os.walk("src"):
        for filename in fnmatch.filter(filenames, "*.cpp"):
            if "flif-interface" in filename:
                continue
            src = os.path.join(root, filename)
            obj = os.path.join("obj", os.path.splitext(src)[0] + ".o")
            n.build(obj, "cxx", src, "src/precompile.h.gch")
            objects.append(obj)

    exe_ext = ".exe" if os.name == "nt" else ""
    n.build("src/flif" + exe_ext, "cxxlink", objects)
    n.default("src/flif" + exe_ext)


try:
    from docopt import docopt
    import ninja_syntax
except ImportError:
    msg = """You are missing one or more dependencies, install using:

    {} -m pip install --user docopt ninja-syntax

If you are using an old version of Python, and don't have pip,
download https://bootstrap.pypa.io/get-pip.py and run it."""

    print(msg.format(os.path.basename(sys.executable)))
    sys.exit(1)

args = docopt(__doc__)
with open("build.ninja", "w") as ninja_file:
    n = ninja_syntax.Writer(ninja_file)
    write_ninja(n, args)
