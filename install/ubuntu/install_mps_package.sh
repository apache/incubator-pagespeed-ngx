#!/bin/bash
# Copyright 2016 Google Inc. All Rights Reserved.
# Author: cheesy@google.com (Steve Hill)
#
# Remove old mod_pagespeed debs and install a new one.

pkg="$1"

echo Purging old releases...
dpkg --purge mod-pagespeed-beta mod-pagespeed-stable

echo "Installing $pkg..."
dpkg --install "$pkg"
