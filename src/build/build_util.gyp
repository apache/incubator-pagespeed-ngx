# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'version_py_path': 'version.py',
    'instaweb_path': '<(DEPTH)/net/instaweb',
    'version_path': '<(instaweb_path)/public/VERSION',
    'version_h_in_path': '<(instaweb_path)/public/version.h.in',
    'public_path' : 'net/instaweb/public',
    'version_h_path': '<(SHARED_INTERMEDIATE_DIR)/<(public_path)/version.h',
    'lastchange_out_path': '<(SHARED_INTERMEDIATE_DIR)/build/LASTCHANGE',
  },
  'targets': [
    {
      'target_name': 'lastchange',
      'type': 'none',
      'variables': {
        'default_lastchange_path': '../LASTCHANGE.in',
      },
      'actions': [
        {
          'action_name': 'lastchange',
          'inputs': [
            # Note:  <(default_lastchange_path) is optional,
            # so it doesn't show up in inputs.
            'util/lastchange.py',
          ],
          'outputs': [
            '<(lastchange_out_path).always',
            '<(lastchange_out_path)',
          ],
          'action': [
            'python', '<@(_inputs)',
            '-o', '<(lastchange_out_path)',
            '-d', '<(default_lastchange_path)',
          ],
          'message': 'Extracting last change to <(lastchange_out_path)',
          'process_outputs_as_sources': '1',
        },
      ],
    },
    {
      'target_name': 'mod_pagespeed_version_header',
      'type': 'none',
      'dependencies': [
        'lastchange',
      ],
      'actions': [
        {
          'action_name': 'version_header',
          'inputs': [
            '<(version_path)',
            '<(lastchange_out_path)',
            '<(version_h_in_path)',
          ],
          'outputs': [
            '<(version_h_path)',
          ],
          'action': [
            'python',
            '<(version_py_path)',
            '-f', '<(version_path)',
            '-f', '<(lastchange_out_path)',
            '<(version_h_in_path)',
            '<@(_outputs)',
          ],
          'message': 'Generating version header file: <@(_outputs)',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
    },
  ]
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
