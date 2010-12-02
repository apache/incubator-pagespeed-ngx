# Copyright 2009 Google Inc.
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
    'instaweb_root': '../..',
    'chromium_code': 1,
  },
  'targets': [
    {
      # TODO: break this up into sub-libs (mocks, real, etc)
      'target_name': 'instaweb_util',
      'type': '<(library)',
      'dependencies': [
        'instaweb_core.gyp:instaweb_util_core',
        '<(instaweb_root)/third_party/base64/base64.gyp:base64',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/chromium/src/net/tools/dump_cache.gyp:url_to_filename_encoder',
        '<(DEPTH)/third_party/libpagespeed/src/pagespeed/core/core.gyp:pagespeed_core',
        '<(DEPTH)/third_party/google-sparsehash/google-sparsehash.gyp:include',
      ],
      'sources': [
        'util/abstract_mutex.cc',
        'util/cache_interface.cc',
        'util/cache_url_async_fetcher.cc',
        'util/cache_url_fetcher.cc',
        'util/content_type.cc',
        'util/data_url.cc',
        'util/dummy_url_fetcher.cc',
        'util/fake_url_async_fetcher.cc',
        'util/file_cache.cc',
        'util/file_system.cc',
        'util/file_system_lock_manager.cc',
        'util/file_writer.cc',
        'util/filename_encoder.cc',
        'util/gzip_inflater.cc',
        'util/hasher.cc',
        'util/http_cache.cc',
        'util/http_response_parser.cc',
        'util/http_dump_url_async_writer.cc',
        'util/http_dump_url_fetcher.cc',
        'util/http_dump_url_writer.cc',
        'util/http_value.cc',
        'util/lru_cache.cc',
        'util/meta_data.cc',
        'util/md5_hasher.cc',
        'util/mock_hasher.cc',
        'util/mock_timer.cc',
        'util/named_lock_manager.cc',
        'util/null_message_handler.cc',
        'util/null_writer.cc',
        'util/pthread_mutex.cc',
        'util/ref_counted.cc',
        'util/rolling_hash.cc',
        'util/query_params.cc',
        'util/simple_meta_data.cc',
        'util/simple_stats.cc',
        'util/statistics.cc',
        'util/statistics_work_bound.cc',
        'util/stdio_file_system.cc',
        'util/threadsafe_cache.cc',
        'util/time_util.cc',
        'util/timer_based_abstract_lock.cc',
        'util/url_async_fetcher.cc',
        'util/url_escaper.cc',
        'util/url_fetcher.cc',
        'util/url_multipart_encoder.cc',
        'util/url_segment_encoder.cc',
        'util/user_agent.cc',
        'util/wait_url_async_fetcher.cc',
        'util/wget_url_fetcher.cc',
        'util/wildcard.cc',
        'util/wildcard_group.cc',
        'util/work_bound.cc',
        'util/write_through_cache.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
    },
    {
      'target_name': 'instaweb_htmlparse',
      'type': '<(library)',
      'dependencies': [
        'instaweb_core.gyp:instaweb_htmlparse_core',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        'htmlparse/file_driver.cc',
        'htmlparse/file_statistics_log.cc',
        'htmlparse/logging_html_filter.cc',
        'htmlparse/null_filter.cc',
        'htmlparse/statistics_log.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
    },
    {
      'target_name': 'instaweb_rewriter_base',
      'type': '<(library)',
      'dependencies': [
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        'rewriter/domain_lawyer.cc',
        'rewriter/resource.cc',
        'rewriter/output_resource.cc',
        'rewriter/resource_namer.cc',
        'rewriter/resource_manager.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
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
      'target_name': 'instaweb_rewriter_image',
      'type': '<(library)',
      'dependencies': [
        'instaweb_rewriter_base',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/libpagespeed/src/pagespeed/image_compression/image_compression.gyp:pagespeed_jpeg_optimizer',
        '<(DEPTH)/third_party/libpagespeed/src/pagespeed/image_compression/image_compression.gyp:pagespeed_png_optimizer',
        '<(DEPTH)/third_party/opencv/opencv.gyp:highgui',
      ],
      'sources': [
        'rewriter/image.cc',
        'rewriter/image_dim.cc',
        'rewriter/img_rewrite_filter.cc',
        'rewriter/img_tag_scanner.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
    },
    {
      'target_name': 'instaweb_rewriter_javascript',
      'type': '<(library)',
      'dependencies': [
        'instaweb_rewriter_base',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/libpagespeed/src/pagespeed/jsminify/js_minify.gyp:pagespeed_jsminify',
      ],
      'sources': [
        'rewriter/javascript_code_block.cc',
        'rewriter/javascript_filter.cc',
        'rewriter/javascript_library_identification.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
    },
    {
      'target_name': 'instaweb_rewriter_css',
      'type': '<(library)',
      'dependencies': [
        'instaweb_rewriter_base',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/css_parser/css_parser.gyp:css_parser',
      ],
      'sources': [
        'rewriter/css_filter.cc',
        'rewriter/css_minify.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
        '<(DEPTH)/third_party/css_parser/src',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
    },
    {
      'target_name': 'instaweb_rewriter',
      'type': '<(library)',
      'dependencies': [
        'instaweb_rewriter_base',
        'instaweb_core.gyp:instaweb_rewriter_html',
        'instaweb_rewriter_css',
        'instaweb_rewriter_image',
        'instaweb_rewriter_javascript',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        'rewriter/add_head_filter.cc',
        'rewriter/add_instrumentation_filter.cc',
        'rewriter/base_tag_filter.cc',
        'rewriter/cache_extender.cc',
        'rewriter/collapse_whitespace_filter.cc',
        'rewriter/common_filter.cc',
        'rewriter/css_combine_filter.cc',
        'rewriter/css_inline_filter.cc',
        'rewriter/css_move_to_head_filter.cc',
        'rewriter/css_outline_filter.cc',
        'rewriter/css_tag_scanner.cc',
        'rewriter/elide_attributes_filter.cc',
        'rewriter/data_url_input_resource.cc',
        'rewriter/file_input_resource.cc',
        'rewriter/html_attribute_quote_removal.cc',
        'rewriter/js_inline_filter.cc',
        'rewriter/js_outline_filter.cc',
        'rewriter/remove_comments_filter.cc',
        'rewriter/resource_tag_scanner.cc',
        'rewriter/rewrite_driver.cc',
        'rewriter/rewrite_driver_factory.cc',
        'rewriter/rewrite_filter.cc',
        'rewriter/rewrite_single_resource_filter.cc',
        'rewriter/rewrite_options.cc',
        'rewriter/script_tag_scanner.cc',
        'rewriter/strip_scripts_filter.cc',
        'rewriter/url_input_resource.cc',
        'rewriter/url_left_trim_filter.cc',
        'rewriter/url_partnership.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
    },
  ],
}
