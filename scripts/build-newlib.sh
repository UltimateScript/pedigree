#!/bin/bash

# Fix executable path as compilers are most likely not present in $PATH right now.
export PATH="$SRCDIR/compilers/dir/bin:$PATH"

# Fix include path so newlib's own headers don't override Pedigree's.
CC_VERSION=`$XGCC -dumpversion`
XGCC="$XGCC -nostdinc -nostdlib"
XGCC="$XGCC -I$SRCDIR/src/subsys/posix/include"
XGCC="$XGCC -I$SRCDIR/compilers/dir/lib/gcc/$COMPILER_TARGET/$CC_VERSION/include"
XGCC="$XGCC -I$SRCDIR/compilers/dir/lib/gcc/$COMPILER_TARGET/$CC_VERSION/include-fixed"
XGCC="$XGCC -I$SRCDIR/images/local/include"

rm -rf mybuild
mkdir -p mybuild
cd mybuild

date >newlib.log 2>&1

die()
{
    cat newlib.log >&2; exit 1;
}

CC="$XGCC $XCFLAGS" LD="$XLD" \
../newlib/configure --host=$COMPILER_TARGET --target=$COMPILER_TARGET \
                    --enable-newlib-multithread --enable-newlib-mb \
                    --disable-newlib-supplied-syscalls --with-pic \
                    >>newlib.log 2>&1 || die

make >>newlib.log 2>&1 || die

cp libg.a "$DROPDIR/stock-libg.a"
cp libm.a "$DROPDIR/stock-libm.a"
