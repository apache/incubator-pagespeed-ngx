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

# This is meant to be include'd into a target to build mod_pagespeed
# against some externally-defined (as dependencies) Apache headers.

{
  'type': 'loadable_module',
  'dependencies': [
    'instaweb_apr.gyp:instaweb_apr',
    '<(DEPTH)/base/base.gyp:base',
    '<(DEPTH)/build/build_util.gyp:mod_pagespeed_version_header',
    '<(DEPTH)/pagespeed/kernel.gyp:util_gflags',
  ],
  'include_dirs': [
    '<(SHARED_INTERMEDIATE_DIR)/protoc_out/instaweb',
    '<(DEPTH)',
  ],
  'sources': [
    '<(DEPTH)/pagespeed/apache/apache_message_handler.cc',
    '<(DEPTH)/pagespeed/apache/apache_request_context.cc',
    '<(DEPTH)/pagespeed/apache/apache_rewrite_driver_factory.cc',
    '<(DEPTH)/pagespeed/apache/apache_server_context.cc',
    '<(DEPTH)/pagespeed/apache/apache_slurp.cc',
    '<(DEPTH)/pagespeed/apache/apache_writer.cc',
    '<(DEPTH)/pagespeed/apache/header_util.cc',
    '<(DEPTH)/pagespeed/apache/instaweb_context.cc',
    '<(DEPTH)/pagespeed/apache/instaweb_handler.cc',
    '<(DEPTH)/pagespeed/apache/log_message_handler.cc',
    '<(DEPTH)/pagespeed/apache/mod_instaweb.cc',
    '<(DEPTH)/pagespeed/kernel/base/mem_debug.cc',
  ],
  'ldflags+': [
    '-Wl,--version-script=build/mod_pagespeed.map',
  ],
}
