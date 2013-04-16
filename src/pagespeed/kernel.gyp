# Copyright 2013 Google Inc.
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
  'targets': [
    {
      'target_name': 'base',
      'type': '<(library)',
      'sources': [
        'kernel/base/string_util.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
    },
    {
      'target_name': 'base_test_util',
      'type': '<(library)',
      'sources': [
        'kernel/base/gtest.cc',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/testing/gtest/include',
        ],
      },
      'include_dirs': [
        '<(DEPTH)',
        '<(DEPTH)/testing/gtest/include',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest_main',
      ],
    },
    {
      'target_name': 'util',
      'type': '<(library)',
      'sources': [
        'kernel/util/fast_wildcard_group.cc',
        'kernel/util/rolling_hash.cc',
        'kernel/util/wildcard.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        'base',
        '<(DEPTH)/third_party/re2/re2.gyp:re2',
      ],
    }
  ],
}
