# Copyright 2016 Google Inc.
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

# Dependency for closure.gypi that invokes closure/download.sh to download and
# install the compiler.

{
  'targets': [
    {
      'target_name': 'download_closure',
      'type': 'none',
      'actions': [
        {
          'action_name': 'download_closure',
          'target_type': 'none',
          'inputs': [ '<(DEPTH)/third_party/closure/download.sh' ],
          'outputs': [ '<(DEPTH)/tools/closure/compiler.jar' ],
          'action': [
            '<(DEPTH)/third_party/closure/download.sh',
            '<(DEPTH)/tools/closure',  # target directory
          ],
        },
      ],
    },
  ],
}
