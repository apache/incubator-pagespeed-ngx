# Copyright 2011 Google Inc.
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

# Author: atulvasu@google.com (Atul Vasu)

{
  'type': '<(library)',
  'rules': [
    {
      'rule_name': 'data2c',
      'extension': 'js',
      'inputs': [
        '<(data2c_exe)',
      ],
      'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
      'outputs': [
        '<(data2c_out_dir)/<(instaweb_data2c_subdir)/<(RULE_INPUT_ROOT)_out.cc',
      ],
      'action': [
        '<(data2c_exe)',
        '--data_file=<(instaweb_root)/<(instaweb_js_subdir)/<(RULE_INPUT_NAME)',
        '--c_file=<(data2c_out_dir)/<(instaweb_data2c_subdir)/<(RULE_INPUT_ROOT)_out.cc',
        '--varname=JS_<(var_name)',
      ],
      'process_outputs_as_sources': 1,
    },
    {
      'rule_name': 'cssdata2c',
      'extension': 'css',
      'inputs': [
        '<(data2c_exe)',
      ],
      'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
      'outputs': [
        '<(data2c_out_dir)/<(instaweb_data2c_subdir)/<(RULE_INPUT_ROOT)_css_out.cc',
      ],
      'action': [
        '<(data2c_exe)',
        '--data_file=<(instaweb_root)/<(instaweb_js_subdir)/<(RULE_INPUT_NAME)',
        '--c_file=<(data2c_out_dir)/<(instaweb_data2c_subdir)/<(RULE_INPUT_ROOT)_css_out.cc',
        '--varname=CSS_<(var_name)',
      ],
      'process_outputs_as_sources': 1,
    },
    {
      'rule_name': 'htmldata2c',
      'extension': 'html',
      'inputs': [
        '<(data2c_exe)',
      ],
      'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
      'outputs': [
        '<(data2c_out_dir)/<(instaweb_data2c_subdir)/<(RULE_INPUT_ROOT)_html_out.cc',
      ],
      'action': [
        '<(data2c_exe)',
        '--data_file=<(instaweb_root)/<(instaweb_js_subdir)/<(RULE_INPUT_NAME)',
        '--c_file=<(data2c_out_dir)/<(instaweb_data2c_subdir)/<(RULE_INPUT_ROOT)_html_out.cc',
        '--varname=HTML_<(var_name)',
      ],
      'process_outputs_as_sources': 1,
    },
  ],
  'dependencies': [
    'instaweb_data2c',
  ],
  'hard_dependency': 1,
  'all_dependent_settings': {
    'hard_dependency': 1,
  },
}
