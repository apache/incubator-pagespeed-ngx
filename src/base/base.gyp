# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Base was branched from the chromium version to reduce the number of
# dependencies of this package.  Specifically, we would like to avoid
# depending on the chrome directory, which contains the chrome version
# and branding information.
# TODO: push this refactoring to chronium trunk.

{
  'variables': {
    'chromium_code': 1,
    'chromium_root': '<(DEPTH)/third_party/chromium/src',
  },
  'includes': [
    'base.gypi',
  ],
  'targets': [
    {
      # This is the subset of files from base that should not be used with a
      # dynamic library. Note that this library cannot depend on base because
      # base depends on base_static.
      'target_name': 'base_static',
      'type': 'static_library',
      'sources': [
        '<(chromium_root)/base/base_switches.cc',
        '<(chromium_root)/base/base_switches.h',
        '<(chromium_root)/base/win/pe_image.cc',
        '<(chromium_root)/base/win/pe_image.h',
      ],
      'include_dirs': [
        '<(chromium_root)',
        '<(DEPTH)',
      ],
    },
    {
      'target_name': 'base_unittests',
      'type': 'executable',
      'sources': [
        '<(chromium_root)/base/string_piece_unittest.cc',
        '<(chromium_root)/base/win/win_util_unittest.cc',
      ],
      'dependencies': [
        'base',
        'base_static',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/testing/gtest.gyp:gtest_main',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'conditions': [
        ['OS != "win"', {
          'sources!': [
            '<(chromium_root)/base/win_util_unittest.cc',
          ],
        }],
      ],
    },
  ],
}
