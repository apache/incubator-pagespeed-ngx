#!/bin/bash
#
# Copyright 2010 Google Inc.
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
# Author: morlovich@google.com (Maks Orlovich)
#
# Runs apache process-scope tests.  Note that the config required for this test
# generates deprecation warnings on apache restart, hence it needs to be
# factored out.

# We need to know the directory this file is located in.  Unfortunately,
# if we're 'source'd from a script in a different directory $(dirname $0) gives
# us the directory that *that* script is located in
this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/../automatic/system_test_helpers.sh" || exit 1

if [ "$SECONDARY_HOSTNAME" != "" ]; then
  start_test Process-scope configuration handling.
  # Must be the same value in top-level and both vhosts
  OUT=$($CURL --silent $EXAMPLE_ROOT/?PageSpeedFilters=+debug)
  check_from "$OUT" fgrep -q "IproMaxResponseBytes (imrb) 1048576003"

  OUT=$($CURL --silent --proxy $SECONDARY_HOSTNAME http://ps1.example.com)
  check_from "$OUT" fgrep -q "IproMaxResponseBytes (imrb) 1048576003"

  OUT=$($CURL --silent --proxy $SECONDARY_HOSTNAME http://ps2.example.com)
  check_from "$OUT" fgrep -q "IproMaxResponseBytes (imrb) 1048576003"
fi

check_failures_and_exit
