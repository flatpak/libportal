#!/bin/sh
#
# Wrapper to set up the right environment variables and start a nested
# shell. Usage:
#
# $ ./tests/gir-testenv.sh
# (nested shell) $ pytest
# (nested shell) $ exit
#
# If you have meson 0.58 or later, you can instead do:
# $ meson devenv -C builddir
# (nested shell) $ cd ../tests
# (nested shell) $ pytest
# (nested shell) $ exit
#

builddir=$(find $PWD -name meson-logs -printf "%h" -quit)

if [ -z "$builddir" ]; then
    echo "Unable to find meson builddir"
    exit 1
fi

echo "Using meson builddir: $builddir"

export LD_LIBRARY_PATH="$builddir/libportal:$LD_LIBRARY_PATH"
export GI_TYPELIB_PATH="$builddir/libportal:$GI_TYPELIB_PATH"

echo "pytest must be run from within the tests/ directory"
# Don't think this is portable, but oh well
${SHELL}
