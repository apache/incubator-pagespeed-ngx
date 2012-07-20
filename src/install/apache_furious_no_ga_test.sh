#!/bin/bash
#
# Copyright 2012 Google Inc. All Rights Reserved.
# Author: jefftk@google.com (Jeff Kaufman)
#
# Runs all Apache-specific experiment framework (furious) tests that depend on
# ModPagespeedAnalyticsID being unsetset.
#
# Exits with status 0 if all tests pass.
# Exits with status 1 immediately if any test fails.
# Exits with status 2 if command line args are wrong.
#
# Argument 1 should be the host:port of the Apache server to talk to.
#
this_dir=$(dirname $0)
source "$this_dir/apache_furious_test.sh" || exit 1

EXAMPLE="$1/mod_pagespeed_example"
EXTEND_CACHE="$EXAMPLE/extend_cache.html"

echo TEST: Analytics javascript is not added for any group.
check_not fgrep 'Experiment:' <(
  $WGET_DUMP --header='Cookie: _GFURIOUS=2' $EXTEND_CACHE)
check_not fgrep 'Experiment:' <(
  $WGET_DUMP --header='Cookie: _GFURIOUS=7' $EXTEND_CACHE)
check_not fgrep 'Experiment:' <(
  $WGET_DUMP --header='Cookie: _GFURIOUS=0' $EXTEND_CACHE)

echo PASS