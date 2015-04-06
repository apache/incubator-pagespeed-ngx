#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This file emits the list of reasons why a particular build needs to be clobbered
(or a list of 'landmines').
"""

import optparse
import sys

import landmine_utils


builder = landmine_utils.builder
distributor = landmine_utils.distributor
gyp_defines = landmine_utils.gyp_defines
gyp_msvs_version = landmine_utils.gyp_msvs_version
platform = landmine_utils.platform


def print_landmines(target):
  """
  ALL LANDMINES ARE EMITTED FROM HERE.
  target can be one of {'Release', 'Debug', 'Debug_x64', 'Release_x64'}.
  """
  if (distributor() == 'goma' and platform() == 'win32' and
      builder() == 'ninja'):
    print 'Need to clobber winja goma due to backend cwd cache fix.'
  if platform() == 'android':
    print 'Clobber: Autogen java file needs to be removed (issue 159173002)'
  if platform() == 'win' and builder() == 'ninja':
    print 'Compile on cc_unittests fails due to symbols removed in r185063.'
  if platform() == 'linux' and builder() == 'ninja':
    print 'Builders switching from make to ninja will clobber on this.'
  if platform() == 'mac':
    print 'Switching from bundle to unbundled dylib (issue 14743002).'
  if platform() in ('win', 'mac'):
    print ('Improper dependency for create_nmf.py broke in r240802, '
           'fixed in r240860.')
  if (platform() == 'win' and builder() == 'ninja' and
      gyp_msvs_version() == '2012' and
      gyp_defines().get('target_arch') == 'x64' and
      gyp_defines().get('dcheck_always_on') == '1'):
    print "Switched win x64 trybots from VS2010 to VS2012."
  if (platform() == 'win' and builder() == 'ninja' and
      gyp_msvs_version().startswith('2013')):
    print "Switched win from VS2010 to VS2013."
  print 'Need to clobber everything due to an IDL change in r154579 (blink)'
  if (platform() != 'ios'):
    print 'Clobber to get rid of obselete test plugin after r248358'


def main():
  parser = optparse.OptionParser()
  parser.add_option('-t', '--target',
                    help=='Target for which the landmines have to be emitted')

  options, args = parser.parse_args()

  if args:
    parser.error('Unknown arguments %s' % args)

  print_landmines(options.target)
  return 0


if __name__ == '__main__':
  sys.exit(main())
