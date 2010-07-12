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
    'protobuf_src': '<(DEPTH)/third_party/protobuf2/src/src',
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out/instaweb',
  },
  'targets': [
    {
      'target_name': 'htmlparse',
      'type': '<(library)',
      'dependencies': [
        'util',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        'htmlparse/empty_html_filter.cc',
        'htmlparse/file_driver.cc',
        'htmlparse/file_statistics_log.cc',
        'htmlparse/html_element.cc',
        'htmlparse/html_event.cc',
        'htmlparse/html_filter.cc',
        'htmlparse/html_lexer.cc',
        'htmlparse/html_node.cc',
        'htmlparse/html_parse.cc',
        'htmlparse/html_writer_filter.cc',
        'htmlparse/logging_html_filter.cc',
        'htmlparse/null_filter.cc',
        'htmlparse/statistics_log.cc',
      ],
      'include_dirs': [
        '.',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
    },
    {
      'target_name': 'rewriter',
      'type': '<(library)',
      'dependencies': [
        'rewrite_pb',
        'util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/libpagespeed/src/pagespeed/image_compression/image_compression.gyp:pagespeed_jpeg_optimizer',
        '<(DEPTH)/third_party/libpagespeed/src/pagespeed/image_compression/image_compression.gyp:pagespeed_png_optimizer',
        '<(DEPTH)/third_party/opencv/opencv.gyp:highgui',
      ],
      'sources': [
        'rewriter/add_head_filter.cc',
        'rewriter/base_tag_filter.cc',
        'rewriter/cache_extender.cc',
        'rewriter/collapse_whitespace_filter.cc',
        'rewriter/css_combine_filter.cc',
        'rewriter/css_tag_scanner.cc',
        'rewriter/elide_attributes_filter.cc',
        'rewriter/file_input_resource.cc',
        'rewriter/hash_output_resource.cc',
        'rewriter/hash_resource_manager.cc',
        'rewriter/html_attribute_quote_removal.cc',
        'rewriter/image.cc',
        'rewriter/img_rewrite_filter.cc',
        'rewriter/img_tag_scanner.cc',
        'rewriter/input_resource.cc',
        'rewriter/outline_filter.cc',
        'rewriter/output_resource.cc',
        'rewriter/remove_comments_filter.cc',
        'rewriter/resource_manager.cc',
        'rewriter/resource_tag_scanner.cc',
        'rewriter/rewrite_driver.cc',
        'rewriter/rewrite_driver_factory.cc',
        'rewriter/rewrite_filter.cc',
        'rewriter/script_tag_scanner.cc',
        'rewriter/url_input_resource.cc',
      ],
      'include_dirs': [
        '.',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
    },
    {
      'target_name': 'util',
      'type': '<(library)',
      'dependencies': [
        'util_pb',
        '<(DEPTH)/base/base.gyp:base',
        '<(instaweb_root)/third_party/base64/base64.gyp:base64',
        '<(DEPTH)/third_party/libpagespeed/src/pagespeed/core/core.gyp:pagespeed_core',
      ],
      'sources': [
        'util/abstract_mutex.cc',
        'util/auto_make_dir_file_system.cc',
        'util/cache_interface.cc',
        'util/cache_url_async_fetcher.cc',
        'util/cache_url_fetcher.cc',
        'util/content_type.cc',
        'util/delay_controller.cc',
        'util/dummy_url_fetcher.cc',
        'util/fake_url_async_fetcher.cc',
        'util/file_cache.cc',
        'util/file_message_handler.cc',
        'util/file_system.cc',
        'util/file_writer.cc',
        'util/filename_encoder.cc',
        'util/hasher.cc',
        'util/http_cache.cc',
        'util/http_response_parser.cc',
        'util/lru_cache.cc',
        'util/message_handler.cc',
        'util/meta_data.cc',
        'util/mock_hasher.cc',
        'util/mock_timer.cc',
        'util/pthread_mutex.cc',
        'util/simple_meta_data.cc',
        'util/statistics.cc',
        'util/stdio_file_system.cc',
        'util/string_buffer.cc',
        'util/string_buffer_writer.cc',
        'util/string_util.cc',
        'util/string_writer.cc',
        'util/threadsafe_cache.cc',
        'util/timer.cc',
        'util/url_async_fetcher.cc',
        'util/url_fetcher.cc',
        'util/wget_url_fetcher.cc',
        'util/writer.cc',
      ],
      'include_dirs': [
        '.',
        '<(DEPTH)',
        '<(protobuf_src)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
          '<(DEPTH)',
          '<(protobuf_src)',
        ],
      },
    },
    {
      'target_name': 'rewriter_genproto',
      'type': 'none',
      'sources': [
        'rewriter/rewrite.proto',
      ],
      'rules': [
        {
          'rule_name': 'genproto',
          'extension': 'proto',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(protoc_out_dir)/net/instaweb/rewriter/<(RULE_INPUT_ROOT).pb.h',
            '<(protoc_out_dir)/net/instaweb/rewriter/<(RULE_INPUT_ROOT).pb.cc',
          ],
          'action': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            '--proto_path=rewriter',
            './rewriter/<(RULE_INPUT_ROOT)<(RULE_INPUT_EXT)',
            '--cpp_out=<(protoc_out_dir)/net/instaweb/rewriter',
          ],
          'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
        },
      ],
      'dependencies': [
        '<(DEPTH)/third_party/protobuf2/protobuf.gyp:protobuf_lite',
        '<(DEPTH)/third_party/protobuf2/protobuf.gyp:protoc#host',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(protoc_out_dir)',
        ]
      },
      'export_dependent_settings': [
        '<(DEPTH)/third_party/protobuf2/protobuf.gyp:protobuf_lite',
      ],
    },
    {
      'target_name': 'util_genproto',
      'type': 'none',
      'sources': [
        'util/util.proto',
      ],
      'rules': [
        {
          'rule_name': 'genproto',
          'extension': 'proto',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(protoc_out_dir)/net/instaweb/util/<(RULE_INPUT_ROOT).pb.h',
            '<(protoc_out_dir)/net/instaweb/util/<(RULE_INPUT_ROOT).pb.cc',
          ],
          'action': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            '--proto_path=util',
            './util/<(RULE_INPUT_ROOT)<(RULE_INPUT_EXT)',
            '--cpp_out=<(protoc_out_dir)/net/instaweb/util',
          ],
          'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
        },
      ],
      'dependencies': [
        '<(DEPTH)/third_party/protobuf2/protobuf.gyp:protobuf_lite',
        '<(DEPTH)/third_party/protobuf2/protobuf.gyp:protoc#host',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(protoc_out_dir)',
        ]
      },
      'export_dependent_settings': [
        '<(DEPTH)/third_party/protobuf2/protobuf.gyp:protobuf_lite',
      ],
    },
    {
      'target_name': 'rewrite_pb',
      'type': '<(library)',
      'hard_dependency': 1,
      'dependencies': [
        'rewriter_genproto',
        '<(DEPTH)/third_party/protobuf2/protobuf.gyp:protobuf_lite',
       ],
      'sources': [
        '<(protoc_out_dir)/net/instaweb/rewriter/rewrite.pb.cc',
      ],
      'export_dependent_settings': [
        'rewriter_genproto',
        '<(DEPTH)/third_party/protobuf2/protobuf.gyp:protobuf_lite',
      ]
    },
    {
      'target_name': 'util_pb',
      'type': '<(library)',
      'hard_dependency': 1,
      'dependencies': [
        'util_genproto',
        '<(DEPTH)/third_party/protobuf2/protobuf.gyp:protobuf_lite',
       ],
      'sources': [
        '<(protoc_out_dir)/net/instaweb/util/util.pb.cc',
      ],
      'export_dependent_settings': [
        'util_genproto',
        '<(DEPTH)/third_party/protobuf2/protobuf.gyp:protobuf_lite',
      ]
    },
  ],
}
