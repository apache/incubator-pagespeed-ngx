# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
      # Patch out the ifdef checking for x86_64 in poly1305.c
      # Old versions of GCC can't use _int128, so we need this.
      {
        'target_name': 'patch_openssl',
        'type': 'none',
        'actions': [
          {
            'action_name': 'remove_ifdefs',
                'variables': {
                  'openssl_parent': '<(DEPTH)/third_party/boringssl',
                  'openssl_root': '<(openssl_parent)/src',
                },
            'inputs': [
                  '<(openssl_root)/crypto/poly1305/poly1305.c',
            ],
            'outputs': [
                  '<(DEPTH)/net/instaweb/genfiles/openssl/poly1305.patch.c',
            ],
            'action': [
              # The guard lines on this file are
              # #if defined(OPENSSL_WINDOWS) || !defined(OPENSSL_X86_64)
              # #endif  /* OPENSSL_WINDOWS || !OPENSSL_X86_64 */
              # These are the only locations where OPENSSL_WINDOWS is used in
              # the file.
              'bash',
              '-c',
              'cat <(openssl_root)/crypto/poly1305/poly1305.c'
              '| sed /OPENSSL_WINDOWS/d > '
              '<(DEPTH)/net/instaweb/genfiles/openssl/poly1305.patch.c',
            ],
            'message': 'Patching for use with gcc 4.6 or lower',
          },
        ]
    },
    {
      'target_name': 'openssl',
      'type': '<(component)',
      'includes': [
        'openssl.gypi',
      ],
       'variables': {
        'openssl_parent': '<(DEPTH)/third_party/boringssl',
        'openssl_root': '<(openssl_parent)/src',
        'openssl_include_dirs': [
          '<(openssl_parent)',
          '<(openssl_root)',
          '<(openssl_root)/include/openssl',
          '<(openssl_root)/crypto',
          '<(openssl_root)/crypto/asn1',
          '<(openssl_root)/crypto/evp',
          '<(openssl_root)/crypto/modes',
        ],
      },
      'sources': [
        '<@(boringssl_crypto_sources)',
        '<@(boringssl_ssl_sources)',
      ],
      'cflags': [
        '-D_POSIX_C_SOURCE=200112L',
        '-std=c99',
      ],
      'defines': [ 'BORINGSSL_IMPLEMENTATION',
                   'BORINGSSL_NO_STATIC_INITIALIZER',
      ],
      'conditions': [
        ['component == "shared_library"', {
          'defines': [
            'BORINGSSL_SHARED_LIBRARY',
          ],
        }],
        ['target_arch == "arm"', {
          'sources': [ '<@(boringssl_linux_arm_sources)' ],
        }],
        ['target_arch == "ia32"', {
          'conditions': [
            ['OS == "mac"', {
              'sources': [ '<@(boringssl_mac_x86_sources)' ],
            }],
            ['OS == "linux" or OS == "android"', {
              'sources': [ '<@(boringssl_linux_x86_sources)' ],
            }],
            ['OS != "mac" and OS != "linux" and OS != "android"', {
              'defines': [ 'OPENSSL_NO_ASM' ],
            }],
          ]
        }],
        ['target_arch == "x64"', {
          'conditions': [
            ['OS == "mac"', {
              'sources': [ '<@(boringssl_mac_x86_64_sources)' ],
            }],
            ['OS == "linux" or OS == "android"', {
              'sources': [ '<@(boringssl_linux_x86_64_sources)' ],
            }],
            ['OS == "win"', {
              'sources': [ '<@(boringssl_win_x86_64_sources)' ],
            }],
            ['OS != "mac" and OS != "linux" and OS != "win" and OS != "android"', {
              'defines': [ 'OPENSSL_NO_ASM' ],
            }],
            ['<(gcc_version) < 46', {
              'dependencies': ['patch_openssl',],
              # Older versions of gcc don't recognize __int128 type.
              'sources': ['<(DEPTH)/net/instaweb/genfiles/openssl/poly1305.patch.c'],
              'sources!': ['<(openssl_root)/crypto/poly1305/poly1305_vec.c'],
            }],
          ]
        }],
        ['target_arch != "arm" and target_arch != "ia32" and target_arch != "x64"', {
          'defines': [ 'OPENSSL_NO_ASM' ],
        }],
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/boringssl/src/include',
        # This is for arm_arch.h, which is needed by some asm files. Since the
        # asm files are generated and kept in a different directory, they
        # cannot use relative paths to find this file.
        '<(DEPTH)/third_party/boringssl/src/crypto',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/boringssl/src/include',
        ],
        'defines' : [
          'OPENSSL_SMALL',
        ],
        'conditions': [
          ['component == "shared_library"', {
            'defines': [
              'BORINGSSL_SHARED_LIBRARY',
            ],
          }],
        ],
      },
    },
  ],
}

