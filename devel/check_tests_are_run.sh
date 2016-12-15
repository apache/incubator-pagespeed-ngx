#!/bin/bash
#
# Scans source directories for _test.cc files to find ones that aren't mentioned
# in the test gyp file.  On success produces no output, otherwise prints the
# names of the unreferenced _test.cc files.

set -u
set -e

this_dir="$(dirname "${BASH_SOURCE[0]}")"
cd "$this_dir/.."

test_gyp="net/instaweb/test.gyp"
find net pagespeed -name *_test.cc | while read test_path; do
  if ! grep -q "$(basename "$test_path")" "$test_gyp"; then
    echo "$test_path ($(basename "$test_path")) missing from $test_gyp"
  fi
done
