#!/bin/sh
# Copyright 2016 Google Inc. All Rights Reserved.
# Author: cheesy@google.com (Steve Hill)
#
# Run a single command in a chroot via schroot.

exec schroot -c "$CHROOT_NAME" -- "$@"
