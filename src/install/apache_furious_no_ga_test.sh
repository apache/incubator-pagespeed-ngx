#!/bin/bash
#
# Copyright 2012 Google Inc. All Rights Reserved.
# Author: jefftk@google.com (Jeff Kaufman)
#
# Runs all Apache-specific experiment framework (furious) tests that depend on
# ModPagespeedAnalyticsID being unsetset.
#
# See apache_furious_test for usage.
#
this_dir=$(dirname $0)
source "$this_dir/apache_furious_test.sh" || exit 1

EXAMPLE="$1/mod_pagespeed_example"
EXTEND_CACHE="$EXAMPLE/extend_cache.html"

start_test Analytics javascript is not added for any group.
OUT=$($WGET_DUMP --header='Cookie: _GFURIOUS=2' $EXTEND_CACHE)
check_not_from "$OUT" fgrep 'Experiment:'
OUT=$($WGET_DUMP --header='Cookie: _GFURIOUS=7' $EXTEND_CACHE)
check_not_from "$OUT" fgrep 'Experiment:'
OUT=$($WGET_DUMP --header='Cookie: _GFURIOUS=0' $EXTEND_CACHE)
check_not_from "$OUT" fgrep 'Experiment:'

system_test_trailer
