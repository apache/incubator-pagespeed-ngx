#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This version contains a bugfix for the compiler returning a single digit
# version number, as is the case for gcc 5.

"""Compiler version checking tool for gcc

Print gcc version as XY if you are running gcc X.Y.*.
This is used to tweak build flags for gcc 4.4.
"""

import os
import re
import subprocess
import sys

def GetVersion(compiler):
  try:
    # Note that compiler could be something tricky like "distcc g++".
    compiler = compiler + " -dumpversion"
    pipe = subprocess.Popen(compiler, shell=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    gcc_output, gcc_error = pipe.communicate()
    if pipe.returncode:
      raise subprocess.CalledProcessError(pipe.returncode, compiler)

    result = re.match(r"(\d+)\.?(\d+)?", gcc_output)
    minor_version = result.group(2)
    if minor_version is None:
      minor_version = "0"
    return result.group(1) + minor_version
  except Exception, e:
    if gcc_error:
      sys.stderr.write(gcc_error)
    print >> sys.stderr, "compiler_version.py failed to execute:", compiler
    print >> sys.stderr, e
    return ""

def main():
  # Check if CXX environment variable exists and
  # if it does use that compiler.
  cxx = os.getenv("CXX", None)
  if cxx:
    cxxversion = GetVersion(cxx)
    if cxxversion != "":
      print cxxversion
      return 0
  else:
    # Otherwise we check the g++ version.
    gccversion = GetVersion("g++")
    if gccversion != "":
      print gccversion
      return 0

  return 1

if __name__ == "__main__":
  sys.exit(main())
