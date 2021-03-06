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

#define PEDIGREE_EXTERNAL_SOURCE 1

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <memory>

#include <benchmark/benchmark.h>
#include <valgrind/callgrind.h>

#include "modules/system/ramfs/RamFs.h"
#include "modules/system/vfs/VFS.h"

static String g_DeepPath("ramfs»/foo/foo/foo/foo");
static String g_ShallowPath("ramfs»/");
static String g_MiddlePath("ramfs»/foo/foo");
static String g_DeepPathNoFs("/foo/foo/foo/foo");
static String g_ShallowPathNoFs("/");
static String g_MiddlePathNoFs("/foo/foo");
static String g_Alias("ramfs");

// A huge pile of paths to add to the filesystem for testing.
// Also used for randomly hitting the filesystem with lookups.
static String paths[] = {
    "ramfs»/foo",
    "ramfs»/bar",
    "ramfs»/baz",
    "ramfs»/foo/foo",
    "ramfs»/foo/bar",
    "ramfs»/foo/baz",
    "ramfs»/bar/foo",
    "ramfs»/bar/bar",
    "ramfs»/bar/baz",
    "ramfs»/baz/foo",
    "ramfs»/baz/bar",
    "ramfs»/baz/baz",
    "ramfs»/foo/foo",
    "ramfs»/foo/bar",
    "ramfs»/foo/baz",
    "ramfs»/bar/foo",
    "ramfs»/bar/bar",
    "ramfs»/bar/baz",
    "ramfs»/baz/foo",
    "ramfs»/baz/bar",
    "ramfs»/baz/baz",
    "ramfs»/foo/foo/foo",
    "ramfs»/foo/foo/bar",
    "ramfs»/foo/foo/baz",
    "ramfs»/foo/bar/foo",
    "ramfs»/foo/bar/bar",
    "ramfs»/foo/bar/baz",
    "ramfs»/foo/baz/foo",
    "ramfs»/foo/baz/bar",
    "ramfs»/foo/baz/baz",
    "ramfs»/bar/foo/foo",
    "ramfs»/bar/foo/bar",
    "ramfs»/bar/foo/baz",
    "ramfs»/bar/bar/foo",
    "ramfs»/bar/bar/bar",
    "ramfs»/bar/bar/baz",
    "ramfs»/bar/baz/foo",
    "ramfs»/bar/baz/bar",
    "ramfs»/bar/baz/baz",
    "ramfs»/baz/foo/foo",
    "ramfs»/baz/foo/bar",
    "ramfs»/baz/foo/baz",
    "ramfs»/baz/bar/foo",
    "ramfs»/baz/bar/bar",
    "ramfs»/baz/bar/baz",
    "ramfs»/baz/baz/foo",
    "ramfs»/baz/baz/bar",
    "ramfs»/baz/baz/baz",
    "ramfs»/foo/foo",
    "ramfs»/foo/bar",
    "ramfs»/foo/baz",
    "ramfs»/bar/foo",
    "ramfs»/bar/bar",
    "ramfs»/bar/baz",
    "ramfs»/baz/foo",
    "ramfs»/baz/bar",
    "ramfs»/baz/baz",
    "ramfs»/foo/foo/foo",
    "ramfs»/foo/foo/bar",
    "ramfs»/foo/foo/baz",
    "ramfs»/foo/bar/foo",
    "ramfs»/foo/bar/bar",
    "ramfs»/foo/bar/baz",
    "ramfs»/foo/baz/foo",
    "ramfs»/foo/baz/bar",
    "ramfs»/foo/baz/baz",
    "ramfs»/bar/foo/foo",
    "ramfs»/bar/foo/bar",
    "ramfs»/bar/foo/baz",
    "ramfs»/bar/bar/foo",
    "ramfs»/bar/bar/bar",
    "ramfs»/bar/bar/baz",
    "ramfs»/bar/baz/foo",
    "ramfs»/bar/baz/bar",
    "ramfs»/bar/baz/baz",
    "ramfs»/baz/foo/foo",
    "ramfs»/baz/foo/bar",
    "ramfs»/baz/foo/baz",
    "ramfs»/baz/bar/foo",
    "ramfs»/baz/bar/bar",
    "ramfs»/baz/bar/baz",
    "ramfs»/baz/baz/foo",
    "ramfs»/baz/baz/bar",
    "ramfs»/baz/baz/baz",
    "ramfs»/foo/foo/foo",
    "ramfs»/foo/foo/bar",
    "ramfs»/foo/foo/baz",
    "ramfs»/foo/bar/foo",
    "ramfs»/foo/bar/bar",
    "ramfs»/foo/bar/baz",
    "ramfs»/foo/baz/foo",
    "ramfs»/foo/baz/bar",
    "ramfs»/foo/baz/baz",
    "ramfs»/bar/foo/foo",
    "ramfs»/bar/foo/bar",
    "ramfs»/bar/foo/baz",
    "ramfs»/bar/bar/foo",
    "ramfs»/bar/bar/bar",
    "ramfs»/bar/bar/baz",
    "ramfs»/bar/baz/foo",
    "ramfs»/bar/baz/bar",
    "ramfs»/bar/baz/baz",
    "ramfs»/baz/foo/foo",
    "ramfs»/baz/foo/bar",
    "ramfs»/baz/foo/baz",
    "ramfs»/baz/bar/foo",
    "ramfs»/baz/bar/bar",
    "ramfs»/baz/bar/baz",
    "ramfs»/baz/baz/foo",
    "ramfs»/baz/baz/bar",
    "ramfs»/baz/baz/baz",
    "ramfs»/foo/foo/foo/foo",
    "ramfs»/foo/foo/foo/bar",
    "ramfs»/foo/foo/foo/baz",
    "ramfs»/foo/foo/bar/foo",
    "ramfs»/foo/foo/bar/bar",
    "ramfs»/foo/foo/bar/baz",
    "ramfs»/foo/foo/baz/foo",
    "ramfs»/foo/foo/baz/bar",
    "ramfs»/foo/foo/baz/baz",
    "ramfs»/foo/bar/foo/foo",
    "ramfs»/foo/bar/foo/bar",
    "ramfs»/foo/bar/foo/baz",
    "ramfs»/foo/bar/bar/foo",
    "ramfs»/foo/bar/bar/bar",
    "ramfs»/foo/bar/bar/baz",
    "ramfs»/foo/bar/baz/foo",
    "ramfs»/foo/bar/baz/bar",
    "ramfs»/foo/bar/baz/baz",
    "ramfs»/foo/baz/foo/foo",
    "ramfs»/foo/baz/foo/bar",
    "ramfs»/foo/baz/foo/baz",
    "ramfs»/foo/baz/bar/foo",
    "ramfs»/foo/baz/bar/bar",
    "ramfs»/foo/baz/bar/baz",
    "ramfs»/foo/baz/baz/foo",
    "ramfs»/foo/baz/baz/bar",
    "ramfs»/foo/baz/baz/baz",
    "ramfs»/bar/foo/foo/foo",
    "ramfs»/bar/foo/foo/bar",
    "ramfs»/bar/foo/foo/baz",
    "ramfs»/bar/foo/bar/foo",
    "ramfs»/bar/foo/bar/bar",
    "ramfs»/bar/foo/bar/baz",
    "ramfs»/bar/foo/baz/foo",
    "ramfs»/bar/foo/baz/bar",
    "ramfs»/bar/foo/baz/baz",
    "ramfs»/bar/bar/foo/foo",
    "ramfs»/bar/bar/foo/bar",
    "ramfs»/bar/bar/foo/baz",
    "ramfs»/bar/bar/bar/foo",
    "ramfs»/bar/bar/bar/bar",
    "ramfs»/bar/bar/bar/baz",
    "ramfs»/bar/bar/baz/foo",
    "ramfs»/bar/bar/baz/bar",
    "ramfs»/bar/bar/baz/baz",
    "ramfs»/bar/baz/foo/foo",
    "ramfs»/bar/baz/foo/bar",
    "ramfs»/bar/baz/foo/baz",
    "ramfs»/bar/baz/bar/foo",
    "ramfs»/bar/baz/bar/bar",
    "ramfs»/bar/baz/bar/baz",
    "ramfs»/bar/baz/baz/foo",
    "ramfs»/bar/baz/baz/bar",
    "ramfs»/bar/baz/baz/baz",
    "ramfs»/baz/foo/foo/foo",
    "ramfs»/baz/foo/foo/bar",
    "ramfs»/baz/foo/foo/baz",
    "ramfs»/baz/foo/bar/foo",
    "ramfs»/baz/foo/bar/bar",
    "ramfs»/baz/foo/bar/baz",
    "ramfs»/baz/foo/baz/foo",
    "ramfs»/baz/foo/baz/bar",
    "ramfs»/baz/foo/baz/baz",
    "ramfs»/baz/bar/foo/foo",
    "ramfs»/baz/bar/foo/bar",
    "ramfs»/baz/bar/foo/baz",
    "ramfs»/baz/bar/bar/foo",
    "ramfs»/baz/bar/bar/bar",
    "ramfs»/baz/bar/bar/baz",
    "ramfs»/baz/bar/baz/foo",
    "ramfs»/baz/bar/baz/bar",
    "ramfs»/baz/bar/baz/baz",
    "ramfs»/baz/baz/foo/foo",
    "ramfs»/baz/baz/foo/bar",
    "ramfs»/baz/baz/foo/baz",
    "ramfs»/baz/baz/bar/foo",
    "ramfs»/baz/baz/bar/bar",
    "ramfs»/baz/baz/bar/baz",
    "ramfs»/baz/baz/baz/foo",
    "ramfs»/baz/baz/baz/bar",
    "ramfs»/baz/baz/baz/baz",
    "ramfs»/foo/foo",
    "ramfs»/foo/bar",
    "ramfs»/foo/baz",
    "ramfs»/bar/foo",
    "ramfs»/bar/bar",
    "ramfs»/bar/baz",
    "ramfs»/baz/foo",
    "ramfs»/baz/bar",
    "ramfs»/baz/baz",
    "ramfs»/foo/foo/foo",
    "ramfs»/foo/foo/bar",
    "ramfs»/foo/foo/baz",
    "ramfs»/foo/bar/foo",
    "ramfs»/foo/bar/bar",
    "ramfs»/foo/bar/baz",
    "ramfs»/foo/baz/foo",
    "ramfs»/foo/baz/bar",
    "ramfs»/foo/baz/baz",
    "ramfs»/bar/foo/foo",
    "ramfs»/bar/foo/bar",
    "ramfs»/bar/foo/baz",
    "ramfs»/bar/bar/foo",
    "ramfs»/bar/bar/bar",
    "ramfs»/bar/bar/baz",
    "ramfs»/bar/baz/foo",
    "ramfs»/bar/baz/bar",
    "ramfs»/bar/baz/baz",
    "ramfs»/baz/foo/foo",
    "ramfs»/baz/foo/bar",
    "ramfs»/baz/foo/baz",
    "ramfs»/baz/bar/foo",
    "ramfs»/baz/bar/bar",
    "ramfs»/baz/bar/baz",
    "ramfs»/baz/baz/foo",
    "ramfs»/baz/baz/bar",
    "ramfs»/baz/baz/baz",
    "ramfs»/foo/foo/foo",
    "ramfs»/foo/foo/bar",
    "ramfs»/foo/foo/baz",
    "ramfs»/foo/bar/foo",
    "ramfs»/foo/bar/bar",
    "ramfs»/foo/bar/baz",
    "ramfs»/foo/baz/foo",
    "ramfs»/foo/baz/bar",
    "ramfs»/foo/baz/baz",
    "ramfs»/bar/foo/foo",
    "ramfs»/bar/foo/bar",
    "ramfs»/bar/foo/baz",
    "ramfs»/bar/bar/foo",
    "ramfs»/bar/bar/bar",
    "ramfs»/bar/bar/baz",
    "ramfs»/bar/baz/foo",
    "ramfs»/bar/baz/bar",
    "ramfs»/bar/baz/baz",
    "ramfs»/baz/foo/foo",
    "ramfs»/baz/foo/bar",
    "ramfs»/baz/foo/baz",
    "ramfs»/baz/bar/foo",
    "ramfs»/baz/bar/bar",
    "ramfs»/baz/bar/baz",
    "ramfs»/baz/baz/foo",
    "ramfs»/baz/baz/bar",
    "ramfs»/baz/baz/baz",
    "ramfs»/foo/foo/foo/foo",
    "ramfs»/foo/foo/foo/bar",
    "ramfs»/foo/foo/foo/baz",
    "ramfs»/foo/foo/bar/foo",
    "ramfs»/foo/foo/bar/bar",
    "ramfs»/foo/foo/bar/baz",
    "ramfs»/foo/foo/baz/foo",
    "ramfs»/foo/foo/baz/bar",
    "ramfs»/foo/foo/baz/baz",
    "ramfs»/foo/bar/foo/foo",
    "ramfs»/foo/bar/foo/bar",
    "ramfs»/foo/bar/foo/baz",
    "ramfs»/foo/bar/bar/foo",
    "ramfs»/foo/bar/bar/bar",
    "ramfs»/foo/bar/bar/baz",
    "ramfs»/foo/bar/baz/foo",
    "ramfs»/foo/bar/baz/bar",
    "ramfs»/foo/bar/baz/baz",
    "ramfs»/foo/baz/foo/foo",
    "ramfs»/foo/baz/foo/bar",
    "ramfs»/foo/baz/foo/baz",
    "ramfs»/foo/baz/bar/foo",
    "ramfs»/foo/baz/bar/bar",
    "ramfs»/foo/baz/bar/baz",
    "ramfs»/foo/baz/baz/foo",
    "ramfs»/foo/baz/baz/bar",
    "ramfs»/foo/baz/baz/baz",
    "ramfs»/bar/foo/foo/foo",
    "ramfs»/bar/foo/foo/bar",
    "ramfs»/bar/foo/foo/baz",
    "ramfs»/bar/foo/bar/foo",
    "ramfs»/bar/foo/bar/bar",
    "ramfs»/bar/foo/bar/baz",
    "ramfs»/bar/foo/baz/foo",
    "ramfs»/bar/foo/baz/bar",
    "ramfs»/bar/foo/baz/baz",
    "ramfs»/bar/bar/foo/foo",
    "ramfs»/bar/bar/foo/bar",
    "ramfs»/bar/bar/foo/baz",
    "ramfs»/bar/bar/bar/foo",
    "ramfs»/bar/bar/bar/bar",
    "ramfs»/bar/bar/bar/baz",
    "ramfs»/bar/bar/baz/foo",
    "ramfs»/bar/bar/baz/bar",
    "ramfs»/bar/bar/baz/baz",
    "ramfs»/bar/baz/foo/foo",
    "ramfs»/bar/baz/foo/bar",
    "ramfs»/bar/baz/foo/baz",
    "ramfs»/bar/baz/bar/foo",
    "ramfs»/bar/baz/bar/bar",
    "ramfs»/bar/baz/bar/baz",
    "ramfs»/bar/baz/baz/foo",
    "ramfs»/bar/baz/baz/bar",
    "ramfs»/bar/baz/baz/baz",
    "ramfs»/baz/foo/foo/foo",
    "ramfs»/baz/foo/foo/bar",
    "ramfs»/baz/foo/foo/baz",
    "ramfs»/baz/foo/bar/foo",
    "ramfs»/baz/foo/bar/bar",
    "ramfs»/baz/foo/bar/baz",
    "ramfs»/baz/foo/baz/foo",
    "ramfs»/baz/foo/baz/bar",
    "ramfs»/baz/foo/baz/baz",
    "ramfs»/baz/bar/foo/foo",
    "ramfs»/baz/bar/foo/bar",
    "ramfs»/baz/bar/foo/baz",
    "ramfs»/baz/bar/bar/foo",
    "ramfs»/baz/bar/bar/bar",
    "ramfs»/baz/bar/bar/baz",
    "ramfs»/baz/bar/baz/foo",
    "ramfs»/baz/bar/baz/bar",
    "ramfs»/baz/bar/baz/baz",
    "ramfs»/baz/baz/foo/foo",
    "ramfs»/baz/baz/foo/bar",
    "ramfs»/baz/baz/foo/baz",
    "ramfs»/baz/baz/bar/foo",
    "ramfs»/baz/baz/bar/bar",
    "ramfs»/baz/baz/bar/baz",
    "ramfs»/baz/baz/baz/foo",
    "ramfs»/baz/baz/baz/bar",
    "ramfs»/baz/baz/baz/baz",
    "ramfs»/foo/foo/foo",
    "ramfs»/foo/foo/bar",
    "ramfs»/foo/foo/baz",
    "ramfs»/foo/bar/foo",
    "ramfs»/foo/bar/bar",
    "ramfs»/foo/bar/baz",
    "ramfs»/foo/baz/foo",
    "ramfs»/foo/baz/bar",
    "ramfs»/foo/baz/baz",
    "ramfs»/bar/foo/foo",
    "ramfs»/bar/foo/bar",
    "ramfs»/bar/foo/baz",
    "ramfs»/bar/bar/foo",
    "ramfs»/bar/bar/bar",
    "ramfs»/bar/bar/baz",
    "ramfs»/bar/baz/foo",
    "ramfs»/bar/baz/bar",
    "ramfs»/bar/baz/baz",
    "ramfs»/baz/foo/foo",
    "ramfs»/baz/foo/bar",
    "ramfs»/baz/foo/baz",
    "ramfs»/baz/bar/foo",
    "ramfs»/baz/bar/bar",
    "ramfs»/baz/bar/baz",
    "ramfs»/baz/baz/foo",
    "ramfs»/baz/baz/bar",
    "ramfs»/baz/baz/baz",
    "ramfs»/foo/foo/foo/foo",
    "ramfs»/foo/foo/foo/bar",
    "ramfs»/foo/foo/foo/baz",
    "ramfs»/foo/foo/bar/foo",
    "ramfs»/foo/foo/bar/bar",
    "ramfs»/foo/foo/bar/baz",
    "ramfs»/foo/foo/baz/foo",
    "ramfs»/foo/foo/baz/bar",
    "ramfs»/foo/foo/baz/baz",
    "ramfs»/foo/bar/foo/foo",
    "ramfs»/foo/bar/foo/bar",
    "ramfs»/foo/bar/foo/baz",
    "ramfs»/foo/bar/bar/foo",
    "ramfs»/foo/bar/bar/bar",
    "ramfs»/foo/bar/bar/baz",
    "ramfs»/foo/bar/baz/foo",
    "ramfs»/foo/bar/baz/bar",
    "ramfs»/foo/bar/baz/baz",
    "ramfs»/foo/baz/foo/foo",
    "ramfs»/foo/baz/foo/bar",
    "ramfs»/foo/baz/foo/baz",
    "ramfs»/foo/baz/bar/foo",
    "ramfs»/foo/baz/bar/bar",
    "ramfs»/foo/baz/bar/baz",
    "ramfs»/foo/baz/baz/foo",
    "ramfs»/foo/baz/baz/bar",
    "ramfs»/foo/baz/baz/baz",
    "ramfs»/bar/foo/foo/foo",
    "ramfs»/bar/foo/foo/bar",
    "ramfs»/bar/foo/foo/baz",
    "ramfs»/bar/foo/bar/foo",
    "ramfs»/bar/foo/bar/bar",
    "ramfs»/bar/foo/bar/baz",
    "ramfs»/bar/foo/baz/foo",
    "ramfs»/bar/foo/baz/bar",
    "ramfs»/bar/foo/baz/baz",
    "ramfs»/bar/bar/foo/foo",
    "ramfs»/bar/bar/foo/bar",
    "ramfs»/bar/bar/foo/baz",
    "ramfs»/bar/bar/bar/foo",
    "ramfs»/bar/bar/bar/bar",
    "ramfs»/bar/bar/bar/baz",
    "ramfs»/bar/bar/baz/foo",
    "ramfs»/bar/bar/baz/bar",
    "ramfs»/bar/bar/baz/baz",
    "ramfs»/bar/baz/foo/foo",
    "ramfs»/bar/baz/foo/bar",
    "ramfs»/bar/baz/foo/baz",
    "ramfs»/bar/baz/bar/foo",
    "ramfs»/bar/baz/bar/bar",
    "ramfs»/bar/baz/bar/baz",
    "ramfs»/bar/baz/baz/foo",
    "ramfs»/bar/baz/baz/bar",
    "ramfs»/bar/baz/baz/baz",
    "ramfs»/baz/foo/foo/foo",
    "ramfs»/baz/foo/foo/bar",
    "ramfs»/baz/foo/foo/baz",
    "ramfs»/baz/foo/bar/foo",
    "ramfs»/baz/foo/bar/bar",
    "ramfs»/baz/foo/bar/baz",
    "ramfs»/baz/foo/baz/foo",
    "ramfs»/baz/foo/baz/bar",
    "ramfs»/baz/foo/baz/baz",
    "ramfs»/baz/bar/foo/foo",
    "ramfs»/baz/bar/foo/bar",
    "ramfs»/baz/bar/foo/baz",
    "ramfs»/baz/bar/bar/foo",
    "ramfs»/baz/bar/bar/bar",
    "ramfs»/baz/bar/bar/baz",
    "ramfs»/baz/bar/baz/foo",
    "ramfs»/baz/bar/baz/bar",
    "ramfs»/baz/bar/baz/baz",
    "ramfs»/baz/baz/foo/foo",
    "ramfs»/baz/baz/foo/bar",
    "ramfs»/baz/baz/foo/baz",
    "ramfs»/baz/baz/bar/foo",
    "ramfs»/baz/baz/bar/bar",
    "ramfs»/baz/baz/bar/baz",
    "ramfs»/baz/baz/baz/foo",
    "ramfs»/baz/baz/baz/bar",
    "ramfs»/baz/baz/baz/baz",
    "ramfs»/foo/foo/foo/foo",
    "ramfs»/foo/foo/foo/bar",
    "ramfs»/foo/foo/foo/baz",
    "ramfs»/foo/foo/bar/foo",
    "ramfs»/foo/foo/bar/bar",
    "ramfs»/foo/foo/bar/baz",
    "ramfs»/foo/foo/baz/foo",
    "ramfs»/foo/foo/baz/bar",
    "ramfs»/foo/foo/baz/baz",
    "ramfs»/foo/bar/foo/foo",
    "ramfs»/foo/bar/foo/bar",
    "ramfs»/foo/bar/foo/baz",
    "ramfs»/foo/bar/bar/foo",
    "ramfs»/foo/bar/bar/bar",
    "ramfs»/foo/bar/bar/baz",
    "ramfs»/foo/bar/baz/foo",
    "ramfs»/foo/bar/baz/bar",
    "ramfs»/foo/bar/baz/baz",
    "ramfs»/foo/baz/foo/foo",
    "ramfs»/foo/baz/foo/bar",
    "ramfs»/foo/baz/foo/baz",
    "ramfs»/foo/baz/bar/foo",
    "ramfs»/foo/baz/bar/bar",
    "ramfs»/foo/baz/bar/baz",
    "ramfs»/foo/baz/baz/foo",
    "ramfs»/foo/baz/baz/bar",
    "ramfs»/foo/baz/baz/baz",
    "ramfs»/bar/foo/foo/foo",
    "ramfs»/bar/foo/foo/bar",
    "ramfs»/bar/foo/foo/baz",
    "ramfs»/bar/foo/bar/foo",
    "ramfs»/bar/foo/bar/bar",
    "ramfs»/bar/foo/bar/baz",
    "ramfs»/bar/foo/baz/foo",
    "ramfs»/bar/foo/baz/bar",
    "ramfs»/bar/foo/baz/baz",
    "ramfs»/bar/bar/foo/foo",
    "ramfs»/bar/bar/foo/bar",
    "ramfs»/bar/bar/foo/baz",
    "ramfs»/bar/bar/bar/foo",
    "ramfs»/bar/bar/bar/bar",
    "ramfs»/bar/bar/bar/baz",
    "ramfs»/bar/bar/baz/foo",
    "ramfs»/bar/bar/baz/bar",
    "ramfs»/bar/bar/baz/baz",
    "ramfs»/bar/baz/foo/foo",
    "ramfs»/bar/baz/foo/bar",
    "ramfs»/bar/baz/foo/baz",
    "ramfs»/bar/baz/bar/foo",
    "ramfs»/bar/baz/bar/bar",
    "ramfs»/bar/baz/bar/baz",
    "ramfs»/bar/baz/baz/foo",
    "ramfs»/bar/baz/baz/bar",
    "ramfs»/bar/baz/baz/baz",
    "ramfs»/baz/foo/foo/foo",
    "ramfs»/baz/foo/foo/bar",
    "ramfs»/baz/foo/foo/baz",
    "ramfs»/baz/foo/bar/foo",
    "ramfs»/baz/foo/bar/bar",
    "ramfs»/baz/foo/bar/baz",
    "ramfs»/baz/foo/baz/foo",
    "ramfs»/baz/foo/baz/bar",
    "ramfs»/baz/foo/baz/baz",
    "ramfs»/baz/bar/foo/foo",
    "ramfs»/baz/bar/foo/bar",
    "ramfs»/baz/bar/foo/baz",
    "ramfs»/baz/bar/bar/foo",
    "ramfs»/baz/bar/bar/bar",
    "ramfs»/baz/bar/bar/baz",
    "ramfs»/baz/bar/baz/foo",
    "ramfs»/baz/bar/baz/bar",
    "ramfs»/baz/bar/baz/baz",
    "ramfs»/baz/baz/foo/foo",
    "ramfs»/baz/baz/foo/bar",
    "ramfs»/baz/baz/foo/baz",
    "ramfs»/baz/baz/bar/foo",
    "ramfs»/baz/baz/bar/bar",
    "ramfs»/baz/baz/bar/baz",
    "ramfs»/baz/baz/baz/foo",
    "ramfs»/baz/baz/baz/bar",
    "ramfs»/baz/baz/baz/baz",
    "ramfs»/foo/foo/foo/foo/foo",
    "ramfs»/foo/foo/foo/foo/bar",
    "ramfs»/foo/foo/foo/foo/baz",
    "ramfs»/foo/foo/foo/bar/foo",
    "ramfs»/foo/foo/foo/bar/bar",
    "ramfs»/foo/foo/foo/bar/baz",
    "ramfs»/foo/foo/foo/baz/foo",
    "ramfs»/foo/foo/foo/baz/bar",
    "ramfs»/foo/foo/foo/baz/baz",
    "ramfs»/foo/foo/bar/foo/foo",
    "ramfs»/foo/foo/bar/foo/bar",
    "ramfs»/foo/foo/bar/foo/baz",
    "ramfs»/foo/foo/bar/bar/foo",
    "ramfs»/foo/foo/bar/bar/bar",
    "ramfs»/foo/foo/bar/bar/baz",
    "ramfs»/foo/foo/bar/baz/foo",
    "ramfs»/foo/foo/bar/baz/bar",
    "ramfs»/foo/foo/bar/baz/baz",
    "ramfs»/foo/foo/baz/foo/foo",
    "ramfs»/foo/foo/baz/foo/bar",
    "ramfs»/foo/foo/baz/foo/baz",
    "ramfs»/foo/foo/baz/bar/foo",
    "ramfs»/foo/foo/baz/bar/bar",
    "ramfs»/foo/foo/baz/bar/baz",
    "ramfs»/foo/foo/baz/baz/foo",
    "ramfs»/foo/foo/baz/baz/bar",
    "ramfs»/foo/foo/baz/baz/baz",
    "ramfs»/foo/bar/foo/foo/foo",
    "ramfs»/foo/bar/foo/foo/bar",
    "ramfs»/foo/bar/foo/foo/baz",
    "ramfs»/foo/bar/foo/bar/foo",
    "ramfs»/foo/bar/foo/bar/bar",
    "ramfs»/foo/bar/foo/bar/baz",
    "ramfs»/foo/bar/foo/baz/foo",
    "ramfs»/foo/bar/foo/baz/bar",
    "ramfs»/foo/bar/foo/baz/baz",
    "ramfs»/foo/bar/bar/foo/foo",
    "ramfs»/foo/bar/bar/foo/bar",
    "ramfs»/foo/bar/bar/foo/baz",
    "ramfs»/foo/bar/bar/bar/foo",
    "ramfs»/foo/bar/bar/bar/bar",
    "ramfs»/foo/bar/bar/bar/baz",
    "ramfs»/foo/bar/bar/baz/foo",
    "ramfs»/foo/bar/bar/baz/bar",
    "ramfs»/foo/bar/bar/baz/baz",
    "ramfs»/foo/bar/baz/foo/foo",
    "ramfs»/foo/bar/baz/foo/bar",
    "ramfs»/foo/bar/baz/foo/baz",
    "ramfs»/foo/bar/baz/bar/foo",
    "ramfs»/foo/bar/baz/bar/bar",
    "ramfs»/foo/bar/baz/bar/baz",
    "ramfs»/foo/bar/baz/baz/foo",
    "ramfs»/foo/bar/baz/baz/bar",
    "ramfs»/foo/bar/baz/baz/baz",
    "ramfs»/foo/baz/foo/foo/foo",
    "ramfs»/foo/baz/foo/foo/bar",
    "ramfs»/foo/baz/foo/foo/baz",
    "ramfs»/foo/baz/foo/bar/foo",
    "ramfs»/foo/baz/foo/bar/bar",
    "ramfs»/foo/baz/foo/bar/baz",
    "ramfs»/foo/baz/foo/baz/foo",
    "ramfs»/foo/baz/foo/baz/bar",
    "ramfs»/foo/baz/foo/baz/baz",
    "ramfs»/foo/baz/bar/foo/foo",
    "ramfs»/foo/baz/bar/foo/bar",
    "ramfs»/foo/baz/bar/foo/baz",
    "ramfs»/foo/baz/bar/bar/foo",
    "ramfs»/foo/baz/bar/bar/bar",
    "ramfs»/foo/baz/bar/bar/baz",
    "ramfs»/foo/baz/bar/baz/foo",
    "ramfs»/foo/baz/bar/baz/bar",
    "ramfs»/foo/baz/bar/baz/baz",
    "ramfs»/foo/baz/baz/foo/foo",
    "ramfs»/foo/baz/baz/foo/bar",
    "ramfs»/foo/baz/baz/foo/baz",
    "ramfs»/foo/baz/baz/bar/foo",
    "ramfs»/foo/baz/baz/bar/bar",
    "ramfs»/foo/baz/baz/bar/baz",
    "ramfs»/foo/baz/baz/baz/foo",
    "ramfs»/foo/baz/baz/baz/bar",
    "ramfs»/foo/baz/baz/baz/baz",
    "ramfs»/bar/foo/foo/foo/foo",
    "ramfs»/bar/foo/foo/foo/bar",
    "ramfs»/bar/foo/foo/foo/baz",
    "ramfs»/bar/foo/foo/bar/foo",
    "ramfs»/bar/foo/foo/bar/bar",
    "ramfs»/bar/foo/foo/bar/baz",
    "ramfs»/bar/foo/foo/baz/foo",
    "ramfs»/bar/foo/foo/baz/bar",
    "ramfs»/bar/foo/foo/baz/baz",
    "ramfs»/bar/foo/bar/foo/foo",
    "ramfs»/bar/foo/bar/foo/bar",
    "ramfs»/bar/foo/bar/foo/baz",
    "ramfs»/bar/foo/bar/bar/foo",
    "ramfs»/bar/foo/bar/bar/bar",
    "ramfs»/bar/foo/bar/bar/baz",
    "ramfs»/bar/foo/bar/baz/foo",
    "ramfs»/bar/foo/bar/baz/bar",
    "ramfs»/bar/foo/bar/baz/baz",
    "ramfs»/bar/foo/baz/foo/foo",
    "ramfs»/bar/foo/baz/foo/bar",
    "ramfs»/bar/foo/baz/foo/baz",
    "ramfs»/bar/foo/baz/bar/foo",
    "ramfs»/bar/foo/baz/bar/bar",
    "ramfs»/bar/foo/baz/bar/baz",
    "ramfs»/bar/foo/baz/baz/foo",
    "ramfs»/bar/foo/baz/baz/bar",
    "ramfs»/bar/foo/baz/baz/baz",
    "ramfs»/bar/bar/foo/foo/foo",
    "ramfs»/bar/bar/foo/foo/bar",
    "ramfs»/bar/bar/foo/foo/baz",
    "ramfs»/bar/bar/foo/bar/foo",
    "ramfs»/bar/bar/foo/bar/bar",
    "ramfs»/bar/bar/foo/bar/baz",
    "ramfs»/bar/bar/foo/baz/foo",
    "ramfs»/bar/bar/foo/baz/bar",
    "ramfs»/bar/bar/foo/baz/baz",
    "ramfs»/bar/bar/bar/foo/foo",
    "ramfs»/bar/bar/bar/foo/bar",
    "ramfs»/bar/bar/bar/foo/baz",
    "ramfs»/bar/bar/bar/bar/foo",
    "ramfs»/bar/bar/bar/bar/bar",
    "ramfs»/bar/bar/bar/bar/baz",
    "ramfs»/bar/bar/bar/baz/foo",
    "ramfs»/bar/bar/bar/baz/bar",
    "ramfs»/bar/bar/bar/baz/baz",
    "ramfs»/bar/bar/baz/foo/foo",
    "ramfs»/bar/bar/baz/foo/bar",
    "ramfs»/bar/bar/baz/foo/baz",
    "ramfs»/bar/bar/baz/bar/foo",
    "ramfs»/bar/bar/baz/bar/bar",
    "ramfs»/bar/bar/baz/bar/baz",
    "ramfs»/bar/bar/baz/baz/foo",
    "ramfs»/bar/bar/baz/baz/bar",
    "ramfs»/bar/bar/baz/baz/baz",
    "ramfs»/bar/baz/foo/foo/foo",
    "ramfs»/bar/baz/foo/foo/bar",
    "ramfs»/bar/baz/foo/foo/baz",
    "ramfs»/bar/baz/foo/bar/foo",
    "ramfs»/bar/baz/foo/bar/bar",
    "ramfs»/bar/baz/foo/bar/baz",
    "ramfs»/bar/baz/foo/baz/foo",
    "ramfs»/bar/baz/foo/baz/bar",
    "ramfs»/bar/baz/foo/baz/baz",
    "ramfs»/bar/baz/bar/foo/foo",
    "ramfs»/bar/baz/bar/foo/bar",
    "ramfs»/bar/baz/bar/foo/baz",
    "ramfs»/bar/baz/bar/bar/foo",
    "ramfs»/bar/baz/bar/bar/bar",
    "ramfs»/bar/baz/bar/bar/baz",
    "ramfs»/bar/baz/bar/baz/foo",
    "ramfs»/bar/baz/bar/baz/bar",
    "ramfs»/bar/baz/bar/baz/baz",
    "ramfs»/bar/baz/baz/foo/foo",
    "ramfs»/bar/baz/baz/foo/bar",
    "ramfs»/bar/baz/baz/foo/baz",
    "ramfs»/bar/baz/baz/bar/foo",
    "ramfs»/bar/baz/baz/bar/bar",
    "ramfs»/bar/baz/baz/bar/baz",
    "ramfs»/bar/baz/baz/baz/foo",
    "ramfs»/bar/baz/baz/baz/bar",
    "ramfs»/bar/baz/baz/baz/baz",
    "ramfs»/baz/foo/foo/foo/foo",
    "ramfs»/baz/foo/foo/foo/bar",
    "ramfs»/baz/foo/foo/foo/baz",
    "ramfs»/baz/foo/foo/bar/foo",
    "ramfs»/baz/foo/foo/bar/bar",
    "ramfs»/baz/foo/foo/bar/baz",
    "ramfs»/baz/foo/foo/baz/foo",
    "ramfs»/baz/foo/foo/baz/bar",
    "ramfs»/baz/foo/foo/baz/baz",
    "ramfs»/baz/foo/bar/foo/foo",
    "ramfs»/baz/foo/bar/foo/bar",
    "ramfs»/baz/foo/bar/foo/baz",
    "ramfs»/baz/foo/bar/bar/foo",
    "ramfs»/baz/foo/bar/bar/bar",
    "ramfs»/baz/foo/bar/bar/baz",
    "ramfs»/baz/foo/bar/baz/foo",
    "ramfs»/baz/foo/bar/baz/bar",
    "ramfs»/baz/foo/bar/baz/baz",
    "ramfs»/baz/foo/baz/foo/foo",
    "ramfs»/baz/foo/baz/foo/bar",
    "ramfs»/baz/foo/baz/foo/baz",
    "ramfs»/baz/foo/baz/bar/foo",
    "ramfs»/baz/foo/baz/bar/bar",
    "ramfs»/baz/foo/baz/bar/baz",
    "ramfs»/baz/foo/baz/baz/foo",
    "ramfs»/baz/foo/baz/baz/bar",
    "ramfs»/baz/foo/baz/baz/baz",
    "ramfs»/baz/bar/foo/foo/foo",
    "ramfs»/baz/bar/foo/foo/bar",
    "ramfs»/baz/bar/foo/foo/baz",
    "ramfs»/baz/bar/foo/bar/foo",
    "ramfs»/baz/bar/foo/bar/bar",
    "ramfs»/baz/bar/foo/bar/baz",
    "ramfs»/baz/bar/foo/baz/foo",
    "ramfs»/baz/bar/foo/baz/bar",
    "ramfs»/baz/bar/foo/baz/baz",
    "ramfs»/baz/bar/bar/foo/foo",
    "ramfs»/baz/bar/bar/foo/bar",
    "ramfs»/baz/bar/bar/foo/baz",
    "ramfs»/baz/bar/bar/bar/foo",
    "ramfs»/baz/bar/bar/bar/bar",
    "ramfs»/baz/bar/bar/bar/baz",
    "ramfs»/baz/bar/bar/baz/foo",
    "ramfs»/baz/bar/bar/baz/bar",
    "ramfs»/baz/bar/bar/baz/baz",
    "ramfs»/baz/bar/baz/foo/foo",
    "ramfs»/baz/bar/baz/foo/bar",
    "ramfs»/baz/bar/baz/foo/baz",
    "ramfs»/baz/bar/baz/bar/foo",
    "ramfs»/baz/bar/baz/bar/bar",
    "ramfs»/baz/bar/baz/bar/baz",
    "ramfs»/baz/bar/baz/baz/foo",
    "ramfs»/baz/bar/baz/baz/bar",
    "ramfs»/baz/bar/baz/baz/baz",
    "ramfs»/baz/baz/foo/foo/foo",
    "ramfs»/baz/baz/foo/foo/bar",
    "ramfs»/baz/baz/foo/foo/baz",
    "ramfs»/baz/baz/foo/bar/foo",
    "ramfs»/baz/baz/foo/bar/bar",
    "ramfs»/baz/baz/foo/bar/baz",
    "ramfs»/baz/baz/foo/baz/foo",
    "ramfs»/baz/baz/foo/baz/bar",
    "ramfs»/baz/baz/foo/baz/baz",
    "ramfs»/baz/baz/bar/foo/foo",
    "ramfs»/baz/baz/bar/foo/bar",
    "ramfs»/baz/baz/bar/foo/baz",
    "ramfs»/baz/baz/bar/bar/foo",
    "ramfs»/baz/baz/bar/bar/bar",
    "ramfs»/baz/baz/bar/bar/baz",
    "ramfs»/baz/baz/bar/baz/foo",
    "ramfs»/baz/baz/bar/baz/bar",
    "ramfs»/baz/baz/bar/baz/baz",
    "ramfs»/baz/baz/baz/foo/foo",
    "ramfs»/baz/baz/baz/foo/bar",
    "ramfs»/baz/baz/baz/foo/baz",
    "ramfs»/baz/baz/baz/bar/foo",
    "ramfs»/baz/baz/baz/bar/bar",
    "ramfs»/baz/baz/baz/bar/baz",
    "ramfs»/baz/baz/baz/baz/foo",
    "ramfs»/baz/baz/baz/baz/bar",
    "ramfs»/baz/baz/baz/baz/baz",
};

static const char *randomPath()
{
    return paths[rand() % (sizeof(paths) / sizeof(paths[0]))];
}

static std::unique_ptr<RamFs> prepareVFS(VFS &vfs)
{
    srand(time(0));

    std::unique_ptr<RamFs> ramfs = std::make_unique<RamFs>();
    ramfs->initialise(nullptr);

    vfs.addAlias(ramfs.get(), g_Alias);

    // Add a bunch of directories for lookups
    for (auto p : paths)
    {
        vfs.createDirectory(p, 0777);
    }

    return ramfs;
}

static void BM_VFSShallowDirectoryTraverse(benchmark::State &state)
{
    VFS vfs;
    auto ramfs = prepareVFS(vfs);

    CALLGRIND_START_INSTRUMENTATION;
    while (state.KeepRunning())
    {
        benchmark::DoNotOptimize(vfs.find(g_ShallowPath));
    }
    CALLGRIND_STOP_INSTRUMENTATION;

    state.SetItemsProcessed(int64_t(state.iterations()));

    vfs.removeAllAliases(ramfs.get(), false);
}

static void BM_VFSMediumDirectoryTraverse(benchmark::State &state)
{
    VFS vfs;
    auto ramfs = prepareVFS(vfs);

    CALLGRIND_START_INSTRUMENTATION;
    while (state.KeepRunning())
    {
        benchmark::DoNotOptimize(vfs.find(g_MiddlePath));
    }
    CALLGRIND_STOP_INSTRUMENTATION;

    state.SetItemsProcessed(int64_t(state.iterations()));

    vfs.removeAllAliases(ramfs.get(), false);
}

static void BM_VFSDeepDirectoryTraverse(benchmark::State &state)
{
    VFS vfs;
    auto ramfs = prepareVFS(vfs);

    CALLGRIND_START_INSTRUMENTATION;
    while (state.KeepRunning())
    {
        benchmark::DoNotOptimize(vfs.find(g_DeepPath));
    }
    CALLGRIND_STOP_INSTRUMENTATION;

    state.SetItemsProcessed(int64_t(state.iterations()));

    vfs.removeAllAliases(ramfs.get(), false);
}

static void BM_VFSRandomDirectoryTraverse(benchmark::State &state)
{
    VFS vfs;
    auto ramfs = prepareVFS(vfs);

    CALLGRIND_START_INSTRUMENTATION;
    while (state.KeepRunning())
    {
        benchmark::DoNotOptimize(vfs.find(randomPath()));
    }
    CALLGRIND_STOP_INSTRUMENTATION;

    state.SetItemsProcessed(int64_t(state.iterations()));

    vfs.removeAllAliases(ramfs.get(), false);
}

static void BM_VFSShallowDirectoryTraverseNoFs(benchmark::State &state)
{
    VFS vfs;
    auto ramfs = prepareVFS(vfs);

    CALLGRIND_START_INSTRUMENTATION;
    while (state.KeepRunning())
    {
        benchmark::DoNotOptimize(vfs.find(g_ShallowPathNoFs, ramfs->getRoot()));
    }
    CALLGRIND_STOP_INSTRUMENTATION;

    state.SetItemsProcessed(int64_t(state.iterations()));

    vfs.removeAllAliases(ramfs.get(), false);
}

static void BM_VFSMediumDirectoryTraverseNoFs(benchmark::State &state)
{
    VFS vfs;
    auto ramfs = prepareVFS(vfs);

    CALLGRIND_START_INSTRUMENTATION;
    while (state.KeepRunning())
    {
        benchmark::DoNotOptimize(vfs.find(g_MiddlePathNoFs, ramfs->getRoot()));
    }
    CALLGRIND_STOP_INSTRUMENTATION;

    state.SetItemsProcessed(int64_t(state.iterations()));

    vfs.removeAllAliases(ramfs.get(), false);
}

static void BM_VFSDeepDirectoryTraverseNoFs(benchmark::State &state)
{
    VFS vfs;
    auto ramfs = prepareVFS(vfs);

    CALLGRIND_START_INSTRUMENTATION;
    while (state.KeepRunning())
    {
        benchmark::DoNotOptimize(vfs.find(g_DeepPathNoFs, ramfs->getRoot()));
    }
    CALLGRIND_STOP_INSTRUMENTATION;

    state.SetItemsProcessed(int64_t(state.iterations()));

    vfs.removeAllAliases(ramfs.get(), false);
}

static void BM_VFSRandomDirectoryTraverseNoFs(benchmark::State &state)
{
    VFS vfs;
    auto ramfs = prepareVFS(vfs);

    CALLGRIND_START_INSTRUMENTATION;
    while (state.KeepRunning())
    {
        benchmark::DoNotOptimize(vfs.find(randomPath() + 7, ramfs->getRoot()));
    }
    CALLGRIND_STOP_INSTRUMENTATION;

    state.SetItemsProcessed(int64_t(state.iterations()));

    vfs.removeAllAliases(ramfs.get(), false);
}

BENCHMARK(BM_VFSDeepDirectoryTraverse);
BENCHMARK(BM_VFSMediumDirectoryTraverse);
BENCHMARK(BM_VFSShallowDirectoryTraverse);
BENCHMARK(BM_VFSRandomDirectoryTraverse);

BENCHMARK(BM_VFSDeepDirectoryTraverseNoFs);
BENCHMARK(BM_VFSMediumDirectoryTraverseNoFs);
BENCHMARK(BM_VFSShallowDirectoryTraverseNoFs);
BENCHMARK(BM_VFSRandomDirectoryTraverseNoFs);
