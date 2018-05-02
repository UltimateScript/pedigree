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

#include "pedigree/native/native-protocol.h"
#include "pedigree/native/native-syscall.h"
#include "pedigree/native/nativeSyscallNumbers.h"

int _syscall(Object *pObject, size_t nOp)
{
    return -1;
    /*
    if(!pObject)
        return -1;
    uintptr_t buff; size_t buffLength;
    pObject->serialise(&buff, buffLength);
    if(!buff)
        return -1;
    syscall3(NATIVE_USERSPACE_TO_KERNEL, (uintptr_t) pObject, buffLength, nOp);
    return 0;
    */
}

void register_object(Object *pObject)
{
    long result = syscall2(
        NATIVE_REGISTER_OBJECT, pObject->guid(),
        reinterpret_cast<uintptr_t>(pObject));
    if (result == 0)
        throw result;
}

void unregister_object(Object *pObject)
{
    syscall1(NATIVE_UNREGISTER_OBJECT, reinterpret_cast<uintptr_t>(pObject));
}

ReturnState
native_call(Object *pObject, uint64_t subid, void *params, size_t params_size)
{
    ReturnState state;
    syscall5(
        NATIVE_CALL, reinterpret_cast<uintptr_t>(pObject), subid,
        reinterpret_cast<uintptr_t>(params), params_size,
        reinterpret_cast<uintptr_t>(&state));
    return state;
}
