#!/bin/bash
#
# Scans source directories for _test.cc files and makes sure they are mentioned
# in the appropriate gyp files.
#
# This script serves two purposes; it is the entry-point to the scan
# (0 args), and is also the script used in 'find -exec' (>0 args).

set -u

if [ $# -eq 0 ]; then
  # Get absolute path for script so we can still reference it after 'cd'.
  this_dir="$(dirname "${BASH_SOURCE[0]}")"
  script="$this_dir/check_tests_are_run.sh"
  src="$this_dir/.."

  set -e
  test_gyp="$src/net/instaweb/test.gyp"
  tps="pagespeed"
  for entry in $tps/*; do
    if [[ -d $entry ]]; then
      find $entry -name *_test.cc -exec $script {} $test_gyp \;
    fi
  done
else
  test_cc_path="$1"; shift
  test_cc_name=$(basename $test_cc_path)

  test_cc_dir=$(dirname $test_cc_path)
  found=0
  for gypfile in "$@"; do
    if grep -q $test_cc_name $gypfile; then
      found=1
    fi
  done

  if [ $found -eq 0 ]; then
    echo $test_cc_path missing from "$@"
  fi
fi
