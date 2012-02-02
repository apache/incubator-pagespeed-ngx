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
    'instaweb_root':  '<(DEPTH)/net/instaweb',
  },

  'targets': [
    {
      'target_name': 'apache',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/build/build_util.gyp:mod_pagespeed_version_header',
        '<(DEPTH)/third_party/apache/httpd/httpd.gyp:include',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        '<(DEPTH)/third_party/serf/serf.gyp:serf',
        '<(instaweb_root)/instaweb.gyp:instaweb_automatic',
        '<(instaweb_root)/instaweb.gyp:instaweb_htmlparse',
        '<(instaweb_root)/instaweb.gyp:instaweb_http',
        '<(instaweb_root)/instaweb.gyp:instaweb_rewriter',
        '<(instaweb_root)/instaweb.gyp:instaweb_spriter',
        '<(instaweb_root)/instaweb.gyp:instaweb_util',
        '<(instaweb_root)/instaweb.gyp:instaweb_util_pthread',
        '<(instaweb_root)/instaweb.gyp:process_context',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'sources': [
        'apache/apache_config.cc',
        'apache/apache_cache.cc',
        'apache/apache_message_handler.cc',
        'apache/apache_resource_manager.cc',
        'apache/apache_rewrite_driver_factory.cc',
        'apache/apache_slurp.cc',
        'apache/apache_thread_system.cc',
        'apache/apr_file_system.cc',
        'apache/apr_statistics.cc',
        'apache/apr_timer.cc',
        'apache/header_util.cc',
        'apache/instaweb_context.cc',
        'apache/log_message_handler.cc',
        'apache/serf_url_async_fetcher.cc',
      ],
      'export_dependent_settings': [
        '<(instaweb_root)/instaweb.gyp:instaweb_util',
        '<(instaweb_root)/instaweb.gyp:instaweb_htmlparse',
      ],
    },
  ],
}
