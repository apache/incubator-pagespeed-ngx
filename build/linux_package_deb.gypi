# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'type': 'none',
  'dependencies': [
    'all.gyp:All',
    'linux_installer_configs',
  ],
  'actions': [
    {
      'action_name': 'linux_package_deb_<(channel)_action',
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
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
