#!/bin/bash
# Copyright 2016 Google Inc. All Rights Reserved.
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
