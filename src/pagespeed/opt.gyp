# Copyright 2014 Google Inc.
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
    # Setting chromium_code to 1 turns on extra warnings. Also, if the compiler
    # is whitelisted in our common.gypi, those warnings will get treated as
    # errors.
    'chromium_code': 1,
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out/instaweb',
    'protoc_executable':
        '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
    'compiled_js_dir': '<(SHARED_INTERMEDIATE_DIR)/closure_out/instaweb',
    'data2c_out_dir': '<(SHARED_INTERMEDIATE_DIR)/data2c_out/instaweb',
    'data2c_exe':
        '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)instaweb_data2c<(EXECUTABLE_SUFFIX)',

  },
  'targets': [
    {
      'target_name': 'pagespeed_ads_util',
      'type': '<(library)',
      'sources': [
        'opt/ads/ads_util.cc',
        'opt/ads/ads_attribute.cc',
        'opt/ads/show_ads_snippet_parser.cc',
      ],
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_base_core',
        '<(DEPTH)/third_party/re2/re2.gyp:re2',
      ],
    },
    {
      'target_name': 'pagespeed_logging_enums_pb',
      'variables': {
        'instaweb_protoc_subdir': 'pagespeed/opt/logging',
      },
      'sources': [
        'opt/logging/enums.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/enums.pb.cc',
      ],
      'includes': [
        '../net/instaweb/protoc.gypi',
      ],
    },
    {
      'target_name': 'pagespeed_logging_pb',
      'variables': {
        'instaweb_protoc_subdir': 'pagespeed/opt/logging',
      },
      'sources': [
        'opt/logging/logging.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/logging.pb.cc',
      ],
      'includes': [
        '../net/instaweb/protoc.gypi',
      ],
      'dependencies': [
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_image_types_pb',
        'pagespeed_logging_enums_pb',
      ]
    },
    {
      'target_name': 'pagespeed_logging',
      'type': '<(library)',
      'dependencies': [
        'pagespeed_logging_pb',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_base_core',
      ],
      'sources': [
        'opt/logging/log_record.cc',
        'opt/logging/request_timing_info.cc',
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
        'pagespeed_logging_pb',
      ],
    },
    {
      'target_name': 'pagespeed_property_cache_pb',
      'variables': {
        'instaweb_protoc_subdir': 'pagespeed/opt/http',
      },
      'sources': [
        'opt/http/property_cache.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/property_cache.pb.cc',
      ],
      'includes': [
        '../net/instaweb/protoc.gypi',
      ],
    },
    {
      'target_name': 'pagespeed_opt_http',
      'type': '<(library)',
      'dependencies': [
        'pagespeed_property_cache_pb',
        'pagespeed_logging',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http',
      ],
      'sources': [
        'opt/http/abstract_property_store_get_callback.cc',
        'opt/http/cache_property_store.cc',
        'opt/http/fallback_property_page.cc',
        'opt/http/mock_property_page.cc',
        'opt/http/property_cache.cc',
        'opt/http/property_store.cc',
        'opt/http/request_context.cc',
        'opt/http/two_level_property_store.cc',
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
        'pagespeed_property_cache_pb',
      ],
    },

    {
      'target_name': 'mobilize_js_dbg',
      'variables': {
        'js_dir': 'opt/mobilize',
        'closure_build_type': 'dbg',
        'extra_closure_flags': [
          '--closure_entry_point=pagespeed.Mob',
          '--js', '<(instaweb_root)/third_party/closure_library',
          '--only_closure_dependencies',
          '--externs=opt/mobilize/externs.js',
        ],
        'js_includes' : [
          'opt/mobilize/mobilize_color.js',
          'opt/mobilize/mobilize_layout.js',
          'opt/mobilize/mobilize_logo.js',
          'opt/mobilize/mobilize_nav.js',
          'opt/mobilize/mobilize_theme.js',
          'opt/mobilize/mobilize_util.js',
        ],
      },
      'sources': [ 'opt/mobilize/mobilize.js' ],
      'includes': ['../net/instaweb/closure.gypi',],
    },
    {
      'target_name': 'mobilize_js_opt',
      'variables': {
        'js_dir': 'opt/mobilize',
        'extra_closure_flags': [
          '--closure_entry_point=pagespeed.Mob',
          '--js', '<(instaweb_root)/third_party/closure_library',
          '--only_closure_dependencies',
          '--externs=opt/mobilize/externs.js',
        ],
        'js_includes' : [
          'opt/mobilize/mobilize_color.js',
          'opt/mobilize/mobilize_layout.js',
          'opt/mobilize/mobilize_logo.js',
          'opt/mobilize/mobilize_nav.js',
          'opt/mobilize/mobilize_theme.js',
          'opt/mobilize/mobilize_util.js',
        ],
      },
      'sources': [ 'opt/mobilize/mobilize.js' ],
      'includes': ['../net/instaweb/closure.gypi'],
    },

    {
      'target_name': 'instaweb_mobilize_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/opt/mobilize',
        'instaweb_js_subdir': '<(compiled_js_dir)/opt/mobilize',
        'var_name': 'mobilize',
      },
      'sources': [
        '<(compiled_js_dir)/opt/mobilize/mobilize_dbg.js',
      ],
      'includes': ['../net/instaweb/data2c.gypi']
    },
    {
      'target_name': 'instaweb_mobilize_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/opt/mobilize',
        'instaweb_js_subdir': '<(compiled_js_dir)/opt/mobilize',
        'var_name': 'mobilize_opt',
      },
      'sources': [
        '<(compiled_js_dir)/opt/mobilize/mobilize_opt.js',
      ],
      'includes': ['../net/instaweb/data2c.gypi']
    },


    # TODO(jmarantz): Figure out how to factor out this boilerplate from
    # the similar lines above for mobilize.
    {
      'target_name': 'mobilize_xhr_js_dbg',
      'variables': {
        'js_dir': 'opt/mobilize',
        'closure_build_type': 'dbg',
        'extra_closure_flags': [
          '--closure_entry_point=pagespeed.XhrHijack',
          '--js', '<(instaweb_root)/third_party/closure_library',
          '--only_closure_dependencies',
          '--externs=opt/mobilize/externs.js',
        ],
      },
      'sources': ['opt/mobilize/mobilize_xhr.js'],
      'includes': ['../net/instaweb/closure.gypi',],
    },
    {
      'target_name': 'mobilize_xhr_js_opt',
      'variables': {
        'js_dir': 'opt/mobilize',
        'extra_closure_flags': [
          '--closure_entry_point=pagespeed.XhrHijack',
          '--js', '<(instaweb_root)/third_party/closure_library',
          '--only_closure_dependencies',
          '--externs=opt/mobilize/externs.js',
        ],
      },
      'sources': ['opt/mobilize/mobilize_xhr.js'],
      'includes': ['../net/instaweb/closure.gypi'],
    },

    {
      'target_name': 'instaweb_mobilize_xhr_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/opt/mobilize',
        'instaweb_js_subdir': '<(compiled_js_dir)/opt/mobilize',
        'var_name': 'mobilize_xhr',
      },
      'sources': [
        '<(compiled_js_dir)/opt/mobilize/mobilize_xhr_dbg.js',
      ],
      'includes': ['../net/instaweb/data2c.gypi']
    },
    {
      'target_name': 'instaweb_mobilize_xhr_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/opt/mobilize',
        'instaweb_js_subdir': '<(compiled_js_dir)/opt/mobilize',
        'var_name': 'mobilize_xhr_opt',
      },
      'sources': [
        '<(compiled_js_dir)/opt/mobilize/mobilize_xhr_opt.js',
      ],
      'includes': ['../net/instaweb/data2c.gypi']
    },

    {
      'target_name': 'instaweb_mobilize_css_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/opt/mobilize',
        'instaweb_js_subdir': 'opt/mobilize',
        'var_name': 'mobilize_css',
      },
      'sources': [
        'opt/mobilize/mobilize.css',
      ],
      'includes': ['../net/instaweb/data2c.gypi']
    },

  ]
}

