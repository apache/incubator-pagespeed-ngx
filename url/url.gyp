# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'chromium_root': '<(DEPTH)/third_party/chromium/src',
  },
  'includes': [
    'url_srcs.gypi',
  ],
  'targets': [
    {
      # Note, this target_name cannot be 'url', because that will generate
      # 'url.dll' for a Windows component build, and that will confuse Windows,
      # which has a system DLL with the same name.
      'target_name': 'url_lib',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(chromium_root)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        # '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
      ],
      'sources': [
        '<@(gurl_sources)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'all_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/url'
        ]
      },
      'include_dirs': [
        '<(DEPTH)/url',
      ],
      'defines': [
        'URL_IMPLEMENTATION',
        # This is a bit of paranoia to make sure this can't load its own copy
        # of string16.h rather than chromium one (base/strings/string16).
        'BASE_STRING16_H_',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    # {
    #   'target_name': 'url_unittests',
    #   'type': 'executable',
    #   'dependencies': [
    #     '../base/base.gyp:base_i18n',
    #     '../base/base.gyp:run_all_unittests',
    #     '../testing/gtest.gyp:gtest',
    #     '../third_party/icu/icu.gyp:icuuc',
    #     'url_lib',
    #   ],
    #   'sources': [
    #     'gurl_unittest.cc',
    #     'url_canon_unittest.cc',
    #     'url_parse_unittest.cc',
    #     'url_test_utils.h',
    #     'url_util_unittest.cc',
    #   ],
    #   'conditions': [
    #     # TODO(dmikurube): Kill linux_use_tcmalloc. http://crbug.com/345554
    #     ['os_posix==1 and OS!="mac" and OS!="ios" and ((use_allocator!="none" and use_allocator!="see_use_tcmalloc") or (use_allocator=="see_use_tcmalloc" and linux_use_tcmalloc==1))',
    #       {
    #         'dependencies': [
    #           '../base/allocator/allocator.gyp:allocator',
    #         ],
    #       }
    #     ],
    #   ],
    #   # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
    #   'msvs_disabled_warnings': [4267, ],
    # },
  ],
}
