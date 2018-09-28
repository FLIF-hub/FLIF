#!/usr/bin/python3
import os
import sys
import fnmatch


__doc__ = """Build configurator for FLIF.

Usage:
    configure.py [options] [-D var]...

Options:
  --native           Enable native optimizations.
  -d, --debug        Compile with debug.
  -D var             Define variable. Also -D EXAMPLE=1.
  -o file            Output ninja file [default: build.ninja].
  -h, --help         Show this screen.
"""

def find_objects(n, dirpaths):
    objects = []
    for dirpath in dirpaths:
        for root, dirnames, filenames in os.walk(dirpath):
            for filename in fnmatch.filter(filenames, "*.cpp"):
                if "flif-interface" in filename:
                    continue
                src = os.path.join(root, filename)
                obj = os.path.join("obj", os.path.splitext(src)[0] + ".o")
                n.build(obj, "cxx", src)
                objects.append(obj)
    return objects

def write_ninja(n, args):
    ########################
    # Generic build rules. #
    ########################
    exe_ext = ".exe" if os.name == "nt" else ""
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
    objects = find_objects(n, ["extern", "src"])
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

# Check if we're likely running for the first time.
first_time = not os.path.isfile(args["-o"])

# Write ninja build file.
with open(args["-o"], "w") as ninja_file:
    n = ninja_syntax.Writer(ninja_file)
    write_ninja(n, args)

# Check if we have ninja in path.
path = os.environ.get("PATH", os.defpath).split(os.pathsep)
if sys.platform == "win32":
    path.insert(0, os.curdir)
    pathext = os.environ.get("PATHEXT", "").split(os.pathsep)
    files = ["ninja" + ext for ext in pathext]
else:
    files = ["ninja"]

is_exe = lambda p: os.path.exists(p) and os.access(p, os.F_OK | os.X_OK) and not os.path.isdir(p)
has_ninja = any(is_exe(os.path.join(d, f)) for d in path for f in files)

if first_time:
    if not has_ninja:
        msg = """It appears you're running configure.py for the first time, but do not have
ninja in your path. On Windows we recommend simply downloading the binary:

    https://github.com/ninja-build/ninja/releases/download/v1.6.0/ninja-win.zip

Extract anywhere in your path, or even inside this directory.

On linux it's easiest to compile from source:

    wget https://github.com/ninja-build/ninja/archive/v1.6.0.tar.gz
    tar xf v1.6.0.tar.gz && rm v1.6.0.tar.gz && cd ninja-1.6.0
    {} configure.py --bootstrap
    sudo cp ninja /usr/local/bin

This should only take half a minute or so."""
        print(msg.format(os.path.basename(sys.executable)))
    else:
        print("Type 'ninja' to build FLIF.")
