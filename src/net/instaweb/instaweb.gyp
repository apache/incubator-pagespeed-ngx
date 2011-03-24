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
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out/instaweb',
    'protoc_executable': '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
    'chromium_code': 1,
  },
  'targets': [
      # NOTE(abliss): These targets appear to get built in the order they
      # appear, regardless of their claimed dependencies.  Moving
      # instaweb_spriter below instaweb_rewriter breaks the build.
    {
      'target_name': 'instaweb_spriter_genproto',
      'type': 'none',
      'sources': [
        'spriter/public/image_spriter.proto',
      ],
      'rules': [
        {
            'rule_name': 'genproto',
            'extension': 'proto',
            'inputs': [
                '<(protoc_executable)',
                ],
            'message': 'Generating C++ code from <(RULE_INPUT_PATH)',

          'outputs': [
            '<(protoc_out_dir)/net/instaweb/spriter/public/<(RULE_INPUT_ROOT).pb.h',
            '<(protoc_out_dir)/net/instaweb/spriter/public/<(RULE_INPUT_ROOT).pb.cc',
          ],
          'action': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            '--proto_path=<(instaweb_root)',
            '<(instaweb_root)/net/instaweb/spriter/public/<(RULE_INPUT_NAME)',
            '--cpp_out=<(protoc_out_dir)',
          ],
        },
      ],
    },
    {
      'target_name': 'instaweb_spriter_pb',
      'type': '<(library)',
      'hard_dependency': 1,
      'dependencies': [
        'instaweb_spriter_genproto',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
       ],
      'sources': [
        '<(protoc_out_dir)/net/instaweb/spriter/public/image_spriter.pb.cc',
      ],
      'include_dirs': [
        '<(protoc_out_dir)',
      ],
      'export_dependent_settings': [
        'instaweb_spriter_genproto',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ]
    },
    {
      'target_name': 'instaweb_spriter',
      'type': '<(library)',
      'dependencies': [
        'instaweb_spriter_pb',
      ],
      'sources': [
          'spriter/image_library_interface.cc',
          'spriter/image_spriter.cc',
          'spriter/public/image_spriter.proto',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
        '<(protoc_out_dir)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
          '<(protoc_out_dir)',
        ],
      },
    },
    {
      'target_name': 'instaweb_rewriter_genproto',
      'type': 'none',
      'sources': [
        'rewriter/cached_result.proto',
      ],
      'rules': [
        {
            'rule_name': 'genproto',
            'extension': 'proto',
            'inputs': [
                '<(protoc_executable)',
                ],
          'outputs': [
            '<(protoc_out_dir)/net/instaweb/rewriter/<(RULE_INPUT_ROOT).pb.h',
            '<(protoc_out_dir)/net/instaweb/rewriter/<(RULE_INPUT_ROOT).pb.cc',
          ],
          'action': [
            '<(protoc_executable)',
            '--proto_path=<(instaweb_root)',
            '<(instaweb_root)/net/instaweb/rewriter/<(RULE_INPUT_NAME)',
            '--cpp_out=<(protoc_out_dir)',
          ],
          'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
        },
      ],
      'dependencies': [
        'instaweb_spriter_genproto',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protoc#host',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '<(protoc_out_dir)',
        ]
      },
      'export_dependent_settings': [
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
    },
    {
      'target_name': 'instaweb_spriter_test',
      'type': '<(library)',
      'dependencies': [
        'instaweb_spriter',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        'instaweb_spriter_genproto',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protoc#host',
      ],
      'sources': [
        'spriter/image_spriter_test.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
      ],
    },
    {
      'target_name': 'instaweb_rewriter_pb',
      'type': '<(library)',
      'hard_dependency': 1,
      'dependencies': [
        'instaweb_rewriter_genproto',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
       ],
      'sources': [
        '<(protoc_out_dir)/net/instaweb/rewriter/cached_result.pb.cc',
      ],
      'export_dependent_settings': [
        'instaweb_rewriter_genproto',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ]
    },
    {
      # TODO: break this up into sub-libs (mocks, real, etc)
      'target_name': 'instaweb_util',
      'type': '<(library)',
      'dependencies': [
        'instaweb_core.gyp:instaweb_util_core',
        'instaweb_http',
        '<(instaweb_root)/third_party/base64/base64.gyp:base64',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/chromium/src/net/tools/dump_cache.gyp:url_to_filename_encoder',
        '<(DEPTH)/third_party/libpagespeed/src/pagespeed/core/core.gyp:pagespeed_core',
        '<(DEPTH)/third_party/google-sparsehash/google-sparsehash.gyp:include',
      ],
      'sources': [
        'http/cache_url_async_fetcher.cc',
        'http/cache_url_fetcher.cc',
        'http/dummy_url_fetcher.cc',
        'http/fake_url_async_fetcher.cc',
        'http/http_cache.cc',
        'http/http_dump_url_async_writer.cc',
        'http/http_dump_url_fetcher.cc',
        'http/http_dump_url_writer.cc',
        'http/http_response_parser.cc',
        'http/http_value.cc',
        'http/meta_data.cc',
        'http/sync_fetcher_adapter.cc',
        'http/sync_fetcher_adapter_callback.cc',
        'http/url_async_fetcher.cc',
        'http/url_fetcher.cc',
        'http/url_pollable_async_fetcher.cc',
        'http/wait_url_async_fetcher.cc',
        'http/wget_url_fetcher.cc',
        'util/abstract_condvar.cc',
        'util/abstract_mutex.cc',
        'util/abstract_shared_mem.cc',
        'util/cache_interface.cc',
        'util/chunking_writer.cc',
        'util/data_url.cc',
        'util/debug.cc',
        'util/file_cache.cc',
        'util/file_system.cc',
        'util/file_system_lock_manager.cc',
        'util/file_writer.cc',
        'util/filename_encoder.cc',
        'util/gzip_inflater.cc',
        'util/hasher.cc',
        'util/lru_cache.cc',
        'util/md5_hasher.cc',
        'util/mock_hasher.cc',
        'util/mock_message_handler.cc',
        'util/mock_timer.cc',
        'util/named_lock_manager.cc',
        'util/null_message_handler.cc',
        'util/null_writer.cc',
        'util/query_params.cc',
        'util/ref_counted.cc',
        'util/rolling_hash.cc',
        'util/simple_stats.cc',
        'util/shared_mem_statistics.cc',
#        'util/split_writer.cc',                Not currently needed
        'util/statistics.cc',
        'util/statistics_work_bound.cc',
        'util/stdio_file_system.cc',
        'util/threadsafe_cache.cc',
        'util/time_util.cc',
        'util/timer_based_abstract_lock.cc',
        'util/url_escaper.cc',
        'util/url_multipart_encoder.cc',
        'util/url_segment_encoder.cc',
        'util/user_agent.cc',
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
      'target_name': 'instaweb_util_pthread',
      'type': '<(library)',
      'dependencies': [
        'instaweb_util',
      ],
      'sources': [
        'util/pthread_condvar.cc',
        'util/pthread_mutex.cc',
        'util/pthread_shared_mem.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
      'ldflags': [
        '-lrt',
      ]
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
      'export_dependent_settings': [
        'instaweb_core.gyp:instaweb_htmlparse_core',
      ],
    },
    {
      'target_name': 'instaweb_http',
      'type': '<(library)',
      'dependencies': [
        'instaweb_core.gyp:instaweb_util_core',
        'instaweb_http_pb',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/libpagespeed/src/pagespeed/core/core.gyp:pagespeed_core',
      ],
      'sources': [
        'http/headers.cc',
        'http/request_headers.cc',
        'http/response_headers.cc',
        'http/response_headers_parser.cc',
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
      'target_name': 'instaweb_http_test',
      'type': '<(library)',
      'dependencies': [
        'instaweb_http',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/libpagespeed/src/pagespeed/core/core.gyp:pagespeed_core',
      ],
      'sources': [
        'http/counting_url_async_fetcher.cc',
        'util/counting_writer.cc',
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
        'instaweb_core.gyp:instaweb_htmlparse_core',
        'instaweb_rewriter_pb',
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
        '<(protoc_out_dir)',
        '<(DEPTH)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
      'export_dependent_settings': [
        'instaweb_core.gyp:instaweb_htmlparse_core',
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
        'rewriter/image_url_encoder.cc',
        'rewriter/img_rewrite_filter.cc',
        'rewriter/img_tag_scanner.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(protoc_out_dir)',
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
        '<(protoc_out_dir)',
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
        'instaweb_spriter',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/css_parser/css_parser.gyp:css_parser',
      ],
      'sources': [
        'rewriter/css_filter.cc',
        'rewriter/css_image_rewriter.cc',
        'rewriter/css_minify.cc',
        'rewriter/img_combine_filter.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(protoc_out_dir)',
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
        'instaweb_http',
        'instaweb_rewriter_css',
        'instaweb_rewriter_image',
        'instaweb_rewriter_javascript',
        'instaweb_spriter',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        'rewriter/add_head_filter.cc',
        'rewriter/add_instrumentation_filter.cc',
        'rewriter/cache_extender.cc',
        'rewriter/common_filter.cc',
        'rewriter/css_combine_filter.cc',
        'rewriter/css_inline_filter.cc',
        'rewriter/css_move_to_head_filter.cc',
        'rewriter/css_outline_filter.cc',
        'rewriter/css_tag_scanner.cc',
        'rewriter/data_url_input_resource.cc',
        'rewriter/file_input_resource.cc',
        'rewriter/google_analytics_filter.cc',
        'rewriter/js_combine_filter.cc',
        'rewriter/js_inline_filter.cc',
        'rewriter/js_outline_filter.cc',
        'rewriter/resource_combiner.cc',
        'rewriter/resource_tag_scanner.cc',
        'rewriter/rewrite_driver.cc',
        'rewriter/rewrite_driver_factory.cc',
        'rewriter/rewrite_filter.cc',
        'rewriter/rewrite_single_resource_filter.cc',
        'rewriter/rewrite_options.cc',
        'rewriter/scan_filter.cc',
        'rewriter/script_tag_scanner.cc',
        'rewriter/strip_scripts_filter.cc',
        'rewriter/url_input_resource.cc',
        'rewriter/url_left_trim_filter.cc',
        'rewriter/url_partnership.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(protoc_out_dir)',
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
      'target_name': 'instaweb_http_genproto',
      'type': 'none',
      'sources': [
        'http/http.proto',
      ],
      'rules': [
        {
            'rule_name': 'genproto',
            'extension': 'proto',
            'inputs': [
                '<(protoc_executable)',
                ],
          'outputs': [
            '<(protoc_out_dir)/net/instaweb/http/<(RULE_INPUT_ROOT).pb.h',
            '<(protoc_out_dir)/net/instaweb/http/<(RULE_INPUT_ROOT).pb.cc',
          ],
          'action': [
            '<(protoc_executable)',
            '--proto_path=<(instaweb_root)',
            '<(instaweb_root)/net/instaweb/http/<(RULE_INPUT_NAME)',
            '--cpp_out=<(protoc_out_dir)',
          ],
          'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
        },
      ],
      'dependencies': [
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protoc#host',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(protoc_out_dir)',
        ]
      },
      'export_dependent_settings': [
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
    },
    {
      'target_name': 'instaweb_http_pb',
      'type': '<(library)',
      'hard_dependency': 1,
      'dependencies': [
        'instaweb_http_genproto',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
       ],
      'sources': [
        '<(protoc_out_dir)/net/instaweb/http/http.pb.cc',
      ],
      'export_dependent_settings': [
        'instaweb_http_genproto',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ]
    },
  ],
}
