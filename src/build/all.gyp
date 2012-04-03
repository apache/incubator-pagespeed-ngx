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
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'xcode_create_dependents_test_runner': 1,
      'dependencies': [
        'mod_pagespeed',
        'test',
        'pagespeed_automatic',
      ],},
    {
      'target_name': 'mod_pagespeed',
      'type': 'none',
      'dependencies': [
        '../net/instaweb/instaweb.gyp:instaweb_rewriter',
        '../net/instaweb/instaweb_apr.gyp:*',
        '../net/instaweb/mod_pagespeed.gyp:*',
        'install.gyp:*',
      ],},
    {
      'target_name': 'pagespeed_automatic',
      'type': 'none',
      'dependencies': [
        '../net/instaweb/test.gyp:pagespeed_automatic_test',
        '../net/instaweb/instaweb.gyp:automatic_util',
      ],},
    {
      'target_name': 'test',
      'type': 'none',
      'xcode_create_dependents_test_runner': 1,
      'dependencies': [
        '../net/instaweb/instaweb.gyp:*',
        '../net/instaweb/instaweb_core.gyp:*',
        '../net/instaweb/instaweb_apr.gyp:*',
        '../net/instaweb/test.gyp:mod_pagespeed_test',
        '../net/instaweb/test.gyp:mod_pagespeed_speed_test',
        'install.gyp:*',
      ]
    },
  ],
}
