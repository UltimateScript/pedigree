#!/bin/bash

# Fix executable path as compilers are most likely not present in $PATH right now.
export PATH="$SRCDIR/compilers/dir/bin:$PATH"

cp "$SRCDIR/src/subsys/posix/musl/glue-musl.c" src/internal/pedigree-musl.c
# TODO: architecture hardcoded here
cp "$SRCDIR/src/subsys/posix/musl/syscall_arch.h" arch/x86_64/syscall_arch.h

# Remove the internal syscall.s as we implement it in our glue.
rm -f src/internal/x86_64/syscall.s

# Remove default signal restore (but we should add one of our own).
rm -f src/signal/x86_64/restore.s

# No vfork()
rm -f src/process/x86_64/vfork.s

# Remove some .s implementations that have .c alternatives.
rm -f src/thread/x86_64/{clone,__unmapself,__set_thread_area}.s

# Custom syscall_cp to use Pedigree's syscall mechanism.
cp "$SRCDIR/src/subsys/posix/musl/syscall_cp-x86_64.musl-s" src/thread/x86_64/syscall_cp.s

# Copy custom headers.
cp "$SRCDIR/src/subsys/posix/musl/fb.h" include/sys/
cp "$SRCDIR/src/subsys/posix/musl/klog.h" include/sys/

rm -rf build
mkdir -p build
cd build

date >musl.log 2>&1

die()
{
    cat musl.log >&2; exit 1;
}

CPPFLAGS="-I$SRCDIR/src/subsys/posix/syscalls -I$SRCDIR/src/system/include -D$ARCH_TARGET" \
CFLAGS="-O2 -g3 -ggdb -fno-omit-frame-pointer" \
../configure --target=$COMPILER_TARGET --prefix="$TARGETDIR" \
    --syslibdir="$TARGETDIR/lib" --enable-shared \
    >>musl.log 2>&1 || die

make >>musl.log 2>&1
make install >>musl.log 2>&1
