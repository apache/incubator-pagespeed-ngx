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
    'serf_root': '<(DEPTH)/third_party/serf/src',
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
        '<(serf_root)/context.c',
        '<(serf_root)/buckets/aggregate_buckets.c',
        '<(serf_root)/buckets/allocator.c',
        '<(serf_root)/buckets/barrier_buckets.c',
        '<(serf_root)/buckets/buckets.c',
        '<(serf_root)/buckets/chunk_buckets.c',
        '<(serf_root)/buckets/dechunk_buckets.c',
        '<(serf_root)/buckets/deflate_buckets.c',
        '<(serf_root)/buckets/file_buckets.c',
        '<(serf_root)/buckets/headers_buckets.c',
        '<(serf_root)/buckets/limit_buckets.c',
        '<(serf_root)/buckets/mmap_buckets.c',
        '<(serf_root)/buckets/request_buckets.c',
        '<(serf_root)/buckets/response_buckets.c',
        '<(serf_root)/buckets/simple_buckets.c',
        '<(serf_root)/buckets/socket_buckets.c',
      ],
     'include_dirs': [
        '<(serf_root)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(serf_root)',
        ],
      },
    }
  ],
}

