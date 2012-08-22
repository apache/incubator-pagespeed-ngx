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
    'protoc_executable':
        '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
    'data2c_out_dir': '<(SHARED_INTERMEDIATE_DIR)/data2c_out/instaweb',
    'data2c_exe':
        '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)instaweb_data2c<(EXECUTABLE_SUFFIX)',
    # Setting chromium_code to 1 turns on extra warnings. Also, if the compiler
    # is whitelisted in our common.gypi, those warnings will get treated as
    # errors.
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'instaweb_data2c',
      'type': 'executable',
      'sources': [
         'js/data_to_c.cc',
       ],
      'dependencies': [
        'instaweb_util',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
    },
    {
      'target_name': 'instaweb_client_domain_rewriter_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/rewriter',
        'var_name': 'client_domain_rewriter',
      },
      'sources': [
        'rewriter/client_domain_rewriter.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_client_domain_rewriter_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'client_domain_rewriter_opt',
      },
      'sources': [
        'genfiles/rewriter/client_domain_rewriter_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_defer_iframe_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/rewriter',
        'var_name': 'defer_iframe',
      },
      'sources': [
        'rewriter/defer_iframe.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_defer_iframe_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'defer_iframe_opt',
      },
      'sources': [
        'genfiles/rewriter/defer_iframe_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_delay_images_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/rewriter',
        'var_name': 'delay_images',
      },
      'sources': [
        'rewriter/delay_images.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_delay_images_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'delay_images_opt',
      },
      'sources': [
        'genfiles/rewriter/delay_images_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_delay_images_inline_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/rewriter',
        'var_name': 'delay_images_inline',
      },
      'sources': [
        'rewriter/delay_images_inline.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_delay_images_inline_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'delay_images_inline_opt',
      },
      'sources': [
        'genfiles/rewriter/delay_images_inline_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_detect_reflow_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/rewriter',
        'var_name': 'detect_reflow',
      },
      'sources': [
        'rewriter/detect_reflow.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_detect_reflow_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'detect_reflow_opt',
      },
      'sources': [
        'genfiles/rewriter/detect_reflow_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_deterministic_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/rewriter',
        'var_name': 'deterministic',
      },
      'sources': [
        'rewriter/deterministic.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_deterministic_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'deterministic_opt',
      },
      'sources': [
        'genfiles/rewriter/deterministic_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_js_defer_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/rewriter',
        'var_name': 'js_defer',
      },
      'sources': [
        'rewriter/js_defer.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_js_defer_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'js_defer_opt',
      },
      'sources': [
        'genfiles/rewriter/js_defer_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_lazyload_images_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/rewriter',
        'var_name': 'lazyload_images',
      },
      'sources': [
        'rewriter/lazyload_images.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_lazyload_images_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'lazyload_images_opt',
      },
      'sources': [
        'genfiles/rewriter/lazyload_images_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_local_storage_cache_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/rewriter',
        'var_name': 'local_storage_cache',
      },
      'sources': [
        'rewriter/local_storage_cache.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_local_storage_cache_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'local_storage_cache_opt',
      },
      'sources': [
        'genfiles/rewriter/local_storage_cache_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_spriter_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/spriter/public',
      },
      'sources': [
        'spriter/public/image_spriter.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/image_spriter.pb.cc',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_spriter',
      'type': '<(library)',
      'dependencies': [
        'instaweb_spriter_pb',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
      ],
      'sources': [
          'spriter/libpng_image_library.cc',
          'spriter/image_library_interface.cc',
          'spriter/image_spriter.cc',
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
      'target_name': 'instaweb_spriter_test',
      'type': '<(library)',
      'dependencies': [
        'instaweb_spriter',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        'instaweb_spriter_pb',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protoc#host',
      ],
      'sources': [
        'spriter/image_spriter_test.cc',
        'spriter/libpng_image_library_test.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
      ],
    },
    {
      'target_name': 'instaweb_flush_early_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        'rewriter/flush_early.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/flush_early.pb.cc',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_panel_config_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/panel_config.pb.cc',
        'rewriter/panel_config.proto',
      ],
      'dependencies': [
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_blink_critical_line_data_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/blink_critical_line_data.pb.cc',
        'rewriter/blink_critical_line_data.proto',
      ],
      'dependencies': [
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_rewriter_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/cached_result.pb.cc',
        'rewriter/cached_result.proto',
      ],
      'dependencies': [
        'instaweb_spriter_pb',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_clientstate_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/util',
      },
      'sources': [
        'util/client_state.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/client_state.pb.cc',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_propcache_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/util',
      },
      'sources': [
        'util/property_cache.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/property_cache.pb.cc',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_http_gperf',
      'variables': {
        'instaweb_gperf_subdir': 'net/instaweb/http',
      },
      'sources': [
        'http/bot_checker.gperf',
      ],
      'includes': [
        'gperf.gypi',
      ]
    },
    {
      'target_name': 'instaweb_rewriter_html_gperf',
      'variables': {
        'instaweb_gperf_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        'rewriter/rewrite_filter_names.gperf',
      ],
      'includes': [
        'gperf.gypi',
      ]
    },
    {
      'target_name': 'instaweb_rewriter_html_option_gperf',
      'variables': {
        'instaweb_gperf_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        'rewriter/rewrite_option_names.gperf',
      ],
      'includes': [
        'gperf.gypi',
      ]
    },
    {
      # TODO: break this up into sub-libs (mocks, real, etc)
      'target_name': 'instaweb_util',
      'type': '<(library)',
      'dependencies': [
        'instaweb_core.gyp:instaweb_util_core',
        'instaweb_http',
        'instaweb_http_gperf',
        'instaweb_clientstate_pb',
        'instaweb_logging_pb',
        'instaweb_propcache_pb',
        '<(instaweb_root)/third_party/base64/base64.gyp:base64',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/libpagespeed/src/pagespeed/core/core.gyp:pagespeed_core',
        '<(DEPTH)/third_party/google-sparsehash/google-sparsehash.gyp:include',
      ],
      'sources': [
        # TODO(sligocki): Move http/ files to instaweb_http.
        'http/async_fetch.cc',
        'http/cache_url_async_fetcher.cc',
        'http/fake_url_async_fetcher.cc',
        'http/http_cache.cc',
        'http/http_dump_url_async_writer.cc',
        'http/http_dump_url_fetcher.cc',
        'http/http_dump_url_writer.cc',
        'http/http_response_parser.cc',
        'http/http_value.cc',
        'http/http_value_writer.cc',
        'http/inflating_fetch.cc',
        'http/log_record.cc',
        'http/meta_data.cc',
        'http/rate_controlling_url_async_fetcher.cc',
        'http/sync_fetcher_adapter.cc',
        'http/sync_fetcher_adapter_callback.cc',
        'http/url_async_fetcher.cc',
        'http/url_async_fetcher_stats.cc',
        'http/url_fetcher.cc',
        'http/url_pollable_async_fetcher.cc',
        'http/user_agent_matcher.cc',
        'http/wait_url_async_fetcher.cc',
        'http/wget_url_fetcher.cc',
        'http/write_through_http_cache.cc',

        'util/abstract_mutex.cc',
        'util/abstract_shared_mem.cc',
        'util/async_cache.cc',
        'util/cache_batcher.cc',
        'util/cache_interface.cc',
        'util/cache_stats.cc',
        'util/checking_thread_system.cc',
        'util/chunking_writer.cc',
        'util/circular_buffer.cc',
        'util/client_state.cc',
        'util/condvar.cc',
        'util/data_url.cc',
        'util/debug.cc',
        'util/escaping.cc',
        'util/file_cache.cc',
        'util/file_system.cc',
        'util/file_system_lock_manager.cc',
        'util/file_writer.cc',
        'util/filename_encoder.cc',
        'util/function.cc',
        'util/google_timer.cc',
        'util/gzip_inflater.cc',
        'util/hashed_referer_statistics.cc',
        'util/hasher.cc',
        'util/inprocess_shared_mem.cc',
        'util/lru_cache.cc',
        'util/md5_hasher.cc',
        'util/mock_hasher.cc',
        'util/mock_message_handler.cc',
        'util/named_lock_manager.cc',
        'util/null_message_handler.cc',
        'util/null_mutex.cc',
        'util/null_rw_lock.cc',
        'util/null_shared_mem.cc',
        'util/null_statistics.cc',
        'util/null_writer.cc',
        'util/property_cache.cc',
        'util/queued_alarm.cc',
        'util/queued_worker.cc',
        'util/queued_worker_pool.cc',
        'util/ref_counted.cc',
        'util/rolling_hash.cc',
        'util/scheduler.cc',
        'util/scheduler_based_abstract_lock.cc',
        'util/shared_circular_buffer.cc',
        'util/shared_dynamic_string_map.cc',
        'util/shared_mem_lock_manager.cc',
        'util/shared_mem_referer_statistics.cc',
        'util/shared_mem_statistics.cc',
        'util/simple_stats.cc',
        'util/scheduler_thread.cc',
        'util/slow_worker.cc',
        'util/split_statistics.cc',
        'util/split_writer.cc',
        'util/statistics.cc',
        'util/statistics_work_bound.cc',
        'util/stdio_file_system.cc',
        'util/thread.cc',
        'util/thread_synchronizer.cc',
        'util/thread_system.cc',
        'util/threadsafe_cache.cc',
        'util/time_util.cc',
        'util/url_escaper.cc',
        'util/url_multipart_encoder.cc',
        'util/url_to_filename_encoder.cc',
        'util/url_segment_encoder.cc',
        'util/waveform.cc',
        'util/worker.cc',
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
        'util/pthread_rw_lock.cc',
        'util/pthread_shared_mem.cc',
        'util/pthread_thread_system.cc',
      ],
      'conditions': [
        ['support_posix_shared_mem != 1', {
          'sources!' : [
            'util/pthread_shared_mem.cc',
          ],
        }]
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
        '<(DEPTH)/base/base.gyp:base',
        'instaweb_core.gyp:http_core',
        'instaweb_http_pb',
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
        'instaweb_flush_early_pb',
        'instaweb_panel_config_pb',
        'instaweb_rewriter_html_gperf',
        'instaweb_rewriter_html_option_gperf',
        'instaweb_rewriter_pb',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        'rewriter/blink_critical_line_data_finder.cc',
        'rewriter/critical_images_finder.cc',
        'rewriter/domain_lawyer.cc',
        'rewriter/resource.cc',
        'rewriter/rewrite_options.cc',
        'rewriter/output_resource.cc',
        'rewriter/resource_namer.cc',
        'rewriter/resource_manager.cc',
        'rewriter/static_javascript_manager.cc',
        'rewriter/url_namer.cc',
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
      'target_name': 'instaweb_rewriter_image',
      'type': '<(library)',
      'dependencies': [
        'instaweb_rewriter_base',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/libpagespeed/src/pagespeed/image_compression/image_compression.gyp:pagespeed_image_converter',
        '<(DEPTH)/third_party/libpagespeed/src/pagespeed/image_compression/image_compression.gyp:pagespeed_jpeg_optimizer',
        '<(DEPTH)/third_party/libpagespeed/src/pagespeed/image_compression/image_compression.gyp:pagespeed_jpeg_utils',
        '<(DEPTH)/third_party/libpagespeed/src/pagespeed/image_compression/image_compression.gyp:pagespeed_png_optimizer',
        '<(DEPTH)/third_party/libjpeg/libjpeg.gyp:libjpeg',
        '<(DEPTH)/third_party/libwebp/libwebp.gyp:libwebp',
        '<(DEPTH)/third_party/opencv/opencv.gyp:highgui',
      ],
      'sources': [
        'rewriter/image.cc',
        'rewriter/image_url_encoder.cc',
        'rewriter/resource_tag_scanner.cc',
        'rewriter/webp_optimizer.cc',
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
        '<(DEPTH)/third_party/libpagespeed/src/pagespeed/js/js.gyp:pagespeed_jsminify',
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
        'instaweb_spriter',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/css_parser/css_parser.gyp:css_parser',
      ],
      'sources': [
        'rewriter/association_transformer.cc',
        'rewriter/css_combine_filter.cc',
        'rewriter/css_filter.cc',
        'rewriter/css_hierarchy.cc',
        'rewriter/css_image_rewriter.cc',
        'rewriter/css_inline_import_to_link_filter.cc',
        'rewriter/css_minify.cc',
        'rewriter/css_resource_slot.cc',
        'rewriter/css_url_counter.cc',
        'rewriter/css_url_encoder.cc',
        'rewriter/css_util.cc',
        'rewriter/image_combine_filter.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
        '<(DEPTH)/third_party/css_parser/src',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
    },
    {
      'target_name': 'instaweb_javascript_gperf',
      'variables': {
        'instaweb_gperf_subdir': 'net/instaweb/js',
      },
      'sources': [
        'js/js_keywords.gperf',
      ],
      'includes': [
        'gperf.gypi',
      ]
    },
    {
      'target_name': 'instaweb_javascript',
      'type': '<(library)',
      'dependencies': [
        'instaweb_util',
        'instaweb_javascript_gperf',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        'js/js_lexer.cc',
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
      'target_name': 'instaweb_rewriter',
      'type': '<(library)',
      'dependencies': [
        'instaweb_client_domain_rewriter_data2c',
        'instaweb_client_domain_rewriter_opt_data2c',
        'instaweb_core.gyp:instaweb_rewriter_html',
        'instaweb_blink_critical_line_data_pb',
        'instaweb_http',
        'instaweb_defer_iframe_data2c',
        'instaweb_defer_iframe_opt_data2c',
        'instaweb_delay_images_data2c',
        'instaweb_delay_images_opt_data2c',
        'instaweb_delay_images_inline_data2c',
        'instaweb_delay_images_inline_opt_data2c',
        'instaweb_detect_reflow_data2c',
        'instaweb_detect_reflow_opt_data2c',
        'instaweb_deterministic_data2c',
        'instaweb_deterministic_opt_data2c',
        'instaweb_flush_early_pb',
        'instaweb_js_defer_data2c',
        'instaweb_js_defer_opt_data2c',
        'instaweb_lazyload_images_data2c',
        'instaweb_lazyload_images_opt_data2c',
        'instaweb_local_storage_cache_data2c',
        'instaweb_local_storage_cache_opt_data2c',
        'instaweb_panel_config_pb',
        'instaweb_rewriter_base',
        'instaweb_rewriter_css',
        'instaweb_rewriter_image',
        'instaweb_rewriter_javascript',
        'instaweb_spriter',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/css_parser/css_parser.gyp:css_parser',
        '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
        '<(DEPTH)/third_party/re2/re2.gyp:re2',
      ],
      'sources': [
        'rewriter/add_head_filter.cc',
        'rewriter/add_instrumentation_filter.cc',
        'rewriter/ajax_rewrite_context.cc',
        'rewriter/base_tag_filter.cc',
        'rewriter/blink_background_filter.cc',
        'rewriter/blink_filter.cc',
        'rewriter/blink_util.cc',
        'rewriter/cache_extender.cc',
        'rewriter/common_filter.cc',
        'rewriter/collect_flush_early_content_filter.cc',
        'rewriter/collect_subresources_filter.cc',
        'rewriter/critical_images_callback.cc',
        'rewriter/css_inline_filter.cc',
        'rewriter/css_move_to_head_filter.cc',
        'rewriter/css_outline_filter.cc',
        'rewriter/css_tag_scanner.cc',
        'rewriter/data_url_input_resource.cc',
        'rewriter/defer_iframe_filter.cc',
        'rewriter/delay_images_filter.cc',
        'rewriter/detect_reflow_js_defer_filter.cc',
        'rewriter/deterministic_js_filter.cc',
        'rewriter/div_structure_filter.cc',
        'rewriter/domain_rewrite_filter.cc',
        'rewriter/file_input_resource.cc',
        'rewriter/flush_early_content_writer_filter.cc',
        'rewriter/flush_html_filter.cc',
        'rewriter/furious_matcher.cc',
        'rewriter/furious_util.cc',
        'rewriter/google_analytics_filter.cc',
        'rewriter/handle_noscript_redirect_filter.cc',
        'rewriter/image_rewrite_filter.cc',
        'rewriter/inline_rewrite_context.cc',
        'rewriter/insert_dns_prefetch_filter.cc',
        'rewriter/insert_ga_filter.cc',
        'rewriter/js_combine_filter.cc',
        'rewriter/js_defer_disabled_filter.cc',
        'rewriter/js_disable_filter.cc',
        'rewriter/js_inline_filter.cc',
        'rewriter/js_outline_filter.cc',
        'rewriter/lazyload_images_filter.cc',
        'rewriter/local_storage_cache_filter.cc',
        'rewriter/meta_tag_filter.cc',
        'rewriter/resource_combiner.cc',
        'rewriter/resource_slot.cc',
        'rewriter/resource_tag_scanner.cc',
        'rewriter/rewrite_context.cc',
        'rewriter/rewrite_driver.cc',
        'rewriter/rewrite_driver_factory.cc',
        'rewriter/rewrite_filter.cc',
        'rewriter/rewrite_query.cc',
        'rewriter/rewrite_stats.cc',
        'rewriter/scan_filter.cc',
        'rewriter/script_tag_scanner.cc',
        'rewriter/simple_text_filter.cc',
        'rewriter/single_rewrite_context.cc',
        'rewriter/strip_non_cacheable_filter.cc',
        'rewriter/strip_scripts_filter.cc',
        'rewriter/support_noscript_filter.cc',
        'rewriter/suppress_prehead_filter.cc',
        'rewriter/url_input_resource.cc',
        'rewriter/url_left_trim_filter.cc',
        'rewriter/url_partnership.cc',
        'rewriter/usage_data_reporter.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)/third_party/css_parser/src',
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
      'target_name': 'instaweb_http_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/http',
      },
      'sources': [
        'http/http.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/http.pb.cc',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_logging_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/http',
      },
      'sources': [
        'http/logging.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/logging.pb.cc',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_automatic',
      'type': '<(library)',
      'dependencies': [
        'instaweb_blink_critical_line_data_pb',
        'instaweb_http',
        'instaweb_flush_early_pb',
        'instaweb_panel_config_pb',
        'instaweb_rewriter',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
        '<(DEPTH)/third_party/libpagespeed/src/pagespeed/js/js.gyp:pagespeed_jsminify',
      ],
      'sources': [
        'automatic/blink_flow.cc',
        'automatic/blink_flow_critical_line.cc',
        'automatic/flush_early_flow.cc',
        'automatic/html_detector.cc',
        'automatic/proxy_fetch.cc',
        'automatic/proxy_interface.cc',
        'automatic/resource_fetch.cc',
      ],
    },
    {
      # TODO(sligocki): Why is this called "automatic" util?
      'target_name': 'automatic_util',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        'instaweb_util',
       ],
      'sources': [
        'rewriter/rewrite_gflags.cc',
        'util/gflags.cc',
        'util/google_timer.cc',
      ],
    },
    {
      'target_name': 'process_context',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        'instaweb_rewriter',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
       ],
      'sources': [
        'rewriter/process_context.cc',
      ],
    },
  ],
}
