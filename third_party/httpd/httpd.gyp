# Copyright 2010 Google Inc.
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
    'apache_root': '<(DEPTH)/third_party/httpd',
    'apache_src_root': '<(apache_root)/src',
    'apache_gen_os_root': '<(apache_root)/gen/arch/<(OS)',
    'apache_gen_arch_root': '<(apache_gen_os_root)/<(target_arch)',
    'system_include_path_httpd%': '/usr/include/apache2',
    'conditions': [
      ['OS!="win"', {
        'apache_os_include': '<(apache_src_root)/os/unix',
      }, {  # else, OS=="win"
        'apache_os_include': '<(apache_src_root)/os/win32',
      }]
    ],
  },
  'conditions': [
    ['use_system_apache_dev==0', {
      'targets': [
        {
          'target_name': 'include',
          'type': 'none',
          'direct_dependent_settings': {
            'include_dirs': [
              '<(apache_src_root)/include',
              '<(apache_os_include)',
              '<(apache_gen_arch_root)/include',
            ],
          },
          'dependencies': [
            '<(DEPTH)/third_party/apr/apr.gyp:include',
            '<(DEPTH)/third_party/aprutil/aprutil.gyp:include',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/third_party/apr/apr.gyp:include',
            '<(DEPTH)/third_party/aprutil/aprutil.gyp:include',
          ],
        },
      ],
    }, {
     'targets': [
       {
         'target_name': 'include',
         'type': 'none',
         'direct_dependent_settings': {
           'include_dirs': [
             '<(system_include_path_httpd)',
           ],
         },
         'dependencies': [
           '<(DEPTH)/third_party/apr/apr.gyp:include',
           '<(DEPTH)/third_party/aprutil/aprutil.gyp:include',
         ],
         'export_dependent_settings': [
           '<(DEPTH)/third_party/apr/apr.gyp:include',
           '<(DEPTH)/third_party/aprutil/aprutil.gyp:include',
         ],
       }
     ],
    }],
  ],
}
