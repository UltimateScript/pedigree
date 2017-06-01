/*
 * Copyright (c) 2008-2014, Pedigree Developers
 *
 * Please see the CONTRIB file in the root of the source tree for a full
 * list of contributors.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

// crashtest: tests exception handling in the host POSIX subsystem

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Don't give a warning for division by zero
#pragma GCC diagnostic ignored "-Wdiv-by-zero"

void sighandler(int sig)
{
    printf("Happily ignoring signal %d\n", sig);
}

int main(int argc, char *argv[])
{
    printf("crashtest: a return value of zero means success\n");

    signal(SIGFPE, sighandler);
    signal(SIGILL, sighandler);

    // SIGFPE
    printf("Testing SIGFPE...\n");
    int a = 1 / 0;
    float b = 1.0f / 0.0f;
    double c = 1.0 / 0.0;
    printf("Works.\n");

    // SIGILL
    printf("Testing SIGILL...\n");
    char badops[] = {0xab, 0xcd, 0xef, 0x12};
    void (*f)() = (void (*)()) badops;
    f();
    printf("Works.\n");

    printf("All signals handled.\n");

    return 0;
}
