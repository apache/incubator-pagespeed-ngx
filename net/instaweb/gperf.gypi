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

# Author: morlovich@google.com (Maksim Orlovich)

{
  'type': '<(library)',
  'rules': [
    {
      'rule_name': 'gperf',
      'extension': 'gperf',
      'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
      'variables': {
        'gperf_out_dir': '<(SHARED_INTERMEDIATE_DIR)/gperf_out/instaweb',
      },
      'outputs': [
        '<(gperf_out_dir)/<(instaweb_gperf_subdir)/<(RULE_INPUT_ROOT).gp.cc',
      ],
      'action': [
        'gperf',
        '-m 10',
        '<(instaweb_root)/<(instaweb_gperf_subdir)/<(RULE_INPUT_NAME)',
        '--output-file=<(gperf_out_dir)/<(instaweb_gperf_subdir)/<(RULE_INPUT_ROOT).gp.cc',
      ],
      'process_outputs_as_sources': 1,
    },
  ],
  'include_dirs': [
    '<(DEPTH)',
  ],
  'hard_dependency': 1,
  'all_dependent_settings': {
    'hard_dependency': 1,
  },
}
