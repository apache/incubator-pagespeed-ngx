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
    'apr_root': '<(DEPTH)/third_party/apr',
    'apr_src_root': '<(apr_root)/src',
    'apr_gen_os_root': '<(apr_root)/gen/arch/<(OS)',
    'apr_gen_arch_root': '<(apr_gen_os_root)/<(target_arch)',
    'system_include_path_apr%': '/usr/include/apr-1.0',
    'conditions': [
      ['OS!="win"', {
        'apr_os_include': '<(apr_src_root)/include/arch/unix',
      }, {  # else, OS=="win"
        'apr_os_include': '<(apr_src_root)/include/arch/win32',
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
              '<(apr_src_root)/include',
              '<(apr_os_include)',
              '<(apr_gen_arch_root)/include',
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
          'target_name': 'apr',
          'type': '<(library)',
          'dependencies': [
            'include',
          ],
          'export_dependent_settings': [
            'include',
          ],
          'sources': [
            '<(apr_src_root)/passwd/apr_getpass.c',
            '<(apr_src_root)/strings/apr_strnatcmp.c',
            '<(apr_src_root)/strings/apr_strtok.c',
            '<(apr_src_root)/strings/apr_strings.c',
            '<(apr_src_root)/strings/apr_snprintf.c',
            '<(apr_src_root)/strings/apr_fnmatch.c',
            '<(apr_src_root)/strings/apr_cpystrn.c',
            '<(apr_src_root)/tables/apr_tables.c',
            '<(apr_src_root)/tables/apr_hash.c',
          ],
          'conditions': [
            ['OS!="win"', { # TODO(lsong): Add win sources.
              'conditions': [
                ['OS=="linux"', {
                  'cflags': [
                    '-pthread',
                    '-Wall',
                  ],
                  'link_settings': {
                    'libraries': [
                      '-ldl',
                  ]},
                }],
              ],
              'sources': [
                'src/atomic/unix/builtins.c',
                'src/atomic/unix/ia32.c',
                'src/atomic/unix/mutex.c',
                'src/atomic/unix/ppc.c',
                'src/atomic/unix/s390.c',
                'src/atomic/unix/solaris.c',
                'src/dso/unix/dso.c',
                'src/file_io/unix/buffer.c',
                'src/file_io/unix/copy.c',
                'src/file_io/unix/dir.c',
                'src/file_io/unix/fileacc.c',
                'src/file_io/unix/filedup.c',
                'src/file_io/unix/filepath.c',
                'src/file_io/unix/filepath_util.c',
                'src/file_io/unix/filestat.c',
                'src/file_io/unix/flock.c',
                'src/file_io/unix/fullrw.c',
                'src/file_io/unix/mktemp.c',
                'src/file_io/unix/open.c',
                'src/file_io/unix/pipe.c',
                'src/file_io/unix/readwrite.c',
                'src/file_io/unix/seek.c',
                'src/file_io/unix/tempdir.c',
                'src/locks/unix/global_mutex.c',
                'src/locks/unix/proc_mutex.c',
                'src/locks/unix/thread_cond.c',
                'src/locks/unix/thread_mutex.c',
                'src/locks/unix/thread_rwlock.c',
                'src/memory/unix/apr_pools.c',
                'src/misc/unix/charset.c',
                'src/misc/unix/env.c',
                'src/misc/unix/errorcodes.c',
                'src/misc/unix/getopt.c',
                'src/misc/unix/otherchild.c',
                'src/misc/unix/rand.c',
                'src/misc/unix/start.c',
                'src/misc/unix/version.c',
                'src/mmap/unix/common.c',
                'src/mmap/unix/mmap.c',
                'src/network_io/unix/inet_ntop.c',
                'src/network_io/unix/inet_pton.c',
                'src/network_io/unix/multicast.c',
                'src/network_io/unix/sendrecv.c',
                'src/network_io/unix/sockaddr.c',
                'src/network_io/unix/socket_util.c',
                'src/network_io/unix/sockets.c',
                'src/network_io/unix/sockopt.c',
                'src/passwd/apr_getpass.c',
                'src/poll/unix/epoll.c',
                'src/poll/unix/kqueue.c',
                'src/poll/unix/poll.c',
                'src/poll/unix/pollcb.c',
                'src/poll/unix/pollset.c',
                'src/poll/unix/port.c',
                'src/poll/unix/select.c',
                'src/random/unix/apr_random.c',
                'src/random/unix/sha2.c',
                'src/random/unix/sha2_glue.c',
                'src/shmem/unix/shm.c',
                'src/strings/apr_cpystrn.c',
                'src/strings/apr_fnmatch.c',
                'src/strings/apr_snprintf.c',
                'src/strings/apr_strings.c',
                'src/strings/apr_strnatcmp.c',
                'src/strings/apr_strtok.c',
                'src/support/unix/waitio.c',
                'src/tables/apr_hash.c',
                'src/tables/apr_tables.c',
                'src/threadproc/unix/proc.c',
                'src/threadproc/unix/procsup.c',
                'src/threadproc/unix/signals.c',
                'src/threadproc/unix/thread.c',
                'src/threadproc/unix/threadpriv.c',
                'src/time/unix/time.c',
                'src/time/unix/timestr.c',
                'src/user/unix/groupinfo.c',
                'src/user/unix/userinfo.c',
              ],
            }],
          ],
        }
      ],
    },
    { # use_system_apache_dev
      'targets': [
        {
          'target_name': 'include',
          'type': 'none',
          'direct_dependent_settings': {
            'include_dirs': [
              '<(system_include_path_apr)',
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
          'target_name': 'apr',
          'type': 'settings',
          'dependencies': [
            'include',
          ],
          'export_dependent_settings': [
            'include',
          ],
          'link_settings': {
            'libraries': [
              '-lapr-1',
            ],
          },
        },
      ],
    }],
  ],
}

