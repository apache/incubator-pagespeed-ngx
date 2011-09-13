# Copyright 2010-2011 Google Inc.
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

# TODO(sligocki): This is a confusing name for the gyp file where tests live.

{
  'variables': {
    # chromium_code indicates that the code is not
    # third-party code and should be subjected to strict compiler
    # warnings/errors in order to catch programming mistakes.
    'chromium_code': 1,
  },

  'targets': [
    {
      'target_name': 'mod_pagespeed',
      'type': 'loadable_module',
      'dependencies': [
        'apache.gyp:apache',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/apache/httpd/httpd.gyp:include',
        '<(DEPTH)/build/build_util.gyp:mod_pagespeed_version_header',
      ],
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)/protoc_out/instaweb',
        '<(DEPTH)',
      ],
      'sources': [
        'apache/instaweb_handler.cc',
        'apache/log_message_handler.cc',
        'util/mem_debug.cc',
        'apache/mod_instaweb.cc',
      ],
    },
  ],
}
