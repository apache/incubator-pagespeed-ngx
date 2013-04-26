# Copyright 2012 Google Inc.
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
#
# Author: gagansingh@google.com (Gagandeep Singh)

{
  'variables': {
    're2_root': '<(DEPTH)/third_party/re2',
  },
  'targets': [
    {
      'target_name': 're2',
      'type': '<(library)',
      'include_dirs': [
        '<(re2_root)/src/',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/re2/src/',
        ],
      },
      'sources': [
        'src/re2/bitstate.cc',
        'src/re2/compile.cc',
        'src/re2/dfa.cc',
        'src/re2/filtered_re2.cc',
        'src/re2/mimics_pcre.cc',
        'src/re2/nfa.cc',
        'src/re2/onepass.cc',
        'src/re2/parse.cc',
        'src/re2/perl_groups.cc',
        'src/re2/prefilter.cc',
        'src/re2/prefilter_tree.cc',
        'src/re2/prog.cc',
        'src/re2/re2.cc',
        'src/re2/regexp.cc',
        'src/re2/set.cc',
        'src/re2/simplify.cc',
        'src/re2/tostring.cc',
        'src/re2/unicode_casefold.cc',
        'src/re2/unicode_groups.cc',

        'src/util/arena.cc',
        'src/util/hash.cc',
        'src/util/pcre.cc',
        'src/util/random.cc',
        'src/util/rune.cc',
        'src/util/stringpiece.cc',
        'src/util/stringprintf.cc',
        'src/util/strutil.cc',
        'src/util/test.cc',
        'src/util/thread.cc',
        'src/util/valgrind.cc',
      ],
    },
    {
      'target_name': 're2_bench_util',
      'type': '<(library)',
      'include_dirs': [
        '<(re2_root)/src/',
      ],
      'sources': [
        'src/util/benchmark.cc',
      ],
    },
  ],
}
