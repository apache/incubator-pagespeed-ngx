#!/bin/bash
#
# Copyright 2011 Google Inc.
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
