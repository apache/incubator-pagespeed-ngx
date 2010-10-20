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
    # Make sure we link statically so everything gets linked into a
    # single shared object.
    'library': 'static_library',

    # We're building a shared library, so everything needs to be built
    # with Position-Independent Code.
    'linux_fpic': 1,

    # Define the overridable use_system_libs variable in its own
    # nested block, so it's available for use in the conditions block
    # below.
    'variables': {
      'use_system_libs%': 0,
    },

    'conditions': [
      ['use_system_libs==1', {
        'use_system_apache_dev': 1,
        'use_system_libjpeg': 1,
        'use_system_libpng': 1,
        'use_system_opencv': 1,
        'use_system_zlib': 1,
      },{
        'use_system_apache_dev%': 0,
      }],
    ],
  },
  'includes': [
    '../third_party/libpagespeed/src/build/common.gypi',
  ],
  'target_defaults': {
    'conditions': [
      ['OS == "linux"', {
        'cflags': [
          # Our dependency on OpenCV need us to turn on exceptions.
          '-fexceptions',
          # Now we are using exceptions. -fno-asynchronous-unwind-tables is
          # set in libpagespeed's common.gypi. Now enable it.
          '-fasynchronous-unwind-tables',
        ],
        'cflags_cc': [
          '-frtti',  # Hardy's g++ 4.2 <trl/function> uses typeid
        ],
      }],
      ['OS == "mac"', {
        'xcode_settings':{
          'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',        # -fexceptions
          'GCC_ENABLE_CPP_RTTI': 'YES',              # -frtti
        },
      }],
    ],
  },
}
