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

# Build the JS files using the closure compiler. Note that this supports two
# different modes, depending on how $BUILD_JS is set. If it's 1, the closure
# compiler will be used, otherwise precompiled JS files checked in to
# net/instaweb/genfiles are used. This allows most people building mod_pagespeed
# to avoid a dependency on having java installed.

# New JS files should be added to instaweb.gyp and should include the following
# fields in a 'variables' sections.
#   js_dir - The directory (rooted at net/instaweb) that the js files is in.
#   closure_build_type - dbg or opt to specify the level of compilation desired
#       Defaults to opt if omitted.
#   js_includes - Array of other files to include. Typically will just be
#       [ 'js/js_utils.js' ]
#   extra_closure_flags - Array of other flags to include when using closure
#       dependencies. A typical example:
#           'extra_closure_flags': [
#             # Define path to the open-source closure checkout.
#             '--js', '<(instaweb_root)/third_party/closure_library',
#             # The namespace goog.provide'd by the file under compilation.
#             '--closure_entry_point', 'pagespeed.Caches',
#             # Remove unused dependencies.
#             '--only_closure_dependencies',
#           ],

{
  'type': '<(library)',
  'rules': [
    {
      'rule_name': 'closure',
      'extension': 'js',
      'message': 'Compiling JS code from <(RULE_INPUT_PATH)',
      'variables': {
        'variables': {
          # Provide default values if these inputs vars are not defined.
          'extra_closure_flags%': [],
          'closure_build_type%': 'opt',
          'js_includes%': '',
        },
        'output_file': '<(compiled_js_dir)/<(js_dir)/<(RULE_INPUT_ROOT)_<(closure_build_type).js',
        # TODO(jud): Simplify extra_closure_flags so that only the entry point
        # needs to be defined. --closure_entry_point and
        # --only_closure_dependencies are always defined when using a closure
        # dep, so we can simplify the targets slightly by adding those in here
        # instead of the targets in instaweb.gyp.
        'closure_flags': [
          '--js', '<(RULE_INPUT_PATH)',
          '--js_output_file', '<(output_file)',
          '--output_wrapper=\'(function(){%output%})();\'',
          '--generate_exports',
          '--manage_closure_dependencies',
          '--externs=<(DEPTH)/net/instaweb/js/externs.js',
          '<@(extra_closure_flags)',
        ],
        'conditions': [
          ['closure_build_type == "dbg"', {
            'closure_flags': [
              '--compilation_level=SIMPLE',
              '--formatting=PRETTY_PRINT',
            ],
          }, {
            'closure_flags': [
              '--compilation_level=ADVANCED',
            ],
          }],
          ['js_includes != ""', {
            'inputs': [ '<(js_includes)' ],
            'closure_flags': [ '--js', '<(js_includes)' ],
          }],
          ['"<!(echo $BUILD_JS)" != "1"', {
            'action': [
              'cp',
              'genfiles/<(js_dir)/<(RULE_INPUT_ROOT)_<(closure_build_type).js',
              '<(compiled_js_dir)/<(js_dir)/<(RULE_INPUT_ROOT)_<(closure_build_type).js',
            ],
          }, {
            'action': [
              'java', '-jar', '<(instaweb_root)/tools/closure/compiler.jar',
              '<@(closure_flags)',
            ],
          }],
        ],
      },
      'outputs': [ '<(output_file)', ],
      'action': [ '<@(action)' ],
      'process_outputs_as_sources': 1,
    },
  ],
  'hard_dependency': 1,
  'all_dependent_settings': {
    'hard_dependency': 1,
  },
}
