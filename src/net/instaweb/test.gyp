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

{
  'variables': {
    # chromium_code indicates that the code is not
    # third-party code and should be subjected to strict compiler
    # warnings/errors in order to catch programming mistakes.
    'chromium_code': 1,
  },

  'targets': [
    {
      'variables': {
        # OpenCV has compile warnings in gcc 4.1 in a header file so turn off
        # strict checking.
        #
        # TODO(jmarantz): disable the specific warning rather than
        # turning off all warnings, and also scope this down to a
        # minimal wrapper around the offending header file.
        #
        # TODO(jmarantz): figure out how to test for this failure in
        # checkin tests, as it passes in gcc 4.2 and fails in gcc 4.1.
        'chromium_code': 0,
      },
      'target_name': 'pagespeed_automatic_test',
      'type': 'executable',
      'dependencies': [
        'test_infrastructure',
        'instaweb.gyp:instaweb_spriter_test',
        'instaweb.gyp:instaweb_util_pthread',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest_main',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/protobuf/src',
        '<(SHARED_INTERMEDIATE_DIR)/protoc_out/instaweb',
        '<(DEPTH)',
      ],
      'sources': [
        'htmlparse/html_keywords_test.cc',
        'htmlparse/html_name_test.cc',
        'htmlparse/html_parse_test.cc',
        'http/cache_fetcher_test.cc',
        'http/cache_url_async_fetcher_test.cc',
        'http/cache_url_fetcher_test.cc',
        'http/fetcher_test.cc',
        'http/http_cache_test.cc',
        'http/http_dump_url_async_writer_test.cc',
        'http/http_dump_url_fetcher_test.cc',
        'http/http_dump_url_writer_test.cc',
        'http/http_response_parser_test.cc',
        'http/http_value_test.cc',
        'http/mock_url_fetcher_test.cc',
        'http/request_headers_test.cc',
        'http/response_headers_test.cc',
        'http/sync_fetcher_adapter_test.cc',
        'http/url_async_fetcher_test.cc',
        'http/wait_url_async_fetcher_test.cc',
        'rewriter/cache_extender_test.cc',
        'rewriter/collapse_whitespace_filter_test.cc',
        'rewriter/common_filter_test.cc',
        'rewriter/css_combine_filter_test.cc',
        'rewriter/css_filter_test.cc',
        'rewriter/css_image_rewriter_test.cc',
        'rewriter/css_inline_filter_test.cc',
        'rewriter/css_move_to_head_filter_test.cc',
        'rewriter/css_outline_filter_test.cc',
        'rewriter/css_rewrite_test_base.cc',
        'rewriter/css_tag_scanner_test.cc',
        'rewriter/domain_lawyer_test.cc',
        'rewriter/domain_rewrite_filter_test.cc',
        'rewriter/elide_attributes_filter_test.cc',
        'rewriter/google_analytics_filter_test.cc',
        'rewriter/html_attribute_quote_removal_test.cc',
        'rewriter/image_combine_filter_test.cc',
        'rewriter/image_endian_test.cc',
        'rewriter/image_rewrite_filter_test.cc',
        'rewriter/image_test.cc',
        'rewriter/javascript_code_block_test.cc',
        'rewriter/javascript_filter_test.cc',
        'rewriter/js_combine_filter_test.cc',
        'rewriter/js_inline_filter_test.cc',
        'rewriter/js_outline_filter_test.cc',
        'rewriter/remove_comments_filter_test.cc',
        'rewriter/resource_combiner_test.cc',
        'rewriter/resource_manager_test.cc',
        'rewriter/resource_manager_test_base.cc',
        'rewriter/resource_namer_test.cc',
        'rewriter/resource_slot_test.cc',
        'rewriter/resource_tag_scanner_test.cc',
        'rewriter/rewrite_context_test.cc',
        'rewriter/rewrite_driver_test.cc',
        'rewriter/rewrite_options_test.cc',
        'rewriter/rewrite_single_resource_filter_test.cc',
        'rewriter/rewriter_test.cc',
        'rewriter/script_tag_scanner_test.cc',
        'rewriter/static_asserts_test.cc',
        'rewriter/strip_scripts_filter_test.cc',
        'rewriter/url_left_trim_filter_test.cc',
        'rewriter/url_partnership_test.cc',
        'spriter/image_spriter_test.cc',
        'util/arena_test.cc',
        'util/base64_test.cc',
        'util/content_type_test.cc',
        'util/chunking_writer_test.cc',
        'util/data_url_test.cc',
        'util/file_cache_test.cc',
        'util/file_system_lock_manager_test.cc',
        'util/filename_encoder_test.cc',
        'util/google_timer.cc',
        'util/google_url_test.cc',
        'util/gzip_inflater_test.cc',
        'util/lru_cache_test.cc',
        'util/md5_hasher_test.cc',
        'util/mem_file_system_test.cc',
        'util/message_handler_test.cc',
        'util/mock_message_handler_test.cc',
        'util/pthread_condvar_test.cc',
        'util/pthread_shared_mem_test.cc',
        'util/pthread_thread_system_test.cc',
        'util/pool_test.cc',
        'util/query_params_test.cc',
        'util/ref_counted_ptr_test.cc',
        'util/shared_mem_lock_manager_test_base.cc',
        'util/shared_mem_statistics_test_base.cc',
        'util/shared_mem_test_base.cc',
        'util/simple_stats_test.cc',
        'util/statistics_work_bound_test.cc',
        'util/string_multi_map_test.cc',
        'util/string_util_test.cc',
        'util/symbol_table_test.cc',
        'util/thread_system_test_base.cc',
        'util/threadsafe_cache_test.cc',
        'util/time_util_test.cc',
        'util/timer_based_lock_test.cc',
        'util/url_escaper_test.cc',
        'util/url_multipart_encoder_test.cc',
        'util/user_agent_test.cc',
        'util/wildcard_group_test.cc',
        'util/wildcard_test.cc',
        'util/write_through_cache_test.cc',
#        'util/split_writer_test.cc',               # not currently needed
#        'util/stdio_file_system_test.cc',          # not currently needed
      ],
    },
    {
      'variables': {
        # OpenCV has compile warnings in gcc 4.1 in a header file so turn off
        # strict checking.
        #
        # TODO(jmarantz): disable the specific warning rather than
        # turning off all warnings, and also scope this down to a
        # minimal wrapper around the offending header file.
        #
        # TODO(jmarantz): figure out how to test for this failure in
        # checkin tests, as it passes in gcc 4.2 and fails in gcc 4.1.
        'chromium_code': 0,
      },
      'target_name': 'mod_pagespeed_test',
      'type': 'executable',
      'dependencies': [
        'test_infrastructure',
        'apache.gyp:apache',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest_main',
        '<(DEPTH)/third_party/apache/apr/apr.gyp:apr',
        '<(DEPTH)/third_party/apache/aprutil/aprutil.gyp:aprutil',
        '<(DEPTH)/third_party/apache/httpd/httpd.gyp:include',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/protobuf/src',
        '<(SHARED_INTERMEDIATE_DIR)/protoc_out/instaweb',
        '<(DEPTH)',
      ],
      'sources': [
        "apache/mem_clean_up.cc",
        'apache/apr_condvar_test.cc',
        'apache/apr_file_system_test.cc',
        'apache/mem_debug.cc',
        'apache/speed_test.cc',
        'apache/serf_url_async_fetcher_test.cc',
      ],
    },
    {
      'target_name': 'test_infrastructure',
      'type': '<(library)',
      'dependencies': [
        'instaweb.gyp:instaweb_rewriter',
        'instaweb.gyp:instaweb_http_test',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/build/build_util.gyp:mod_pagespeed_version_header',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        '<(DEPTH)/third_party/google-sparsehash/google-sparsehash.gyp:include',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/protobuf/src',
        '<(SHARED_INTERMEDIATE_DIR)/protoc_out/instaweb',
        '<(DEPTH)',
      ],
      'sources': [
        'htmlparse/html_parse_test_base.cc',
        'http/mock_url_fetcher.cc',
        'rewriter/resource_manager_test_base.cc',
        'util/mem_file_system.cc',
        'util/file_system_test.cc',
        'util/gtest.cc',
      ],
    },

  ],
}
