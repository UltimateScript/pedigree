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

#include <compiler.h>
#include <Log.h>
#include <Module.h>
#include <vfs/VFS.h>
#include <processor/Processor.h>
#include <process/Scheduler.h>
#include <subsys/posix/PosixSubsystem.h>
#include <subsys/posix/PosixProcess.h>
#include <core/BootIO.h>
#include <linker/DynamicLinker.h>
#include <users/UserManager.h>

static void error(const char *s)
{
    extern BootIO bootIO;
    static HugeStaticString str;
    str += s;
    str += "\n";
    bootIO.write(str, BootIO::Red, BootIO::Black);
    str.clear();
}

static int init_stage2(void *param)
{
#if defined(HOSTED) && defined(HAS_ADDRESS_SANITIZER)
    extern void system_reset();
    NOTICE("Note: ASAN build, so triggering a restart now.");
    system_reset();
    return;
#endif

    String init_path("root»/applications/init");
    if (!VFS::instance().find(init_path))
    {
        WARNING("Did not find " << init_path << ", trying for a Linux userspace...");
        init_path = "root»/sbin/init";
    }

    List<SharedPointer<String>> argv, env;
    argv.pushBack(SharedPointer<String>::allocate(init_path));
    argv.pushBack(SharedPointer<String>::allocate("1"));  // runlevel 1

    Process *pProcess = Processor::information().getCurrentThread()->getParent();
    if (!pProcess->getSubsystem()->invoke(init_path, argv, env))
    {
        error("failed to load init program");
    }

    Process::setInit(pProcess);

    return 0;
}

static bool init()
{
#ifdef THREADS
    // Create a new process for the init process.
    Process *pProcess = new PosixProcess(Processor::information().getCurrentThread()->getParent());
    pProcess->setUser(UserManager::instance().getUser(0));
    pProcess->setGroup(UserManager::instance().getUser(0)->getDefaultGroup());
    pProcess->setEffectiveUser(pProcess->getUser());
    pProcess->setEffectiveGroup(pProcess->getGroup());

    pProcess->description().clear();
    pProcess->description().append("init");

    pProcess->setCwd(VFS::instance().find(String("root»/")));
    pProcess->setCtty(0);

    PosixSubsystem *pSubsystem = new PosixSubsystem;
    pProcess->setSubsystem(pSubsystem);

    // add an empty stdout, stdin
    File *pNull = VFS::instance().find(String("dev»/null"));
    if (!pNull)
    {
        error("dev»/null does not exist");
    }

    FileDescriptor *stdinDescriptor = new FileDescriptor(pNull, 0, 0, 0, 0);
    FileDescriptor *stdoutDescriptor = new FileDescriptor(pNull, 0, 1, 0, 0);

    pSubsystem->addFileDescriptor(0, stdinDescriptor);
    pSubsystem->addFileDescriptor(1, stdoutDescriptor);

    Thread *pThread = new Thread(pProcess, init_stage2, 0);
    pThread->detach();
#endif

    return true;
}

static void destroy()
{
}

#if defined(X86_COMMON)
#define __MOD_DEPS "vfs", "posix", "linker", "users"
#define __MOD_DEPS_OPT "gfx-deps", "mountroot"
#else
#define __MOD_DEPS "vfs", "posix", "linker", "users"
#define __MOD_DEPS_OPT "mountroot"
#endif
MODULE_INFO("init", &init, &destroy, __MOD_DEPS);
#ifdef __MOD_DEPS_OPT
MODULE_OPTIONAL_DEPENDS(__MOD_DEPS_OPT);
#endif
