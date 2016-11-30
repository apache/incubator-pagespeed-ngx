#!/bin/bash
# Copyright 2016 Google Inc. All Rights Reserved.
# Author: cheesy@google.com (Steve Hill)
#
# Trivial wrapper for shell function that writes output to a log.

source "$(dirname "$BASH_SOURCE")/build_env.sh" || exit 1

run_with_log "$@"
