# Copyright 2010 Google Inc.
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
  },

  'targets': [
    {
      'target_name': 'url_to_filename_encoder',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
      ],
      'include_dirs': [
        'dump_cache',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'dump_cache',
        ],
      },
      'sources': [
        'dump_cache/url_to_filename_encoder.cc',
        'dump_cache/url_utilities.cc',
      ],
    },
  ],
}
