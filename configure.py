import ninja_syntax
from docopt import docopt
import os.path
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

arguments = docopt(__doc__)

ninja_file = open("build.ninja", "w")
n = ninja_syntax.Writer(ninja_file)

########################
# Generic build rules. #
########################
n.variable("cc", "gcc")
n.variable("cxx", "g++")

if arguments["--debug"]:
    n.variable("dbgflags", "-ggdb")
    debug_flag = ""
else:
    n.variable("dbgflags", "")
    debug_flag = " -DNDEBUG"

native_flag = " -march=native" if arguments["--native"] else ""
n.variable("optflags", "-O2 -ftree-vectorize" + native_flag + debug_flag)

defines = [" -D" + d for d in arguments["-D"]]

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
        if "flif-interface" in filename: continue
        src = os.path.join(root, filename)
        obj = os.path.join("obj", os.path.splitext(src)[0] + ".o")
        n.build(obj, "cxx", src, "src/precompile.h.gch")
        objects.append(obj)

n.build("src/flif", "cxxlink", objects)
n.default("src/flif")

ninja_file.close()
