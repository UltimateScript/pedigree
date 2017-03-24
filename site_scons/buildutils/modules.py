'''
Copyright (c) 2008-2014, Pedigree Developers

Please see the CONTRIB file in the root of the source tree for a full
list of contributors.

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
'''

import os


def buildModule(env, stripped_target, target, sources):
    module_env = env.Clone()

    if env['clang_cross']:
        module_env['LINKFLAGS'] = env['CLANG_BASE_LINKFLAGS']
    else:
        # Wipe out the kernel's link flags so we can substitute our own.
        module_env['LINKFLAGS'] = []

    for key in ('CC', 'CXX', 'LINK', 'CFLAGS', 'CCFLAGS', 'CXXFLAGS'):
        module_env[key] = module_env['TARGET_%s' % key]

    if "STATIC_DRIVERS" in env['CPPDEFINES']:
        module_env['LSCRIPT'] = module_env.File("#src/modules/link_static.ld")
    else:
        module_env['LSCRIPT'] = module_env.File("#src/modules/link.ld")

    extra_linkflags = module_env.get('MODULE_LINKFLAGS', [])

    module_env.MergeFlags({
        'LINKFLAGS': ['-nodefaultlibs', '-nostartfiles', '-r', '-Wl,-T,$LSCRIPT'] +
            extra_linkflags,
    })

    libmodule_dir = os.path.join(module_env['BUILDDIR'], 'modules')
    libmodule_path = os.path.join(libmodule_dir, 'libmodule.a')

    module_env.MergeFlags({
        'LIBS': ['module', 'gcc'],
        'LIBPATH': [libmodule_dir],
        'CPPDEFINES': ['IN_PEDIGREE_KERNEL'],  # modules are in-kernel
    })

    module_env.Depends(target, libmodule_path)
    module_env.Depends(stripped_target, libmodule_path)

    if env['clang_cross'] and env['clang_analyse']:
        return module_env.Program(stripped_target, sources)

    intermediate = module_env.Program(target, sources)
    module_env.Depends(target, module_env['LSCRIPT'])
    module_env.Depends(target, libmodule_path)
    return module_env.Command(stripped_target, intermediate,
                              action='$STRIP -d --strip-unneeded -o $TARGET '
                                     '$SOURCE')
