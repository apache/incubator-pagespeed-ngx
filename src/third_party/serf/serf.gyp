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
    'serf_root': '<(DEPTH)/third_party/serf',
    'serf_src': '<(serf_root)/src',
  },
  'targets': [
    {
      'target_name': 'serf',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/third_party/apache/apr/apr.gyp:include',
        '<(DEPTH)/third_party/apache/aprutil/aprutil.gyp:include',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
      ],
      'sources': [
        '<(serf_root)/instaweb_context.c',
        '<(serf_src)/context.c',
        '<(serf_src)/buckets/aggregate_buckets.c',
        '<(serf_src)/buckets/allocator.c',
        '<(serf_src)/buckets/barrier_buckets.c',
        '<(serf_src)/buckets/buckets.c',
        '<(serf_src)/buckets/chunk_buckets.c',
        '<(serf_src)/buckets/dechunk_buckets.c',
        '<(serf_src)/buckets/deflate_buckets.c',
        '<(serf_src)/buckets/file_buckets.c',
        '<(serf_src)/buckets/headers_buckets.c',
        '<(serf_src)/buckets/limit_buckets.c',
        '<(serf_src)/buckets/mmap_buckets.c',
        '<(serf_src)/buckets/request_buckets.c',
# There are two bugs in serf 0.3.1.
#    1. There is a buffer-overrun risk in fetch_headers
#    2. Serf always unzips zipped content, which is not what we want, and
#       then it leaves the 'gzip' headers in.  This is in contrast to the
#       behavior of curl and wget, which provide gzipped output if that's
#       what was requested.
#
# We will try to pursue fixes to these issues with the Serf community but
# in the meantime we will override our own version of response_buckets.c,
# which will be placed in the root directory.
        '<(serf_root)/instaweb_response_buckets.c',
        '<(serf_src)/buckets/simple_buckets.c',
        '<(serf_src)/buckets/socket_buckets.c',
      ],
     'include_dirs': [
        '<(serf_src)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(serf_src)',
        ],
      },
    }
  ],
}

