#!/bin/bash
# Copyright 2016 Google Inc. All Rights Reserved.
# Author: cheesy@google.com (Steve Hill)
#
# Remove old mod_pagespeed debs and install a new one.

pkg="$1"

echo Purging old releases...
# rpm --erase only succeeds if all packages listed are installed, so we need
# to find which one is installed and only erase that.
rpm --query mod-pagespeed-stable mod-pagespeed-beta | \
    grep -v "is not installed" | \
    xargs --no-run-if-empty sudo rpm --erase

echo "Installing $pkg..."
rpm --install "$pkg"
