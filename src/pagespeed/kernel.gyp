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
      'target_name': 'pagespeed_base',
      # xcode build names libraries just based on the target_name, so
      # if this were merely base then its libbase.a would clash with
      # Chromium libbase.a
      'type': '<(library)',
      'sources': [
        'kernel/base/abstract_mutex.cc',
        'kernel/base/condvar.cc',
        'kernel/base/debug.cc',
        'kernel/base/file_system.cc',
        'kernel/base/google_message_handler.cc',
        'kernel/base/message_handler.cc',
        'kernel/base/null_message_handler.cc',
        'kernel/base/null_writer.cc',
        'kernel/base/print_message_handler.cc',
        'kernel/base/stdio_file_system.cc',
        'kernel/base/string_util.cc',
        'kernel/base/string_writer.cc',
        'kernel/base/time_util.cc',
        'kernel/base/timer.cc',
        'kernel/base/thread_system.cc',
        'kernel/base/writer.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
        '<(DEPTH)/third_party/chromium/src/base/third_party/nspr',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/google-sparsehash/google-sparsehash.gyp:include',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/google-sparsehash/google-sparsehash.gyp:include',
      ],
    },
    {
      'target_name': 'pagespeed_base_test_infrastructure',
      'type': '<(library)',
      'sources': [
        'kernel/base/mock_message_handler.cc',
        'kernel/base/platform.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/base/base.gyp:base'
      ],
    },
    {
      'target_name': 'base_test_util',
      'type': '<(library)',
      'sources': [
        'kernel/base/file_system_test_base.cc',
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
      'target_name': 'pagespeed_http',
      'type': '<(library)',
      'sources': [
        'kernel/http/caching_headers.cc',
        'kernel/http/content_type.cc',
        'kernel/http/http_names.cc',
      ],
      'dependencies': [
        'pagespeed_base',
      ],
    },
    {
      'target_name': 'jsminify',
      'type': '<(library)',
      'sources': [
        'kernel/js/js_minify.cc',
      ],
      # TODO(bmcquade): We should fix the code so this is not needed.
      'msvs_disabled_warnings': [ 4018 ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        'pagespeed_base',
        'pagespeed_javascript_gperf',
      ],
    },
    {
      'target_name': 'pagespeed_javascript_gperf',
      'variables': {
        'instaweb_gperf_subdir': 'kernel/js',
        'instaweb_root':  '<(DEPTH)/pagespeed',
      },
      'sources': [
        'kernel/js/js_keywords.gperf',
      ],
      'includes': [
        # TODO(jkarlin): Move gperf.gypi to pagespeed/, changing all references
        # in net/instaweb gyp files.
        '../net/instaweb/gperf.gypi',
      ]
    },
    {
      'target_name': 'util',
      'type': '<(library)',
      'sources': [
        'kernel/util/checking_thread_system.cc',
        'kernel/util/fast_wildcard_group.cc',
        'kernel/util/rolling_hash.cc',
        'kernel/util/wildcard.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        'pagespeed_base',
        '<(DEPTH)/third_party/re2/re2.gyp:re2',
      ],
    }
  ],
}
