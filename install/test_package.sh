#!/bin/bash
#
# Copyright 2016 Google Inc.
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
# Author: cheesy@google.com (Steve Hill)
#
# Install a mod_pagespeed package and run tests on it.

source "$(dirname "$BASH_SOURCE")/build_env.sh" || exit 1

verbose=
if [ "${1:-}" = '--verbose' ]; then
  verbose='--verbose'
  shift
fi

if [ $# -ne 1 ]; then
  echo "Usage: $(basename $0) [--verbose] <pagespeed_package>" >&2
  exit 1
fi

if [ $UID -ne 0 ]; then
  echo "This script requires root. Re-execing myself with sudo"
  exec sudo "$0" "$@"
  exit 1  # NOTREACHED
fi

pkg="$1"

mkdir -p log

echo "Installing $pkg..."
run_with_log $verbose log/install.log \
  $(dirname "$0")/install_mps_package.sh "$pkg"

echo Test restart to make sure config file is valid ...
run_with_log $verbose log/install.log make -C install apache_debug_restart

echo Testing release ...
run_with_log $verbose log/system_test.log make -C install apache_vm_system_tests
