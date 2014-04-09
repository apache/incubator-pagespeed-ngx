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
    'js_minify':
        '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)js_minify<(EXECUTABLE_SUFFIX)',
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
      'target_name': 'instaweb_add_instrumentation_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'add_instrumentation',
      },
      'sources': [
        'genfiles/rewriter/add_instrumentation_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_add_instrumentation_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'add_instrumentation_opt',
      },
      'sources': [
        'genfiles/rewriter/add_instrumentation_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_extended_instrumentation_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'extended_instrumentation',
      },
      'sources': [
        'genfiles/rewriter/extended_instrumentation_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_extended_instrumentation_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'extended_instrumentation_opt',
      },
      'sources': [
        'genfiles/rewriter/extended_instrumentation_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_client_domain_rewriter_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'client_domain_rewriter',
      },
      'sources': [
        'genfiles/rewriter/client_domain_rewriter_dbg.js',
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
      'target_name': 'instaweb_critical_css_beacon_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'critical_css_beacon',
      },
      'sources': [
        'genfiles/rewriter/critical_css_beacon_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_critical_css_beacon_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'critical_css_beacon_opt',
      },
      'sources': [
        'genfiles/rewriter/critical_css_beacon_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_critical_css_loader_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'critical_css_loader',
      },
      'sources': [
        'genfiles/rewriter/critical_css_loader_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_critical_css_loader_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'critical_css_loader_opt',
      },
      'sources': [
        'genfiles/rewriter/critical_css_loader_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_critical_images_beacon_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'critical_images_beacon',
      },
      'sources': [
        'genfiles/rewriter/critical_images_beacon_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_critical_images_beacon_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'critical_images_beacon_opt',
      },
      'sources': [
        'genfiles/rewriter/critical_images_beacon_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_dedup_inlined_images_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'dedup_inlined_images',
      },
      'sources': [
        'genfiles/rewriter/dedup_inlined_images_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_dedup_inlined_images_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'dedup_inlined_images_opt',
      },
      'sources': [
        'genfiles/rewriter/dedup_inlined_images_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_defer_iframe_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'defer_iframe',
      },
      'sources': [
        'genfiles/rewriter/defer_iframe_dbg.js',
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
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'delay_images',
      },
      'sources': [
        'genfiles/rewriter/delay_images_dbg.js',
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
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'delay_images_inline',
      },
      'sources': [
        'genfiles/rewriter/delay_images_inline_dbg.js',
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
      'target_name': 'instaweb_deterministic_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'deterministic',
      },
      'sources': [
        'genfiles/rewriter/deterministic_dbg.js',
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
      'target_name': 'instaweb_ghost_click_buster_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'ghost_click_buster_opt',
      },
      'sources': [
        'genfiles/rewriter/ghost_click_buster_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_split_html_beacon_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'split_html_beacon',
      },
      'sources': [
        'genfiles/rewriter/split_html_beacon_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_split_html_beacon_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'split_html_beacon_opt',
      },
      'sources': [
        'genfiles/rewriter/split_html_beacon_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_panel_loader_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'panel_loader_opt',
      },
      'sources': [
        'genfiles/rewriter/panel_loader_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_js_defer_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'js_defer',
      },
      'sources': [
        'genfiles/rewriter/js_defer_dbg.js',
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
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'lazyload_images',
      },
      'sources': [
        'genfiles/rewriter/lazyload_images_dbg.js',
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
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'local_storage_cache',
      },
      'sources': [
        'genfiles/rewriter/local_storage_cache_dbg.js',
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
      'target_name': 'instaweb_console_js_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'console_js',
      },
      'sources': [
        'genfiles/rewriter/console_js_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_console_js_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'console_js_opt',
      },
      'sources': [
        'genfiles/rewriter/console_js_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    # Deprecated. TODO(sligocki): Remove.
    {
      'target_name': 'instaweb_console_js_old_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/apache/install/mod_pagespeed_example',
        'instaweb_js_subdir': 'net/instaweb/genfiles/mod_pagespeed_console',
        'var_name': 'mod_pagespeed_console_js',
      },
      'sources': [
        'genfiles/mod_pagespeed_console/mod_pagespeed_console.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_console_css_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': 'net/instaweb/genfiles/rewriter',
        'var_name': 'console_css',
      },
      'sources': [
        'genfiles/rewriter/console_css.css',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    # Deprecated. TODO(sligocki): Remove.
    {
      'target_name': 'instaweb_console_css_old_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/apache/install/mod_pagespeed_example',
        'instaweb_js_subdir': 'net/instaweb/genfiles/mod_pagespeed_console',
        'var_name': 'mod_pagespeed_console_css',
      },
      'sources': [
        'genfiles/mod_pagespeed_console/mod_pagespeed_console.css',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    # Deprecated. TODO(sligocki): Remove.
    {
      'target_name': 'instaweb_console_body_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/apache/install/mod_pagespeed_example',
        'instaweb_js_subdir': 'net/instaweb/genfiles/mod_pagespeed_console',
        'var_name': 'mod_pagespeed_console_body',
      },
      'sources': [
        'genfiles/mod_pagespeed_console/mod_pagespeed_console.html',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    # Deprecated. TODO(sligocki): Remove.
    {
      'target_name': 'instaweb_console',
      'type': '<(library)',
      'dependencies': [
        ':instaweb_console_css_old_data2c',
        ':instaweb_console_js_old_data2c',
        ':instaweb_console_body_data2c',
      ],
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
        'instaweb_spriter_pb',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:proto_util',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
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
      'dependencies': [
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http_pb',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_critical_css_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/critical_css.pb.cc',
        'rewriter/critical_css.proto',
      ],
      'dependencies': [
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_rendered_image_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/rendered_image.pb.cc',
        'rewriter/rendered_image.proto',
      ],
      'dependencies': [
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_critical_images_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/critical_images.pb.cc',
        'rewriter/critical_images.proto',
      ],
      'dependencies': [
        'instaweb_critical_keys_pb',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_critical_keys_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/critical_keys.pb.cc',
        'rewriter/critical_keys.proto',
      ],
      'dependencies': [
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_critical_line_info_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/critical_line_info.pb.cc',
        'rewriter/critical_line_info.proto',
      ],
      'dependencies': [
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_cache_html_info_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        'rewriter/cache_html_info.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/cache_html_info.pb.cc',
      ],
      'dependencies': [
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http_pb',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_http',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http',
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
      'target_name': 'instaweb_rewriter_html_gperf',
      'variables': {
        'instaweb_gperf_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        'rewriter/rewrite_filter_names.gperf',
      ],
      'dependencies': [
        '<(DEPTH)/pagespeed/kernel.gyp:util',
        '<(DEPTH)/pagespeed/kernel.gyp:proto_util',
      ],
      'includes': [
        'gperf.gypi',
      ],
    },
    {
      # TODO: break this up into sub-libs (mocks, real, etc)
      'target_name': 'instaweb_util',
      'type': '<(library)',
      'dependencies': [
        'instaweb_logging_pb',
        'instaweb_propcache_pb',
        '<(instaweb_root)/third_party/base64/base64.gyp:base64',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_base_core',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_cache',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_sharedmem',
        '<(DEPTH)/pagespeed/kernel.gyp:util',
        '<(DEPTH)/pagespeed/kernel.gyp:proto_util',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
      ],
      'sources': [
        # TODO(sligocki): Move http/ files to pagespeed_fetch.
        'http/async_fetch.cc',
        'http/async_fetch_with_lock.cc',
        'http/cache_url_async_fetcher.cc',
        'http/external_url_fetcher.cc',
        'http/http_cache.cc',
        'http/http_dump_url_async_writer.cc',
        'http/http_dump_url_fetcher.cc',
        'http/http_response_parser.cc',
        'http/http_value.cc',
        'http/http_value_writer.cc',
        'http/inflating_fetch.cc',
        'http/log_record.cc',
        'http/rate_controller.cc',
        'http/rate_controlling_url_async_fetcher.cc',
        'http/request_context.cc',
        'http/sync_fetcher_adapter_callback.cc',
        'http/url_async_fetcher.cc',
        'http/url_async_fetcher_stats.cc',
        'http/wait_url_async_fetcher.cc',
        'http/wget_url_fetcher.cc',
        'http/write_through_http_cache.cc',

        'util/abstract_property_store_get_callback.cc',
        'util/cache_property_store.cc',
        'util/fallback_property_page.cc',
        'util/mock_property_page.cc',
        'util/property_cache.cc',
        'util/property_store.cc',
        'util/two_level_property_store.cc',
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
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http',
      ],
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
      'target_name': 'instaweb_http_test',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        'http/counting_url_async_fetcher.cc',
        '<(DEPTH)/pagespeed/kernel.gyp:kernel_test_util'
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
        'instaweb_rendered_image_pb',
        'instaweb_rewriter_html_gperf',
        'instaweb_rewriter_pb',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_cache',
        '<(DEPTH)/pagespeed/kernel.gyp:proto_util',
      ],
      'sources': [
        'config/rewrite_options_manager.cc',
        'rewriter/beacon_critical_images_finder.cc',
        'rewriter/beacon_critical_line_info_finder.cc',
        'rewriter/cache_html_info_finder.cc',
        'rewriter/critical_images_finder.cc',
        'rewriter/critical_line_info_finder.cc',
        'rewriter/device_properties.cc',
        'rewriter/domain_lawyer.cc',
        'rewriter/downstream_cache_purger.cc',
        'rewriter/downstream_caching_directives.cc',
        'rewriter/flush_early_info_finder.cc',
        'rewriter/output_resource.cc',
        'rewriter/request_properties.cc',
        'rewriter/resource.cc',
        'rewriter/resource_namer.cc',
        'rewriter/rewrite_options.cc',
        'rewriter/server_context.cc',
        'rewriter/static_asset_manager.cc',
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
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_image_processing',
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
      'target_name': 'js_minify',
      'type': 'executable',
      'sources': [
         'rewriter/js_minify_main.cc',
       ],
      'dependencies': [
        'instaweb_util',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:jsminify',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
    },
    {
      'target_name': 'instaweb_rewriter_javascript',
      'type': '<(library)',
      'dependencies': [
        'instaweb_rewriter_base',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:jsminify',
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
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http',
        '<(DEPTH)/third_party/css_parser/css_parser.gyp:css_parser',
      ],
      'sources': [
        'rewriter/association_transformer.cc',
        'rewriter/css_absolutify.cc',
        'rewriter/css_combine_filter.cc',
        'rewriter/css_filter.cc',
        'rewriter/css_hierarchy.cc',
        'rewriter/css_image_rewriter.cc',
        'rewriter/css_inline_import_to_link_filter.cc',
        'rewriter/css_minify.cc',
        'rewriter/css_resource_slot.cc',
        'rewriter/css_summarizer_base.cc',
        'rewriter/css_url_counter.cc',
        'rewriter/css_url_encoder.cc',
        'rewriter/css_util.cc',
        'rewriter/image_combine_filter.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
        '<(DEPTH)/third_party/css_parser/src',
        '<(DEPTH)/pagespeed/kernel.gyp:proto_util',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
    },
    {
      'target_name': 'instaweb_javascript',
      'type': '<(library)',
      'dependencies': [
        'instaweb_util',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_javascript_gperf',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [],
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
        'instaweb_add_instrumentation_data2c',
        'instaweb_add_instrumentation_opt_data2c',
        'instaweb_cache_html_info_pb',
        'instaweb_client_domain_rewriter_data2c',
        'instaweb_client_domain_rewriter_opt_data2c',
        'instaweb_console_css_data2c',
        'instaweb_console_js_data2c',
        'instaweb_console_js_opt_data2c',
        'instaweb_core.gyp:instaweb_rewriter_html',
        'instaweb_critical_css_beacon_data2c',
        'instaweb_critical_css_beacon_opt_data2c',
        'instaweb_critical_css_loader_data2c',
        'instaweb_critical_css_loader_opt_data2c',
        'instaweb_critical_css_pb',
        'instaweb_critical_images_beacon_data2c',
        'instaweb_critical_images_beacon_opt_data2c',
        'instaweb_critical_images_pb',
        'instaweb_critical_keys_pb',
        'instaweb_critical_line_info_pb',
        'instaweb_dedup_inlined_images_data2c',
        'instaweb_dedup_inlined_images_opt_data2c',
        'instaweb_defer_iframe_data2c',
        'instaweb_defer_iframe_opt_data2c',
        'instaweb_delay_images_data2c',
        'instaweb_delay_images_inline_data2c',
        'instaweb_delay_images_inline_opt_data2c',
        'instaweb_delay_images_opt_data2c',
        'instaweb_deterministic_data2c',
        'instaweb_deterministic_opt_data2c',
        'instaweb_extended_instrumentation_data2c',
        'instaweb_extended_instrumentation_opt_data2c',
        'instaweb_flush_early_pb',
        'instaweb_ghost_click_buster_opt_data2c',
        'instaweb_js_defer_data2c',
        'instaweb_js_defer_opt_data2c',
        'instaweb_lazyload_images_data2c',
        'instaweb_lazyload_images_opt_data2c',
        'instaweb_local_storage_cache_data2c',
        'instaweb_local_storage_cache_opt_data2c',
        'instaweb_panel_loader_opt_data2c',
        'instaweb_rewriter_base',
        'instaweb_rewriter_css',
        'instaweb_rewriter_image',
        'instaweb_rewriter_javascript',
        'instaweb_split_html_beacon_data2c',
        'instaweb_split_html_beacon_opt_data2c',
        'instaweb_spriter',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http',
        '<(DEPTH)/third_party/css_parser/css_parser.gyp:css_parser',
      ],
      'sources': [
        'rewriter/add_head_filter.cc',
        'rewriter/add_instrumentation_filter.cc',
        'rewriter/base_tag_filter.cc',
        'rewriter/blink_util.cc',
        'rewriter/cache_extender.cc',
        'rewriter/cache_html_filter.cc',
        'rewriter/cacheable_resource_base.cc',
        'rewriter/collect_flush_early_content_filter.cc',
        'rewriter/common_filter.cc',
        'rewriter/compute_visible_text_filter.cc',
        'rewriter/critical_css_beacon_filter.cc',
        'rewriter/critical_css_filter.cc',
        'rewriter/critical_css_finder.cc',
        'rewriter/critical_finder_support_util.cc',
        'rewriter/critical_images_beacon_filter.cc',
        'rewriter/critical_selector_filter.cc',
        'rewriter/critical_selector_finder.cc',
        'rewriter/css_inline_filter.cc',
        'rewriter/css_move_to_head_filter.cc',
        'rewriter/css_outline_filter.cc',
        'rewriter/css_tag_scanner.cc',
        'rewriter/data_url_input_resource.cc',
        'rewriter/debug_filter.cc',
        'rewriter/decode_rewritten_urls_filter.cc',
        'rewriter/dedup_inlined_images_filter.cc',
        'rewriter/defer_iframe_filter.cc',
        'rewriter/delay_images_filter.cc',
        'rewriter/deterministic_js_filter.cc',
        'rewriter/dom_stats_filter.cc',
        'rewriter/domain_rewrite_filter.cc',
        'rewriter/experiment_matcher.cc',
        'rewriter/experiment_util.cc',
        'rewriter/file_input_resource.cc',
        'rewriter/file_load_mapping.cc',
        'rewriter/file_load_policy.cc',
        'rewriter/file_load_rule.cc',
        'rewriter/fix_reflow_filter.cc',
        'rewriter/flush_early_content_writer_filter.cc',
        'rewriter/flush_html_filter.cc',
        'rewriter/google_analytics_filter.cc',
        'rewriter/google_font_css_inline_filter.cc',
        'rewriter/google_font_service_input_resource.cc',
        'rewriter/handle_noscript_redirect_filter.cc',
        'rewriter/image_rewrite_filter.cc',
        'rewriter/in_place_rewrite_context.cc',
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
        'rewriter/pedantic_filter.cc',
        'rewriter/property_cache_util.cc',
        'rewriter/redirect_on_size_limit_filter.cc',
        'rewriter/resource_combiner.cc',
        'rewriter/resource_fetch.cc',
        'rewriter/resource_slot.cc',
        'rewriter/resource_tag_scanner.cc',
        'rewriter/rewrite_context.cc',
        'rewriter/rewrite_driver.cc',
        'rewriter/rewrite_driver_factory.cc',
        'rewriter/rewrite_driver_pool.cc',
        'rewriter/rewrite_filter.cc',
        'rewriter/rewrite_query.cc',
        'rewriter/rewrite_stats.cc',
        'rewriter/rewritten_content_scanning_filter.cc',
        'rewriter/scan_filter.cc',
        'rewriter/script_tag_scanner.cc',
        'rewriter/simple_text_filter.cc',
        'rewriter/single_rewrite_context.cc',
        'rewriter/split_html_beacon_filter.cc',
        'rewriter/split_html_config.cc',
        'rewriter/split_html_filter.cc',
        'rewriter/split_html_helper_filter.cc',
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
      'target_name': 'instaweb_image_types_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        'rewriter/image_types.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/image_types.pb.cc',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_util_enums_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/util',
      },
      'sources': [
        'util/enums.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/enums.pb.cc',
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
      'dependencies': [
        'instaweb_image_types_pb',
        'instaweb_util_enums_pb',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_automatic',
      'type': '<(library)',
      'dependencies': [
        'instaweb_cache_html_info_pb',
        'instaweb_critical_css_pb',
        'instaweb_critical_keys_pb',
        'instaweb_critical_line_info_pb',
        'instaweb_flush_early_pb',
        'instaweb_rewriter',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:jsminify',
      ],
      'sources': [
        'automatic/cache_html_flow.cc',
        'automatic/flush_early_flow.cc',
        'automatic/html_detector.cc',
        'automatic/proxy_fetch.cc',
        'automatic/proxy_interface.cc',
      ],
    },
    {
      'target_name': 'instaweb_system',
      'type': '<(library)',
      'dependencies': [
        'instaweb_console',
        'instaweb_util',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http',
        '<(DEPTH)/third_party/apr/apr.gyp:include',
        '<(DEPTH)/third_party/aprutil/aprutil.gyp:include',
        '<(DEPTH)/third_party/domain_registry_provider/src/domain_registry/'
               + 'domain_registry.gyp:init_registry_tables_lib',
      ],
      'sources': [
        'system/apr_mem_cache.cc',
        'system/apr_thread_compatible_pool.cc',
        'system/in_place_resource_recorder.cc',
        'system/system_cache_path.cc',
        'system/system_caches.cc',
        'system/system_rewrite_driver_factory.cc',
        'system/system_rewrite_options.cc',
        'system/system_request_context.cc',
        'system/system_server_context.cc',
        'system/system_thread_system.cc',
        '<(DEPTH)/third_party/aprutil/apr_memcache2.c',
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
        '<(DEPTH)/pagespeed/kernel/util/gflags.cc',
      ],
    },
    {
      'target_name': 'process_context',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        'instaweb_rewriter',
        '<(DEPTH)/pagespeed/kernel.gyp:proto_util',
       ],
      'sources': [
        'rewriter/process_context.cc',
      ],
    },
  ],
}
