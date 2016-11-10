#!/bin/bash
# Copyright 2016 Google Inc. All Rights Reserved.
# Author: cheesy@google.com (Steve Hill)
#
# Trivial wrapper for shell function that installs a package from source
# tarball.

source "$(dirname "$BASH_SOURCE")/build_env.sh" || exit 1

install_from_src "$@"
