#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

"""Compiler version checking tool for clang.

(Based on corresponding tool for gcc in Chromium build system).

Prints X*100 + Y if $CXX is pointing to clang X.Y.*. Prints 0 otherwise.
Note that this output convention is different from compiler_version.py's. This
also never returns a failing status, since we want to run this even on systems
without clang, and gyp will complain on a non-successful status.
"""

import os
import re
import subprocess
import sys


def GetVersion(compiler):
  try:
    compiler = compiler + " --version"
    pipe = subprocess.Popen(compiler, shell=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    output, error = pipe.communicate()
    if pipe.returncode:
      raise subprocess.CalledProcessError(pipe.returncode, compiler)

    result = re.search(r"clang version (\d+)\.?(\d+)?", output)
    if result is None:
      return "0"
    minor_version = result.group(2)
    if minor_version is None:
      minor_version = "0"
    return str(int(result.group(1)) * 100 + int(minor_version))
  except Exception, e:
    if error:
      sys.stderr.write(error)
    print >> sys.stderr, "clang_version.py failed to execute:", compiler
    print >> sys.stderr, e
    return "0"


def main():
  # Check if CXX environment variable exists, and if it does use that compiler.
  cxx = os.getenv("CXX", None)
  if cxx:
    print GetVersion(cxx)
  else:
    print "0"

if __name__ == "__main__":
  main()
  sys.exit(0)
