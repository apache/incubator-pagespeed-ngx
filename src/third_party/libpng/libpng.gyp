# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
        # Link to system .so since we already use it due to GTK.
        'use_system_libpng%': 1,
      }, {  # OS!="linux" and OS!="freebsd" and OS!="openbsd"
        'use_system_libpng%': 0,
      }],
    ],
  },
  'conditions': [
    ['use_system_libpng==0', {
      'targets': [
        {
          'target_name': 'libpng',
          'type': '<(component)',
          'dependencies': [
            '../zlib/zlib.gyp:zlib',
          ],
          'msvs_guid': 'C564F145-9172-42C3-BFCB-6014CA97DBCD',
          'sources': [
            'png.c',
            'png.h',
            'pngconf.h',
            'pngerror.c',
            'pnggccrd.c',
            'pngget.c',
            'pngmem.c',
            'pngpread.c',
            'pngread.c',
            'pngrio.c',
            'pngrtran.c',
            'pngrutil.c',
            'pngset.c',
            'pngtrans.c',
            'pngusr.h',
            'pngvcrd.c',
            'pngwio.c',
            'pngwrite.c',
            'pngwtran.c',
            'pngwutil.c',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '.',
            ],
            'defines': [
              # We end up including setjmp.h directly, but libpng
              # doesn't like that. This define tells libpng to not
              # complain about our inclusion of setjmp.h.
              'PNG_SKIP_SETJMP_CHECK',
            ],
          },
          'export_dependent_settings': [
            '../zlib/zlib.gyp:zlib',
          ],
          'conditions': [
            ['OS!="win"', {'product_name': 'png'}],
            ['OS=="win" and component=="shared_library"', {
              'defines': [
                'PNG_BUILD_DLL',
                'PNG_NO_MODULEDEF',
              ],
              'direct_dependent_settings': {
                'defines': [
                  'PNG_USE_DLL',
                ],
              },
            }],
          ],
        },
      ]
    }, {
      'conditions': [
        ['sysroot!=""', {
          'variables': {
            'pkg-config': '../../build/linux/pkg-config-wrapper "<(sysroot)"',
          },
        }, {
          'variables': {
            'pkg-config': 'pkg-config'
          },
        }],
      ],
      'targets': [
        {
          'target_name': 'libpng',
          'type': 'none',
          'dependencies': [
            '../zlib/zlib.gyp:zlib',
          ],
          'variables': {
            # Quoth libpagespeed's libpng.gyp:
            # "The PNG_FREE_ME_SUPPORTED define was dropped in libpng
            #  1.4.0beta78, with its behavior becoming the default
            #  behavior."
            #
            # Hence, we define it ourselves for version >= 1.4.0 so that
            # libpagespeed's code (which checks PNG_FREE_ME_SUPPORTED for
            # compatibility with earlier versions) will run with both earlier
            # and later versions of libpng.
            #
            # This detects the version and sets the variable to non-zero for
            # pre-1.4 versions.
            'png_free_me_suported_define_in_libpng' :
              '<!(<(pkg-config) --atleast-version=1.4.0 libpng; echo $?)'
          },
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags libpng)',
            ],
            'defines+': [
              'USE_SYSTEM_LIBPNG',
              'DBG=<(png_free_me_suported_define_in_libpng)',

              # We end up including setjmp.h directly, but libpng
              # doesn't like that. This define tells libpng to not
              # complain about our inclusion of setjmp.h.
              'PNG_SKIP_SETJMP_CHECK',
            ],
          },
          'conditions': [
            ['<(png_free_me_suported_define_in_libpng)==0', {
              'direct_dependent_settings': {
                'defines+': [
                  'PNG_FREE_ME_SUPPORTED',
                ],
              }
            }],
          ],
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other libpng)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l libpng)',
            ],
          },
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
