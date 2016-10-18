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
# to avoid a dependency on having java installed. Note that this env var is
# checked when gclient sync is run, not when compiling, so make sure to run
# gclient sync to modify the compilation mode.

# New JS files should be added to instaweb.gyp, following the instructions
# there.

{
  'type': '<(library)',
  'dependencies': [
    '<(DEPTH)/net/instaweb/closure.gyp:download_closure',
  ],
  'rules': [
    {
      'rule_name': 'closure',
      'extension': 'js',
      'message': 'Compiling JS code from <(RULE_INPUT_PATH)',
      'variables': {
        # Note that it might be possible to eliminate this nesting of variables
        # blocks with a better understanding of gyp file processing order. As it
        # stands, without the nested block, the action block below doesn't see
        # the variables declared here, so nested it is. Note though that this
        # requires another declaration of the inputs var below so that it can be
        # used as a value for the inputs key at the top level. Ugh.
        'variables': {
          # Provide default values if these inputs vars are not defined.
          'extra_closure_flags%': [],
          'closure_build_type%': 'opt',
          'js_includes%': '',
        },
        'inputs': '<(js_includes)',
        'output_file': '<(compiled_js_dir)/<(js_dir)/<(RULE_INPUT_ROOT)_<(closure_build_type).js',
        # TODO(jud): Simplify extra_closure_flags so that only the entry point
        # needs to be defined.
        # --dependency_mode=STRICT is always defined when using a closure dep,
        # so we can simplify the targets slightly by adding those in here
        # instead of the targets in instaweb.gyp.
        'closure_flags': [
          '--js', '<(RULE_INPUT_PATH)',
          '--js_output_file', '<(output_file)',
          '--output_wrapper=\'(function(){%output%})();\'',
          '--generate_exports',
          '--externs=<(DEPTH)/net/instaweb/js/externs.js',
          '--warning_level=VERBOSE',
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
            'closure_flags': ['<!@(python -c "print \' \'.join([\'--js \' + js for js in \'<(js_includes)\'.split()]) ")'],
          }],
          ['"<!(echo $BUILD_JS)" != "1"', {
            'action': [
              'cp',
              '<(DEPTH)/net/instaweb/genfiles/<(js_dir)/<(RULE_INPUT_ROOT)_<(closure_build_type).js',
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
      'inputs': [ '<@(inputs)' ],
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
