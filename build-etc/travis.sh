#!/bin/bash

# Handle running on Travis-CI.
TRAVIS_OPTIONS=
if [ ! -z "$TRAVIS" ]; then
    TRAVIS_OPTIONS="-DPEDIGREE_TRAVIS=ON"

    # Override the compiler directory for Travis caching.
    COMPILER_DIR=$HOME/pedigree-compiler

    # The toolchain .deb file installs all files owned as root, which is not
    # useful when we want to symlink our crt* and POSIX headers. So, fix that.
    ME=`whoami`
    sudo chown -R "$ME" $COMPILER_DIR
fi
