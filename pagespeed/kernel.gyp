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
  'variables': {
    'instaweb_root': '..',
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out/instaweb',
    'protoc_executable':
        '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
    'data2c_out_dir': '<(SHARED_INTERMEDIATE_DIR)/data2c_out/instaweb',
    'data2c_exe':
        '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)instaweb_data2c<(EXECUTABLE_SUFFIX)',
    'js_minify':
        '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)js_minify<(EXECUTABLE_SUFFIX)',
    # Setting chromium_code to 1 turns on extra warnings. Also, if the compiler
    # is whitelisted in our common.gypi, those warnings will get treated as
    # errors.
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'pagespeed_base_core',
      # xcode build names libraries just based on the target_name, so
      # if this were merely base then its libbase.a would clash with
      # Chromium libbase.a
      'type': '<(library)',
      'sources': [
        'kernel/base/abstract_mutex.cc',
        'kernel/base/annotated_message_handler.cc',
        'kernel/base/atom.cc',
        'kernel/base/debug.cc',
        'kernel/base/file_message_handler.cc',
        'kernel/base/file_system.cc',
        'kernel/base/google_message_handler.cc',
        'kernel/base/message_handler.cc',
        'kernel/base/null_message_handler.cc',
        'kernel/base/null_mutex.cc',
        'kernel/base/null_shared_mem.cc',
        'kernel/base/null_writer.cc',
        'kernel/base/print_message_handler.cc',
        'kernel/base/statistics.cc',
        'kernel/base/stdio_file_system.cc',
        'kernel/base/string_convert.cc',
        'kernel/base/string_util.cc',
        'kernel/base/string_writer.cc',
        'kernel/base/symbol_table.cc',
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
      'target_name': 'pagespeed_base',
      # xcode build names libraries just based on the target_name, so
      # if this were merely base then its libbase.a would clash with
      # Chromium libbase.a
      'type': '<(library)',
      'sources': [
        'kernel/base/abstract_shared_mem.cc',
        'kernel/base/cache_interface.cc',
        'kernel/base/charset_util.cc',
        'kernel/base/checking_thread_system.cc',
        'kernel/base/chunking_writer.cc',
        'kernel/base/circular_buffer.cc',
        'kernel/base/condvar.cc',
        'kernel/base/countdown_timer.cc',
        'kernel/base/escaping.cc',
        'kernel/base/fast_wildcard_group.cc',
        'kernel/base/file_writer.cc',
        'kernel/base/function.cc',
        'kernel/base/hasher.cc',
        'kernel/base/hostname_util.cc',
        'kernel/base/json_writer.cc',
        'kernel/base/md5_hasher.cc',
        'kernel/base/mem_debug.cc',
        'kernel/base/named_lock_manager.cc',
        'kernel/base/null_rw_lock.cc',
        'kernel/base/null_statistics.cc',
        'kernel/base/posix_timer.cc',
        'kernel/base/request_trace.cc',
        'kernel/base/rolling_hash.cc',
        'kernel/base/sha1_signature.cc',
        'kernel/base/shared_string.cc',
        'kernel/base/signature.cc',
        'kernel/base/source_map.cc',
        'kernel/base/split_statistics.cc',
        'kernel/base/split_writer.cc',
        'kernel/base/thread.cc',
        'kernel/base/waveform.cc',
        'kernel/base/wildcard.cc',
      ],
      'dependencies': [
        'pagespeed_base_core',
        '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
        '<(DEPTH)/third_party/serf/select_openssl.gyp:select_openssl',
      ],
    },
    {
      'target_name': 'pagespeed_base_test_infrastructure',
      'type': '<(library)',
      'sources': [
        'kernel/base/counting_writer.cc',
        'kernel/base/mem_file_system.cc',
        'kernel/base/mock_hasher.cc',
        'kernel/base/mock_message_handler.cc',
        'kernel/base/mock_timer.cc',
        'kernel/base/null_thread_system.cc',
        'kernel/cache/delay_cache.cc',
        'kernel/cache/mock_time_cache.cc',
        'kernel/html/canonical_attributes.cc',
        'kernel/html/explicit_close_tag.cc',
        'kernel/thread/mock_scheduler.cc',
        'kernel/util/platform.cc',
        'kernel/util/simple_stats.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        ':pthread_system',
        ':util',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/base/base.gyp:base'
      ],
    },
    {
      'target_name': 'kernel_test_util',
      'type': '<(library)',
      'sources': [
        'kernel/base/file_system_test_base.cc',
        'kernel/base/gtest.cc',
        'kernel/base/message_handler_test_base.cc',
        'kernel/cache/cache_spammer.cc',
        'kernel/http/user_agent_matcher_test_base.cc',
        'kernel/sharedmem/shared_circular_buffer_test_base.cc',
        'kernel/sharedmem/shared_dynamic_string_map_test_base.cc',
        'kernel/sharedmem/shared_mem_cache_data_test_base.cc',
        'kernel/sharedmem/shared_mem_cache_test_base.cc',
        'kernel/sharedmem/shared_mem_lock_manager_test_base.cc',
        'kernel/sharedmem/shared_mem_statistics_test_base.cc',
        'kernel/sharedmem/shared_mem_test_base.cc',
        'kernel/thread/thread_system_test_base.cc',
        'kernel/thread/worker_test_base.cc',
        'kernel/util/lock_manager_spammer.cc',
        'kernel/util/mock_nonce_generator.cc',
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
        'mem_lock',
        'pagespeed_sharedmem',
        'util',
      ],
    },
    {
      'target_name': 'pagespeed_cache',
      'type': '<(library)',
      'sources': [
        'kernel/cache/async_cache.cc',
        'kernel/cache/cache_batcher.cc',
        'kernel/cache/cache_stats.cc',
        'kernel/cache/compressed_cache.cc',
        'kernel/cache/delegating_cache_callback.cc',
        'kernel/cache/fallback_cache.cc',
        'kernel/cache/file_cache.cc',
        'kernel/cache/in_memory_cache.cc',
        'kernel/cache/key_value_codec.cc',
        'kernel/cache/lru_cache.cc',
        'kernel/cache/purge_context.cc',
        'kernel/cache/purge_set.cc',
        'kernel/cache/threadsafe_cache.cc',
        'kernel/cache/write_through_cache.cc',
       ],
      'dependencies': [
        'pagespeed_base',
        '<(DEPTH)/third_party/rdestl/rdestl.gyp:rdestl',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
    },
    {
      'target_name': 'pagespeed_html_gperf',
      'variables': {
        'instaweb_gperf_subdir': 'kernel/html',
        'instaweb_root':  '<(DEPTH)/pagespeed',
      },
      'sources': [
        'kernel/html/html_name.gperf',
      ],
      # TODO(morlovich): Move gperf.gypi to pagespeed/, changing all
      # references in net/instaweb gyp files.
      'includes': [
        '../net/instaweb/gperf.gypi',
      ],
    },
    {
      'target_name': 'pagespeed_html',
      'type': '<(library)',
      'sources': [
        'kernel/html/amp_document_filter.cc',
        'kernel/html/elide_attributes_filter.cc',
        'kernel/html/collapse_whitespace_filter.cc',
        'kernel/html/doctype.cc',
        'kernel/html/empty_html_filter.cc',
        'kernel/html/html_attribute_quote_removal.cc',
        'kernel/html/html_element.cc',
        'kernel/html/html_event.cc',
        'kernel/html/html_filter.cc',
        'kernel/html/html_keywords.cc',
        'kernel/html/html_lexer.cc',
        'kernel/html/html_node.cc',
        'kernel/html/html_parse.cc',
        'kernel/html/html_writer_filter.cc',
        'kernel/html/remove_comments_filter.cc',
      ],
      'dependencies': [
        ':pagespeed_base_core',
        ':pagespeed_html_gperf',
        ':pagespeed_http_core',
      ],
    },
    {
      'target_name': 'pagespeed_http_core',
      'type': '<(library)',
      'sources': [
        'kernel/http/caching_headers.cc',
        'kernel/http/content_type.cc',
        'kernel/http/google_url.cc',
        'kernel/http/http_names.cc',
        'kernel/http/query_params.cc',
        'kernel/http/semantic_type.cc',
      ],
      'dependencies': [
        'pagespeed_base_core',
        '<(DEPTH)/url/url.gyp:url_lib',
      ],
    },
    {
      'target_name': 'pagespeed_http',
      'type': '<(library)',
      'sources': [
        'kernel/http/data_url.cc',
        'kernel/http/domain_registry.cc',
        'kernel/http/headers.cc',
        'kernel/http/http_options.cc',
        'kernel/http/response_headers_parser.cc',
        'kernel/http/response_headers.cc',
        'kernel/http/request_headers.cc',
        'kernel/http/user_agent_matcher.cc',
        'kernel/http/user_agent_normalizer.cc',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/domain_registry_provider/src/domain_registry/domain_registry.gyp:init_registry_tables_lib',
        'pagespeed_http_core',
        'pagespeed_http_gperf',
        'pagespeed_http_pb',
        'util',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
    },
    {
      'target_name': 'pagespeed_http_gperf',
      'variables': {
        'instaweb_gperf_subdir': 'kernel/http',
        'instaweb_root':  '<(DEPTH)/pagespeed',
      },
      'sources': [
        'kernel/http/bot_checker.gperf',
      ],
      'includes': [
        '../net/instaweb/gperf.gypi',
      ]
    },
    {
      'target_name': 'pagespeed_proto_matcher_test_proto',
      'variables': {
        'instaweb_protoc_subdir': 'pagespeed/kernel/base',
      },
      'sources': [
        'kernel/base/proto_matcher_test.proto',
      ],
      'includes': [
        '../net/instaweb/protoc.gypi',
      ],
    },
    {
      'target_name': 'pagespeed_http_pb',
      'variables': {
        'instaweb_protoc_subdir': 'pagespeed/kernel/http',
      },
      'sources': [
        'kernel/http/http.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/http.pb.cc',
      ],
      'includes': [
        '../net/instaweb/protoc.gypi',
      ],
    },
    {
      'target_name': 'pagespeed_image_types_pb',
      'variables': {
        'instaweb_protoc_subdir': 'pagespeed/kernel/http',
      },
      'sources': [
        'kernel/http/image_types.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/image_types.pb.cc',
      ],
      'includes': [
        '../net/instaweb/protoc.gypi',
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
        'pagespeed_base_core',
        'js_tokenizer',
        'pagespeed_javascript_gperf',
      ],
    },
    {
      'target_name': 'js_tokenizer',
      'type': '<(library)',
      'sources': [
        'kernel/js/js_tokenizer.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        'pagespeed_base_core',
        'pagespeed_javascript_gperf',
        '<(DEPTH)/third_party/re2/re2.gyp:re2',
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
        'kernel/util/file_system_lock_manager.cc',
        'kernel/util/gzip_inflater.cc',
        'kernel/util/hashed_nonce_generator.cc',
        'kernel/util/input_file_nonce_generator.cc',
        'kernel/util/nonce_generator.cc',
        'kernel/util/simple_random.cc',
        'kernel/util/statistics_logger.cc',
        'kernel/util/url_escaper.cc',
        'kernel/util/url_multipart_encoder.cc',
        'kernel/util/url_segment_encoder.cc',
        'kernel/util/url_to_filename_encoder.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        'pagespeed_base',
        'pagespeed_thread',
        '<(DEPTH)/third_party/re2/re2.gyp:re2',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
        '<(DEPTH)/url/url.gyp:url_lib',
      ],
    },
    {
      'target_name': 'mem_lock',
      'type': '<(library)',
      'sources': [
        'kernel/util/mem_lock.cc',
        'kernel/util/mem_lock_manager.cc',
        'kernel/util/mem_lock_state.cc',
        'kernel/util/threadsafe_lock_manager.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        'pagespeed_base',
        'pagespeed_thread',
      ],
    },
    {
      'target_name': 'util_gflags',
      'type': '<(library)',
      'sources': [
        'kernel/util/gflags.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
      ],
    },
    {
      'target_name': 'redis_cache_cluster_setup_lib',
      'type': '<(library)',
      'sources': [
        'system/redis_cache_cluster_setup.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        'pagespeed_base',
        'tcp_connection_for_testing',
        '<(DEPTH)/third_party/apr/apr.gyp:apr',
      ],
    },
    {
      'target_name': 'redis_cache_cluster_setup',
      'type': 'executable',
      'sources': [
        'system/redis_cache_cluster_setup_main.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        'redis_cache_cluster_setup_lib',
        '<(DEPTH)/third_party/apr/apr.gyp:apr',
      ],
    },
    {
      'target_name': 'tcp_connection_for_testing',
      'type': '<(library)',
      'sources': [
        'system/tcp_connection_for_testing.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        'pagespeed_base',
        '../net/instaweb/instaweb.gyp:instaweb_system',
        '<(DEPTH)/third_party/apr/apr.gyp:apr',
      ],
    },
    {
      'target_name': 'tcp_server_thread_for_testing',
      'type': '<(library)',
      'sources': [
        'system/tcp_server_thread_for_testing.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        'pagespeed_base',
        'kernel_test_util',
        '<(DEPTH)/third_party/apr/apr.gyp:apr',
      ],
    },
    {
      'target_name': 'pagespeed_image_processing',
      'type': '<(library)',
      'dependencies': [
        ':pagespeed_image_optimizer_pb',
        ':pagespeed_image_types_pb',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/build/libwebp.gyp:libwebp_enc',
        '<(DEPTH)/build/libwebp.gyp:libwebp_dec',
        '<(DEPTH)/third_party/giflib/giflib.gyp:dgiflib',
        '<(DEPTH)/third_party/libjpeg_turbo/libjpeg_turbo.gyp:libjpeg_turbo',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
        '<(DEPTH)/third_party/optipng/optipng.gyp:opngreduc',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
      ],
      'sources': [
        'kernel/image/frame_interface_optimizer.cc',
        'kernel/image/gif_reader.cc',
        'kernel/image/image_analysis.cc',
        'kernel/image/image_converter.cc',
        'kernel/image/image_frame_interface.cc',
        'kernel/image/image_optimizer.cc',
        'kernel/image/image_resizer.cc',
        'kernel/image/image_util.cc',
        'kernel/image/jpeg_optimizer.cc',
        'kernel/image/jpeg_reader.cc',
        'kernel/image/jpeg_utils.cc',
        'kernel/image/pixel_format_optimizer.cc',
        'kernel/image/png_optimizer.cc',
        'kernel/image/read_image.cc',
        'kernel/image/scanline_interface_frame_adapter.cc',
        'kernel/image/scanline_utils.cc',
        'kernel/image/webp_optimizer.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
        '<(DEPTH)/third_party/libwebp/src',
      ],
      'msvs_disabled_warnings': [
        4996,  # std::string::copy() is deprecated on Windows, but we use it,
               # so we need to disable the warning.
      ],
      'export_dependent_settings': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/libjpeg_turbo/libjpeg_turbo.gyp:libjpeg_turbo',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
      ],
    },
    {
      'target_name': 'pagespeed_image_test_util',
      'type': '<(library)',
      'sources': [
        'kernel/image/gif_square.cc',
        'kernel/image/jpeg_optimizer_test_helper.cc',
        'kernel/image/test_utils.cc',
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
        ':kernel_test_util',
        ':pagespeed_image_processing',
        '<(DEPTH)/third_party/giflib/giflib.gyp:egiflib',
      ],
    },
    {
      'target_name': 'pagespeed_sharedmem_pb',
      'variables': {
        'instaweb_protoc_subdir': 'pagespeed/kernel/sharedmem',
      },
      'sources': [
        'kernel/sharedmem/shared_mem_cache_snapshot.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/shared_mem_cache_snapshot.pb.cc',
      ],
      'includes': [
        '../net/instaweb/protoc.gypi',
      ],
    },
    {
      'target_name': 'pagespeed_sharedmem',
      'type': '<(library)',
      'sources': [
        'kernel/sharedmem/inprocess_shared_mem.cc',
        'kernel/sharedmem/shared_circular_buffer.cc',
        'kernel/sharedmem/shared_dynamic_string_map.cc',
        'kernel/sharedmem/shared_mem_cache.cc',
        'kernel/sharedmem/shared_mem_cache_data.cc',
        'kernel/sharedmem/shared_mem_lock_manager.cc',
        'kernel/sharedmem/shared_mem_statistics.cc',
      ],
      'dependencies': [
        'pagespeed_base',
        'pagespeed_sharedmem_pb',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
    },
    {
      'target_name': 'pagespeed_thread',
      'type': '<(library)',
      'sources': [
        'kernel/thread/queued_alarm.cc',
        'kernel/thread/queued_worker.cc',
        'kernel/thread/queued_worker_pool.cc',
        'kernel/thread/scheduler.cc',
        'kernel/thread/scheduler_based_abstract_lock.cc',
        'kernel/thread/scheduler_sequence.cc',
        'kernel/thread/scheduler_thread.cc',
        'kernel/thread/sequence.cc',
        'kernel/thread/slow_worker.cc',
        'kernel/thread/thread_synchronizer.cc',
        'kernel/thread/worker.cc',
      ],
      'dependencies': [
        'pagespeed_base',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
    },
    {
      # We use a library called protobuf_full_do_not_use because it enables
      # protobuf introspection, which in turn enables us to show an admin page
      # with detail about the contents of caches.  This makes 64-bit Release
      # libmod_pagespeed.so 639k larger (8.5M vs 7.9M).  If we decide this is
      # not worth it, we could write and maintain hand-serializers for the
      # protobufs of interest, or find some less expensive way to display
      # the contents of protobufs in a human-readable form.
      #
      # The 'do_not_use' part of this name is only in reference to official
      # builds of Chrome.
      'target_name': 'proto_util',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_full_do_not_use',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_full_do_not_use',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
    },
    {
      'target_name': 'pthread_system',
      'type': '<(library)',
      'sources': [
        'kernel/thread/pthread_condvar.cc',
        'kernel/thread/pthread_mutex.cc',
        'kernel/thread/pthread_rw_lock.cc',
        'kernel/thread/pthread_shared_mem.cc',
        'kernel/thread/pthread_thread_system.cc',
      ],
      'conditions': [
        ['support_posix_shared_mem != 1', {
          'sources!' : [
            'kernel/thread/pthread_shared_mem.cc',
          ],
        }]
      ],
      'dependencies': [
        'pagespeed_base',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'ldflags': [
        '-lrt',
      ]
    },
    {
      'target_name': 'brotli',
      'type': '<(library)',
      'sources': [
        '<(DEPTH)/pagespeed/kernel/util/brotli_inflater.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/brotli/brotli.gyp:brotli_dec',
        '<(DEPTH)/third_party/brotli/brotli.gyp:brotli_enc',
      ],
      'cflags': [
        '-Wno-sign-compare', # Brotli header has some macros.
      ],
    },
    {
      'target_name': 'pagespeed_image_optimizer_pb',
      'variables': {
        'instaweb_protoc_subdir': 'pagespeed/kernel/image',
      },
      'sources': [
        'kernel/image/image_optimizer.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/image_optimizer.pb.cc',
      ],
      'includes': [
        '../net/instaweb/protoc.gypi',
      ],
    },
  ],
}
