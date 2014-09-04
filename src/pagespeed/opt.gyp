# Copyright 2014 Google Inc.
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
  'instaweb_root': '..',
    # Setting chromium_code to 1 turns on extra warnings. Also, if the compiler
    # is whitelisted in our common.gypi, those warnings will get treated as
    # errors.
    'chromium_code': 1,
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out/instaweb',
    'protoc_executable':
        '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
  },
  'targets': [
    {
      'target_name': 'pagespeed_ads_util',
      'type': '<(library)',
      'sources': [
        'opt/ads/ads_util.cc',
        'opt/ads/ads_attribute.cc',
        'opt/ads/show_ads_snippet_parser.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_base_core',
        '<(DEPTH)/third_party/re2/re2.gyp:re2',
      ],
    },
    {
      'target_name': 'pagespeed_logging_enums_pb',
      'variables': {
        'instaweb_protoc_subdir': 'pagespeed/opt/logging',
      },
      'sources': [
        'opt/logging/enums.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/enums.pb.cc',
      ],
      'includes': [
        '../net/instaweb/protoc.gypi',
      ],
    },
  ]
}

