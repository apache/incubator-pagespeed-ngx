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
  },

  'targets': [
    {
      'target_name': 'css_parser',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
	'<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
	'<(DEPTH)/third_party/google-sparsehash/google-sparsehash.gyp:include',
      ],
      # For .h files included in these .h files.
      'direct_dependent_settings': {
        'include_dirs': [
          # TODO(sligocki): Import these from google-sparsehash.gyp
          '<(DEPTH)/third_party/google-sparsehash/gen/arch/<(OS)/<(target_arch)/include',
          '<(DEPTH)/third_party/google-sparsehash/src',
        ],
      },
      'include_dirs': [
        '<(css_parser_root)',
        '<(DEPTH)',
      ],
      'cflags': ['-Wno-sign-compare', '-Wno-return-type'],
      'sources': [
        '<(css_parser_root)/string_using.h',
        '<(css_parser_root)/webutil/css/identifier.cc',
        '<(css_parser_root)/webutil/css/identifier.h',
        '<(css_parser_root)/webutil/css/parser.cc',
        '<(css_parser_root)/webutil/css/parser.h',
        '<(css_parser_root)/webutil/css/property.cc',
        '<(css_parser_root)/webutil/css/property.h',
        '<(css_parser_root)/webutil/css/selector.cc',
        '<(css_parser_root)/webutil/css/selector.h',
        '<(css_parser_root)/webutil/css/string.h',
        '<(css_parser_root)/webutil/css/string_util.cc',
        '<(css_parser_root)/webutil/css/string_util.h',
        '<(css_parser_root)/webutil/css/tostring.cc',
        '<(css_parser_root)/webutil/css/util.cc',
        '<(css_parser_root)/webutil/css/util.h',
        '<(css_parser_root)/webutil/css/value.cc',
        '<(css_parser_root)/webutil/css/value.h',
        '<(css_parser_root)/webutil/css/valuevalidator.cc',
        '<(css_parser_root)/webutil/css/valuevalidator.h',

        #'<(css_parser_root)/webutil/css/parse_arg.cc',
	# Tests
	#'<(css_parser_root)/webutil/css/gtest_main.cc',
        #'<(css_parser_root)/webutil/css/identifier_test.cc',
        #'<(css_parser_root)/webutil/css/parser_unittest.cc',
        #'<(css_parser_root)/webutil/css/property_test.cc',
        #'<(css_parser_root)/webutil/css/tostring_test.cc',
        #'<(css_parser_root)/webutil/css/util_test.cc',
        #'<(css_parser_root)/webutil/css/valuevalidator_test.cc',

        '<(css_parser_root)/webutil/html/htmlcolor.cc',
        '<(css_parser_root)/webutil/html/htmlcolor.h',
        '<(css_parser_root)/webutil/html/htmltagenum.cc',
        '<(css_parser_root)/webutil/html/htmltagenum.h',
        '<(css_parser_root)/webutil/html/htmltagindex.cc',
        '<(css_parser_root)/webutil/html/htmltagindex.h',

	# UnicodeText
        '<(css_parser_root)/util/utf8/internal/unicodetext.cc',
        '<(css_parser_root)/util/utf8/internal/unilib.cc',
        '<(css_parser_root)/util/utf8/public/config.h',
        '<(css_parser_root)/util/utf8/public/unicodetext.h',
        '<(css_parser_root)/util/utf8/public/unilib.h',

	# libutf
        #'<(css_parser_root)/third_party/utf/Make.Linux-x86_64',
        #'<(css_parser_root)/third_party/utf/Makefile',
        #'<(css_parser_root)/third_party/utf/NOTICE',
        #'<(css_parser_root)/third_party/utf/README',
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
        # TODO(sligocki): What is the correct format for this?
        # runetypebody-5.0.0.c should not be compiled by itself, only #included.
        #'<(css_parser_root)/third_party/utf/runetypebody-5.0.0.c',
        '<(css_parser_root)/third_party/utf/utf.h',
        '<(css_parser_root)/third_party/utf/utfdef.h',
        '<(css_parser_root)/third_party/utf/utfecpy.c',
        '<(css_parser_root)/third_party/utf/utflen.c',
        '<(css_parser_root)/third_party/utf/utfnlen.c',
        '<(css_parser_root)/third_party/utf/utfrrune.c',
        '<(css_parser_root)/third_party/utf/utfrune.c',
        '<(css_parser_root)/third_party/utf/utfutf.c',

	# Supporting interfaces.
        '<(css_parser_root)/base/commandlineflags.h',
        '<(css_parser_root)/base/googleinit.h',
        '<(css_parser_root)/base/macros.h',
        '<(css_parser_root)/base/paranoid.h',
        '<(css_parser_root)/base/stringprintf.h',
        '<(css_parser_root)/string_using.h',
        '<(css_parser_root)/strings/ascii_ctype.cc',
        '<(css_parser_root)/strings/ascii_ctype.h',
        '<(css_parser_root)/strings/escaping.h',
        '<(css_parser_root)/strings/join.h',
        '<(css_parser_root)/strings/memutil.h',
        '<(css_parser_root)/strings/stringpiece.h',
        '<(css_parser_root)/strings/stringpiece_utils.cc',
        '<(css_parser_root)/strings/stringpiece_utils.h',
        '<(css_parser_root)/strings/stringprintf.h',
        '<(css_parser_root)/strings/strutil.h',
        #'<(css_parser_root)/testing/base/public/googletest.h',
        #'<(css_parser_root)/testing/base/public/gunit.h',
        '<(css_parser_root)/testing/production_stub/public/gunit_prod.h',
        '<(css_parser_root)/util/gtl/dense_hash_map.h',
        '<(css_parser_root)/util/gtl/map-util.h',
        '<(css_parser_root)/util/gtl/singleton.h',
        '<(css_parser_root)/util/gtl/stl_util-inl.h',
        '<(css_parser_root)/util/hash/hash.h',
	],
    },
  ],
}
