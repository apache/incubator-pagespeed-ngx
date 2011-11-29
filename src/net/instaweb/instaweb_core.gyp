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
      'target_name': 'instaweb_util_core',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
      'sources': [
        'util/atom.cc',
        'util/file_message_handler.cc',
        'util/google_message_handler.cc',
        'util/google_url.cc',
        'util/message_handler.cc',
        'util/query_params.cc',
        'util/string_convert.cc',
        'util/string_util.cc',
        'util/string_writer.cc',
        'util/symbol_table.cc',
        'util/timer.cc',
        'util/wildcard.cc',
        'util/wildcard_group.cc',
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
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
    },
    {
      'target_name': 'instaweb_htmlparse_core_gperf',
      'variables': {
        'instaweb_gperf_subdir': 'net/instaweb/htmlparse/',
      },
      'sources': [
        'htmlparse/html_name.gperf',
      ],
      'includes': [
        'gperf.gypi',
      ]
    },
    {
      'target_name': 'instaweb_htmlparse_core',
      'type': '<(library)',
      'dependencies': [
        'instaweb_util_core',
        'instaweb_htmlparse_core_gperf',
        'http_core',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
      'sources': [
        'htmlparse/doctype.cc',
        'htmlparse/empty_html_filter.cc',
        'htmlparse/explicit_close_tag.cc',
        'htmlparse/html_element.cc',
        'htmlparse/html_event.cc',
        'htmlparse/html_filter.cc',
        'htmlparse/html_keywords.cc',
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
      'target_name': 'http_core',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        'instaweb_core.gyp:instaweb_util_core',
      ],
      'sources': [
        'http/content_type.cc',
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
      'target_name': 'instaweb_rewriter_html_gperf',
      'variables': {
        'instaweb_gperf_subdir': 'net/instaweb/rewriter/',
      },
      'sources': [
        'rewriter/rewrite_option_names.gperf',
      ],
      'includes': [
        'gperf.gypi',
      ]
    },
    {
      'target_name': 'instaweb_rewriter_html',
      'type': '<(library)',
      'dependencies': [
        'instaweb_util_core',
        'instaweb_htmlparse_core',
        'instaweb_rewriter_html_gperf',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        'rewriter/collapse_whitespace_filter.cc',
        'rewriter/elide_attributes_filter.cc',
        'rewriter/file_load_policy.cc',
        'rewriter/html_attribute_quote_removal.cc',
        'rewriter/remove_comments_filter.cc',
        'rewriter/rewrite_options.cc'
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
        'instaweb_htmlparse_core',
      ]
    },
    # We build this target to make sure that we don't accidentially
    # introduce dependencies from the core libraries to non-core
    # libraries.
    {
      'target_name': 'html_minifier_main',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        'instaweb_util_core',
        'instaweb_htmlparse_core',
        'instaweb_rewriter_html',
      ],
      'sources': [
        'rewriter/html_minifier_main.cc',
      ],
    },
  ],
}
