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
        'instaweb.gyp:instaweb_automatic',
        'instaweb.gyp:instaweb_javascript',
        'instaweb.gyp:instaweb_spriter_test',
        'instaweb.gyp:instaweb_util_pthread',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest_main',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/protobuf/src',
        '<(SHARED_INTERMEDIATE_DIR)/protoc_out/instaweb',
        '<(DEPTH)',
        '<(DEPTH)/third_party/css_parser/src',
      ],
      'sources': [
        'automatic/html_detector_test.cc',
        'automatic/proxy_interface_test.cc',
        'htmlparse/html_keywords_test.cc',
        'htmlparse/html_name_test.cc',
        'htmlparse/html_parse_test.cc',
        'http/bot_checker_test.cc',
        'http/content_type_test.cc',
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
        'http/user_agent_matcher_test.cc',
        'http/wait_url_async_fetcher_test.cc',
        'http/write_through_http_cache_test.cc',
        'js/js_lexer_test.cc',
        'rewriter/add_instrumentation_filter_test.cc',
        'rewriter/ajax_rewrite_context_test.cc',
        'rewriter/cache_extender_test.cc',
        'rewriter/collapse_whitespace_filter_test.cc',
        'rewriter/common_filter_test.cc',
        'rewriter/css_combine_filter_test.cc',
        'rewriter/css_filter_test.cc',
        'rewriter/css_image_rewriter_test.cc',
        'rewriter/css_inline_filter_test.cc',
        'rewriter/css_inline_import_to_link_filter_test.cc',
        'rewriter/css_move_to_head_filter_test.cc',
        'rewriter/css_outline_filter_test.cc',
        'rewriter/css_rewrite_test_base.cc',
        'rewriter/css_tag_scanner_test.cc',
        'rewriter/css_util_test.cc',
        'rewriter/delay_images_filter_test.cc',
        'rewriter/div_structure_filter_test.cc',
        'rewriter/domain_lawyer_test.cc',
        'rewriter/domain_rewrite_filter_test.cc',
        'rewriter/elide_attributes_filter_test.cc',
        'rewriter/file_load_policy_test.cc',
        'rewriter/flush_html_filter_test.cc',
        'rewriter/google_analytics_filter_test.cc',
        'rewriter/html_attribute_quote_removal_test.cc',
        'rewriter/image_combine_filter_test.cc',
        'rewriter/image_endian_test.cc',
        'rewriter/image_oom_test.cc',
        'rewriter/image_rewrite_filter_test.cc',
        'rewriter/image_test.cc',
        'rewriter/image_test_base.cc',
        'rewriter/javascript_code_block_test.cc',
        'rewriter/javascript_filter_test.cc',
        'rewriter/js_combine_filter_test.cc',
        'rewriter/js_defer_disabled_filter_test.cc',
        'rewriter/js_defer_filter_test.cc',
        'rewriter/js_disable_filter_test.cc',
        'rewriter/js_inline_filter_test.cc',
        'rewriter/js_outline_filter_test.cc',
        'rewriter/lazyload_images_filter_test.cc',
        'rewriter/meta_tag_filter_test.cc',
        'rewriter/mock_resource_callback.cc',
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
        'rewriter/rewrite_query_test.cc',
        'rewriter/rewrite_single_resource_filter_test.cc',
        'rewriter/rewriter_test.cc',
        'rewriter/script_tag_scanner_test.cc',
        'rewriter/static_asserts_test.cc',
        'rewriter/strip_scripts_filter_test.cc',
        'rewriter/url_left_trim_filter_test.cc',
        'rewriter/url_partnership_test.cc',
        'spriter/libpng_image_library_test.cc',
        'spriter/image_spriter_test.cc',
        'util/arena_test.cc',
        'util/base64_test.cc',
        'util/chunking_writer_test.cc',
        'util/circular_buffer_test.cc',
        'util/data_url_test.cc',
        'util/file_cache_test.cc',
        'util/file_system_lock_manager_test.cc',
        'util/filename_encoder_test.cc',
        'util/function_test.cc',
        'util/google_timer.cc',
        'util/google_url_test.cc',
        'util/gzip_inflater_test.cc',
        'util/hashed_referer_statistics_test_base.cc',
        'util/hasher_test.cc',
        'util/lru_cache_test.cc',
        'util/md5_hasher_test.cc',
        'util/mem_debug.cc',
        'util/mem_file_system_test.cc',
        'util/message_handler_test.cc',
        'util/mock_message_handler_test.cc',
        'util/mock_scheduler_test.cc',
        'util/mock_time_cache_test.cc',
        'util/mock_timer_test.cc',
        'util/pthread_condvar_test.cc',
        'util/pthread_shared_mem_test.cc',
        'util/pthread_thread_system_test.cc',
        'util/pool_test.cc',
        'util/query_params_test.cc',
        'util/queued_alarm_test.cc',
        'util/queued_worker_test.cc',
        'util/queued_worker_pool_test.cc',
        'util/ref_counted_owner_test.cc',
        'util/ref_counted_ptr_test.cc',
        'util/scheduler_based_abstract_lock_test.cc',
        'util/scheduler_test.cc',
        'util/scheduler_thread_test.cc',
        'util/shared_circular_buffer_test_base.cc',
        'util/shared_dynamic_string_map_test_base.cc',
        'util/shared_mem_lock_manager_test_base.cc',
        'util/shared_mem_referer_statistics_test_base.cc',
        'util/shared_mem_statistics_test_base.cc',
        'util/shared_mem_test_base.cc',
        'util/simple_stats_test.cc',
        'util/slow_worker_test.cc',
        'util/statistics_work_bound_test.cc',
        'util/string_multi_map_test.cc',
        'util/string_util_test.cc',
        'util/symbol_table_test.cc',
        'util/thread_system_test_base.cc',
        'util/threadsafe_cache_test.cc',
        'util/time_util_test.cc',
        'util/url_escaper_test.cc',
        'util/url_multipart_encoder_test.cc',
        'util/waveform_test.cc',
        'util/wildcard_group_test.cc',
        'util/wildcard_test.cc',
        'util/worker_test_base.cc',
        'util/write_through_cache_test.cc',
#        'util/split_writer_test.cc',               # not currently needed
#        'util/stdio_file_system_test.cc',          # not currently needed
      ],
      'conditions': [
        ['support_posix_shared_mem != 1', {
          'sources!' : [
            'util/pthread_shared_mem_test.cc',
          ],
        }]
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
        'apache/apr_file_system_test.cc',
        'apache/speed_test.cc',
        'apache/serf_url_async_fetcher_test.cc',
        'util/mem_debug.cc',
      ],
    },
    {
      'target_name': 'test_infrastructure',
      'type': '<(library)',
      'dependencies': [
        'instaweb.gyp:instaweb_rewriter',
        'instaweb.gyp:instaweb_http_test',
        'instaweb.gyp:mem_clean_up',
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
        'rewriter/test_rewrite_driver_factory.cc',
        'rewriter/test_url_namer.cc',
        'util/file_system_test.cc',
        'util/gtest.cc',
        'util/mem_file_system.cc',
        'util/mock_scheduler.cc',
        'util/mock_timer.cc',
        'util/mock_time_cache.cc',
      ],
    },

  ],
}
