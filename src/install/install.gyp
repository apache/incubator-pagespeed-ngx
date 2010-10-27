# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'version_py_path': '<(DEPTH)/build/version.py',
    'version_path': '<(DEPTH)/net/instaweb/public/VERSION',
    'lastchange_path': '<(SHARED_INTERMEDIATE_DIR)/build/LASTCHANGE',
    'branding_dir': 'common',
  },
  'conditions': [
    ['OS=="linux"', {
      'variables': {
        'version' : '<!(python <(version_py_path) -f <(version_path) -t "@MAJOR@.@MINOR@.@BUILD@.@PATCH@")',
        'revision' : '<!(python <(DEPTH)/build/util/lastchange.py | cut -d "=" -f 2)',
        'packaging_files_common': [
          'common/apt.include',
          'common/mod-pagespeed/mod-pagespeed.info',
          'common/installer.include',
          'common/repo.cron',
          'common/rpm.include',
          'common/rpmrepo.cron',
          'common/updater',
          'common/variables.include',
          'common/BRANDING',
          'common/pagespeed.load.template',
          'common/pagespeed.conf.template',
        ],
        'packaging_files_deb': [
          'debian/build.sh',
          'debian/changelog.template',
          'debian/conffiles',
          'debian/control.template',
          'debian/postinst',
          'debian/postrm',
          'debian/prerm',
        ],
        'packaging_files_rpm': [
          'rpm/build.sh',
          'rpm/mod-pagespeed.spec.template',
        ],
        'packaging_files_binaries': [
          '<(PRODUCT_DIR)/libmod_pagespeed.so',
        ],
        'flock_bash': ['flock', '--', '/tmp/linux_package_lock', 'bash'],
        'deb_build': '<(PRODUCT_DIR)/install/debian/build.sh',
        'rpm_build': '<(PRODUCT_DIR)/install/rpm/build.sh',
        'deb_cmd': ['<@(flock_bash)', '<(deb_build)', '-o' '<(PRODUCT_DIR)',
                    '-b', '<(PRODUCT_DIR)', '-a', '<(target_arch)'],
        'rpm_cmd': ['<@(flock_bash)', '<(rpm_build)', '-o' '<(PRODUCT_DIR)',
                    '-b', '<(PRODUCT_DIR)', '-a', '<(target_arch)'],
        'conditions': [
          ['target_arch=="ia32"', {
            'deb_arch': 'i386',
            'rpm_arch': 'i386',
          }],
          ['target_arch=="x64"', {
            'deb_arch': 'amd64',
            'rpm_arch': 'x86_64',
          }],
        ],
      },
      'targets': [
        {
          'target_name': 'linux_installer_configs',
          'type': 'none',
          # Add these files to the build output so the build archives will be
          # "hermetic" for packaging.
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/install/debian/',
              'files': [
                '<@(packaging_files_deb)',
              ]
            },
            {
              'destination': '<(PRODUCT_DIR)/install/rpm/',
              'files': [
                '<@(packaging_files_rpm)',
              ]
            },
            {
              'destination': '<(PRODUCT_DIR)/install/common/',
              'files': [
                '<@(packaging_files_common)',
              ]
            },
          ],
          'actions': [
            {
              'action_name': 'save_build_info',
              'inputs': [
                '<(branding_dir)/BRANDING',
                '<(version_path)',
                '<(lastchange_path)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/installer/version.txt',
              ],
              # Just output the default version info variables.
              'action': [
                'python', '<(version_py_path)',
                '-f', '<(branding_dir)/BRANDING',
                '-f', '<(version_path)',
                '-f', '<(lastchange_path)',
                '-o', '<@(_outputs)'
              ],
            },
          ],
        },
        {
          'target_name': 'linux_packages',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'linux_package_deb',
            'linux_package_rpm',
          ],
        },
        {
          'target_name': 'linux_package_deb',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            '<(DEPTH)/net/instaweb/mod_pagespeed.gyp:mod_pagespeed',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'beta',
              },
              'action_name': 'deb_package_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(deb_build)',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_deb)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/mod-pagespeed-<(channel)-<(version)-r<(revision)_<(deb_arch).deb',
              ],
              'action': [ '<@(deb_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
        {
          'target_name': 'linux_package_rpm',
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            '<(DEPTH)/net/instaweb/mod_pagespeed.gyp:mod_pagespeed',
            'linux_installer_configs',
          ],
          'actions': [
            {
              'variables': {
                'channel': 'beta',
              },
              'action_name': 'rpm_package_<(channel)',
              'process_outputs_as_sources': 1,
              'inputs': [
                '<(rpm_build)',
                '<(PRODUCT_DIR)/install/rpm/mod-pagespeed.spec.template',
                '<@(packaging_files_binaries)',
                '<@(packaging_files_common)',
                '<@(packaging_files_rpm)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/mod-pagespeed-<(channel)-<(version)-r<(revision).<(rpm_arch).rpm',
              ],
              'action': [ '<@(rpm_cmd)', '-c', '<(channel)', ],
            },
          ],
        },
      ],
    },{
      'targets': [
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
