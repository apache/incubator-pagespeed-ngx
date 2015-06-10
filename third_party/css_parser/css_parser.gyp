# Copyright 2009 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'variables': {
    # chromium_code indicates that the code is not
    # third-party code and should be subjected to strict compiler
    # warnings/errors in order to catch programming mistakes.
    'chromium_code': 1,
    'css_parser_root': 'src',
    'instaweb_root': '../..',
  },

  'targets': [
    {
      'variables': {
        'chromium_code': 0,
      },
      'target_name': 'utf',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
      ],
      'include_dirs': [
        '<(css_parser_root)',
        '<(DEPTH)',
      ],
      'cflags': ['-funsigned-char', '-Wno-sign-compare', '-Wno-return-type'],
      'sources': [
        '<(css_parser_root)/third_party/utf/rune.c',
        '<(css_parser_root)/third_party/utf/runestrcat.c',
        '<(css_parser_root)/third_party/utf/runestrchr.c',
        '<(css_parser_root)/third_party/utf/runestrcmp.c',
        '<(css_parser_root)/third_party/utf/runestrcpy.c',
        '<(css_parser_root)/third_party/utf/runestrecpy.c',
        '<(css_parser_root)/third_party/utf/runestrlen.c',
        '<(css_parser_root)/third_party/utf/runestrncat.c',
        '<(css_parser_root)/third_party/utf/runestrncmp.c',
        '<(css_parser_root)/third_party/utf/runestrncpy.c',
        '<(css_parser_root)/third_party/utf/runestrrchr.c',
        '<(css_parser_root)/third_party/utf/runestrstr.c',
        '<(css_parser_root)/third_party/utf/runetype.c',
        '<(css_parser_root)/third_party/utf/utf.h',
        '<(css_parser_root)/third_party/utf/utfdef.h',
        '<(css_parser_root)/third_party/utf/utfecpy.c',
        '<(css_parser_root)/third_party/utf/utflen.c',
        '<(css_parser_root)/third_party/utf/utfnlen.c',
        '<(css_parser_root)/third_party/utf/utfrrune.c',
        '<(css_parser_root)/third_party/utf/utfrune.c',
        '<(css_parser_root)/third_party/utf/utfutf.c',
      ],
    },
    {
      'target_name': 'css_parser_gperf',
      'variables': {
        'instaweb_gperf_subdir': 'third_party/css_parser/src/webutil/css',
      },
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:proto_util',
        '<(DEPTH)/pagespeed/kernel.gyp:util',
        '<(DEPTH)/third_party/google-sparsehash/google-sparsehash.gyp:include',
      ],
      'sources': [
        '<(css_parser_root)/webutil/css/identifier.gperf',
        '<(css_parser_root)/webutil/css/property.gperf',
      ],
      'include_dirs': [
        '<(css_parser_root)',
        '<(DEPTH)',
      ],
      'includes': [
        '../../net/instaweb/gperf.gypi',
      ],
    },
    {
      'target_name': 'css_parser',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        '<(DEPTH)/third_party/google-sparsehash/google-sparsehash.gyp:include',
        'css_parser_gperf',
        'utf',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/third_party/google-sparsehash/google-sparsehash.gyp:include',
      ],
      'include_dirs': [
        '<(css_parser_root)',
        '<(DEPTH)',
      ],
      'cflags': ['-funsigned-char', '-Wno-sign-compare', '-Wno-return-type'],
      'sources': [
        '<(css_parser_root)/string_using.h',
        '<(css_parser_root)/webutil/css/media.cc',
        '<(css_parser_root)/webutil/css/parser.cc',
        '<(css_parser_root)/webutil/css/selector.cc',
        '<(css_parser_root)/webutil/css/string_util.cc',
        '<(css_parser_root)/webutil/css/tostring.cc',
        '<(css_parser_root)/webutil/css/util.cc',
        '<(css_parser_root)/webutil/css/value.cc',

        #'<(css_parser_root)/webutil/css/parse_arg.cc',
        # Tests
        #'<(css_parser_root)/webutil/css/gtest_main.cc',
        #'<(css_parser_root)/webutil/css/identifier_test.cc',
        #'<(css_parser_root)/webutil/css/parser_unittest.cc',
        #'<(css_parser_root)/webutil/css/property_test.cc',
        #'<(css_parser_root)/webutil/css/tostring_test.cc',
        #'<(css_parser_root)/webutil/css/util_test.cc',

        '<(css_parser_root)/webutil/html/htmlcolor.cc',
        '<(css_parser_root)/webutil/html/htmltagenum.cc',
        '<(css_parser_root)/webutil/html/htmltagindex.cc',

        # UnicodeText
        '<(css_parser_root)/util/utf8/internal/unicodetext.cc',
        '<(css_parser_root)/util/utf8/internal/unilib.cc',

        # Supporting interfaces.
        '<(css_parser_root)/strings/ascii_ctype.cc',
        '<(css_parser_root)/strings/stringpiece_utils.cc',
      ],
    },
  ],
}
