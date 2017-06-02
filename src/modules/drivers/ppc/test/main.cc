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

#include "../../../system/kernel/core/BootIO.h"
#include "pedigree/kernel/Log.h"
#include <Module.h>
#include "pedigree/kernel/utilities/StaticString.h"

extern BootIO bootIO;

bool entry()
{
    HugeStaticString str;
    str += "Module is teh loadzor!";
    bootIO.write(str, BootIO::Blue, BootIO::White);
    return false;
}

void ex()
{
    HugeStaticString str;
    str += "Module is teh exit0r!";
    bootIO.write(str, BootIO::Blue, BootIO::White);
}

const char *g_pModuleName = "test";
ModuleEntry g_pModuleEntry = &entry;
ModuleExit g_pModuleExit = &ex;
