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

#
# Notice: We do not include the dbd file in the source list.
#

{
  'variables': {
    'aprutil_root': '<(DEPTH)/third_party/aprutil',
    'aprutil_src_root': '<(aprutil_root)/src',
    'aprutil_gen_os_root': '<(aprutil_root)/gen/arch/<(OS)',
    'aprutil_gen_arch_root': '<(aprutil_gen_os_root)/<(target_arch)',
    'system_include_path_aprutil%': '/usr/include/apr-1.0',
  },
  'conditions': [
    ['use_system_apache_dev==0', {
      'targets': [
        {
          'target_name': 'include',
          'type': 'none',
          'direct_dependent_settings': {
            'include_dirs': [
              '<(aprutil_src_root)/include',
              '<(aprutil_gen_arch_root)/include',
            ],
            'conditions': [
              ['OS=="mac"', {
                'defines': [
                  'HAVE_CONFIG_H',
                  'DARWIN',
                  'SIGPROCMASK_SETS_THREAD_MASK',
                ]}],
              ['OS=="linux"', {
                'defines': [
                  # We need to define _LARGEFILE64_SOURCE so <sys/types.h>
                  # provides off64_t.
                  '_LARGEFILE64_SOURCE',
                  'HAVE_CONFIG_H',
                  'LINUX=2',
                  '_REENTRANT',
                  '_GNU_SOURCE',
                ],
              }],
            ],
          },
        },
        {
          'target_name': 'aprutil',
          'type': '<(library)',
          'dependencies': [
            'include',
            '<(DEPTH)/third_party/apr/apr.gyp:apr',
          ],
          'export_dependent_settings': [
            'include',
          ],
          'include_dirs': [
            '<(aprutil_src_root)/include/private',
            '<(aprutil_gen_arch_root)/include/private',
          ],
          'sources': [
            'src/buckets/apr_brigade.c',
            'src/buckets/apr_buckets.c',
            'src/buckets/apr_buckets_alloc.c',
            'src/buckets/apr_buckets_eos.c',
            'src/buckets/apr_buckets_file.c',
            'src/buckets/apr_buckets_flush.c',
            'src/buckets/apr_buckets_heap.c',
            'src/buckets/apr_buckets_mmap.c',
            'src/buckets/apr_buckets_pipe.c',
            'src/buckets/apr_buckets_pool.c',
            'src/buckets/apr_buckets_refcount.c',
            'src/buckets/apr_buckets_simple.c',
            'src/buckets/apr_buckets_socket.c',
            'src/dbm/apr_dbm.c',
            'src/dbm/apr_dbm_sdbm.c',
            'src/dbm/sdbm/sdbm.c',
            'src/dbm/sdbm/sdbm_hash.c',
            'src/dbm/sdbm/sdbm_lock.c',
            'src/dbm/sdbm/sdbm_pair.c',
            'src/encoding/apr_base64.c',
            'src/hooks/apr_hooks.c',
            'src/ldap/apr_ldap_stub.c',
            'src/ldap/apr_ldap_url.c',
            'src/memcache/apr_memcache.c',
            'src/misc/apr_date.c',
            'src/misc/apr_queue.c',
            'src/misc/apr_reslist.c',
            'src/misc/apr_rmm.c',
            'src/misc/apr_thread_pool.c',
            'src/misc/apu_dso.c',
            'src/misc/apu_version.c',
            'src/strmatch/apr_strmatch.c',
            'src/uri/apr_uri.c',
            'src/xlate/xlate.c',
          ],
          'conditions': [
           ['OS!="win"', {
              'conditions': [
                ['OS=="linux"', {
                  'cflags': [
                    '-pthread',
                    '-Wall',
                  ],
                }],
              ],
            }],
          ],
        }
      ],
    }, { # use_system_apache_dev
      'targets': [
        {
          'target_name': 'include',
          'type': 'none',
          'direct_dependent_settings': {
            'include_dirs': [
              '<(system_include_path_aprutil)',
            ],
            'defines': [
              # We need to define _LARGEFILE64_SOURCE so <sys/types.h>
              # provides off64_t.
              '_LARGEFILE64_SOURCE',
              'HAVE_CONFIG_H',
              'LINUX=2',
              '_REENTRANT',
              '_GNU_SOURCE',
            ],
          },
        },
        {
          'target_name': 'aprutil',
          'type': 'none',
          'dependencies': [
            'include',
          ],
          'export_dependent_settings': [
            'include',
          ],
          'link_settings': {
            'libraries': [
              '-laprutil-1',
            ],
          },
        },
      ],
    }],
  ],
}

