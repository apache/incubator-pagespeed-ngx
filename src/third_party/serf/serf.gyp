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
        # This is a mixture of original serf sources (in serf_src)
        # and our patched versions in serf_root
        '<(serf_src)/buckets/aggregate_buckets.c',
        '<(serf_src)/buckets/request_buckets.c',
        '<(serf_src)/buckets/request_buckets.c',
        '<(serf_src)/context.c',
        '<(serf_src)/buckets/buckets.c',
        '<(serf_src)/buckets/simple_buckets.c',
        '<(serf_src)/buckets/file_buckets.c',
        '<(serf_src)/buckets/mmap_buckets.c',
        '<(serf_src)/buckets/socket_buckets.c',
        '<(serf_root)/instaweb_response_buckets.c',
        '<(serf_root)/instaweb_headers_buckets.c',
        '<(serf_root)/instaweb_allocator.c',
        '<(serf_src)/buckets/dechunk_buckets.c',
        '<(serf_src)/buckets/deflate_buckets.c',
        '<(serf_src)/buckets/limit_buckets.c',
        '<(serf_src)/buckets/ssl_buckets.c',
        '<(serf_src)/buckets/barrier_buckets.c',
        '<(serf_src)/buckets/chunk_buckets.c',
        '<(serf_src)/buckets/bwtp_buckets.c',
        '<(serf_src)/incoming.c',
        '<(serf_root)/instaweb_outgoing.c',
        # If we ever want to support authentication, the following changes will
        # need to be made:
        # 1) Uncomment these lines below.
        # 2) Update aprutil.gyp to provide some more files (md5, uuid, etc. ---
        #    the link errors will be your guide).
        # 3) Re-enable call to serf__handle_auth_response in handle_response in
        #    instaweb_outgoing.c.
        # 4) Setup a callback via serf_config_credentials_callback to actually
        #    provide login info.
        #
        #'<(serf_src)/auth/auth.c',
        #'<(serf_src)/auth/auth_basic.c',
        #'<(serf_src)/auth/auth_digest.c',
        #'<(serf_src)/auth/auth_kerb.c',
        #'<(serf_src)/auth/auth_kerb_gss.c'
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

