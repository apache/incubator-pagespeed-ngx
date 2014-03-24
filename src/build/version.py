#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# (See http://src.chromium.org/viewvc/chrome/trunk/src/LICENSE)
# This file itself is from
# http://src.chromium.org/viewvc/chrome/trunk/src/build/util/ as of
# revision r252481

"""
version.py -- Chromium version string substitution utility.
"""

import getopt
import os
import sys


class Usage(Exception):
  def __init__(self, msg):
    self.msg = msg


def fetch_values_from_file(values_dict, file_name):
  """
  Fetches KEYWORD=VALUE settings from the specified file.

  Everything to the left of the first '=' is the keyword,
  everything to the right is the value.  No stripping of
  white space, so beware.

  The file must exist, otherwise you get the Python exception from open().
  """
  for line in open(file_name, 'r').readlines():
    key, val = line.rstrip('\r\n').split('=', 1)
    values_dict[key] = val


def fetch_values(file_list):
  """
  Returns a dictionary of values to be used for substitution, populating
  the dictionary with KEYWORD=VALUE settings from the files in 'file_list'.

  Explicitly adds the following value from internal calculations:

    OFFICIAL_BUILD
  """
  CHROME_BUILD_TYPE = os.environ.get('CHROME_BUILD_TYPE')
  if CHROME_BUILD_TYPE == '_official':
    official_build = '1'
  else:
    official_build = '0'

  values = dict(
    OFFICIAL_BUILD = official_build,
  )

  for file_name in file_list:
    fetch_values_from_file(values, file_name)

  return values


def subst_template(contents, values):
  """
  Returns the template with substituted values from the specified dictionary.

  Keywords to be substituted are surrounded by '@':  @KEYWORD@.

  No attempt is made to avoid recursive substitution.  The order
  of evaluation is random based on the order of the keywords returned
  by the Python dictionary.  So do NOT substitute a value that
  contains any @KEYWORD@ strings expecting them to be recursively
  substituted, okay?
  """
  for key, val in values.iteritems():
    try:
      contents = contents.replace('@' + key + '@', val)
    except TypeError:
      print repr(key), repr(val)
  return contents


def subst_file(file_name, values):
  """
  Returns the contents of the specified file_name with substited
  values from the specified dictionary.

  This is like subst_template, except it operates on a file.
  """
  template = open(file_name, 'r').read()
  return subst_template(template, values);


def write_if_changed(file_name, contents):
  """
  Writes the specified contents to the specified file_name
  iff the contents are different than the current contents.
  """
  try:
    old_contents = open(file_name, 'r').read()
  except EnvironmentError:
    pass
  else:
    if contents == old_contents:
      return
    os.unlink(file_name)
  open(file_name, 'w').write(contents)


def main(argv=None):
  if argv is None:
    argv = sys.argv

  short_options = 'e:f:i:o:t:h'
  long_options = ['eval=', 'file=', 'help']

  helpstr = """\
Usage:  version.py [-h] [-f FILE] ([[-i] FILE] | -t TEMPLATE) [[-o] FILE]

  -f FILE, --file=FILE              Read variables from FILE.
  -i FILE, --input=FILE             Read strings to substitute from FILE.
  -o FILE, --output=FILE            Write substituted strings to FILE.
  -t TEMPLATE, --template=TEMPLATE  Use TEMPLATE as the strings to substitute.
  -e VAR=VAL, --eval=VAR=VAL        Evaluate VAL after reading variables. Can
                                    be used to synthesize variables. e.g.
                                    -e 'PATCH_HI=int(PATCH)/256'.
  -h, --help                        Print this help and exit.
"""

  evals = {}
  variable_files = []
  in_file = None
  out_file = None
  template = None

  try:
    try:
      opts, args = getopt.getopt(argv[1:], short_options, long_options)
    except getopt.error, msg:
      raise Usage(msg)
    for o, a in opts:
      if o in ('-e', '--eval'):
        try:
          evals.update(dict([a.split('=',1)]))
        except ValueError:
          raise Usage("-e requires VAR=VAL")
      elif o in ('-f', '--file'):
        variable_files.append(a)
      elif o in ('-i', '--input'):
        in_file = a
      elif o in ('-o', '--output'):
        out_file = a
      elif o in ('-t', '--template'):
        template = a
      elif o in ('-h', '--help'):
        print helpstr
        return 0
    while len(args) and (in_file is None or out_file is None or
                         template is None):
      if in_file is None:
        in_file = args.pop(0)
      elif out_file is None:
        out_file = args.pop(0)
    if args:
      msg = 'Unexpected arguments: %r' % args
      raise Usage(msg)
  except Usage, err:
    sys.stderr.write(err.msg)
    sys.stderr.write('; Use -h to get help.\n')
    return 2

  values = fetch_values(variable_files)
  for key, val in evals.iteritems():
    values[key] = str(eval(val, globals(), values))

  if template is not None:
    contents = subst_template(template, values)
  elif in_file:
    contents = subst_file(in_file, values)
  else:
    # Generate a default set of version information.
    contents = """MAJOR=%(MAJOR)s
MINOR=%(MINOR)s
BUILD=%(BUILD)s
PATCH=%(PATCH)s
LASTCHANGE=%(LASTCHANGE)s
OFFICIAL_BUILD=%(OFFICIAL_BUILD)s
""" % values


  if out_file:
    write_if_changed(out_file, contents)
  else:
    print contents

  return 0


if __name__ == '__main__':
  sys.exit(main())
