# Copyright 2013 Google Inc.
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

# Author: Maksim Orlovich <morlovich@google.com>
# This lets us chose whether to use system OpenSSL or an own copy from
# Chromium.

{
  'variables': {
    'use_system_openssl%': 0,
  },
  'conditions': [
    ['use_system_openssl==0', {
      'targets': [
        {
          'target_name': 'select_openssl',
          'type': 'none',
          'dependencies': [
            '<(DEPTH)/third_party/serf/openssl.gyp:openssl',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/third_party/serf/openssl.gyp:openssl',
          ],
        },
      ]
    },
    { # use_system_openssl
      'targets': [
        {
          'target_name': 'select_openssl',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
              '<!@(pkg-config --cflags openssl)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other openssl)',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l openssl)',
            ],
          },
        },
      ],
    }],
  ],
}

