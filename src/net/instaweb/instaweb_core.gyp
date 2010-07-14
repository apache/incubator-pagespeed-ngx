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
  },
  'targets': [
    {
      'target_name': 'instaweb_util_core',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/protobuf2/protobuf.gyp:protobuf_lite',

        # TODO: we need base64 because we depend on string_util, but
        # not everyone that wants string_util wants base64. We should
        # split the base64 dependency from string_util.
        '<(instaweb_root)/third_party/base64/base64.gyp:base64',
      ],
      'sources': [
        'util/abstract_mutex.cc',
        'util/cache_interface.cc',
        'util/content_type.cc',
        'util/file_message_handler.cc',
        'util/file_system.cc',
        'util/hasher.cc',
        'util/message_handler.cc',
        'util/meta_data.cc',
        'util/string_util.cc',
        'util/string_writer.cc',
        'util/writer.cc',
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
        '<(DEPTH)/third_party/protobuf2/protobuf.gyp:protobuf_lite',
      ],
    },
    {
      'target_name': 'instaweb_htmlparse_core',
      'type': '<(library)',
      'dependencies': [
        'instaweb_util_core',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/protobuf2/protobuf.gyp:protobuf_lite',
      ],
      'sources': [
        'htmlparse/empty_html_filter.cc',
        'htmlparse/html_element.cc',
        'htmlparse/html_event.cc',
        'htmlparse/html_filter.cc',
        'htmlparse/html_lexer.cc',
        'htmlparse/html_node.cc',
        'htmlparse/html_parse.cc',
        'htmlparse/html_writer_filter.cc',
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
        'instaweb_util_core',
      ],
    },
    {
      'target_name': 'instaweb_rewriter_base',
      'type': '<(library)',
      'dependencies': [
        'instaweb_util_core',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/protobuf2/protobuf.gyp:protobuf_lite',
      ],
      'sources': [
        'rewriter/input_resource.cc',
        'rewriter/output_resource.cc',
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
      'target_name': 'instaweb_rewriter_html',
      'type': '<(library)',
      'dependencies': [
        'instaweb_util_core',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        'rewriter/collapse_whitespace_filter.cc',
        'rewriter/elide_attributes_filter.cc',
        'rewriter/html_attribute_quote_removal.cc',
        'rewriter/remove_comments_filter.cc',
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
