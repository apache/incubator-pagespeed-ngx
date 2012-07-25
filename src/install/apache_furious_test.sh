#!/bin/bash
#
# Copyright 2012 Google Inc. All Rights Reserved.
# Author: jefftk@google.com (Jeff Kaufman)
#
# Runs all Apache-specific experiment framework (furious) tests that don't
# depend on Google Analytics.
#
# Exits with status 0 if all tests pass.
# Exits with status 1 immediately if any test fails.
# Exits with status 2 if command line args are wrong.
#
# Argument 1 should be the host:port of the Apache server to talk to.
#
this_dir=$(dirname $0)
source "$this_dir/system_test_helpers.sh" || exit 1

EXAMPLE="$1/mod_pagespeed_example"
EXTEND_CACHE="$EXAMPLE/extend_cache.html"
EXAMPLE_FILE_DIR="$this_dir/mod_pagespeed_example"
TEST_ROOT_FILE_DIR="$this_dir/mod_pagespeed_test"

echo Testing whether or not Furious is working.
echo TEST: $EXAMPLE_FILE_DIR must have a .htaccess file.
check test -f $EXAMPLE_FILE_DIR/.htaccess

echo TEST: _GFURIOUS cookie is set.
check fgrep "_GFURIOUS=" <($WGET_DUMP $EXTEND_CACHE)

echo TEST: $TEST_ROOT_FILE_DIR must not have a .htaccess file.
check_not test -f $TEST_ROOT_FILE_DIR/.htaccess

echo TEST: ModPagespeedFilters query param should disable experiments.
check_not fgrep '_GFURIOUS=' <(
  $WGET_DUMP '$EXTEND_CACHE?ModPagespeed=on&ModPagespeedFilters=rewrite_css')

echo TEST: If the user is already assigned, no need to assign them again.
check_not fgrep '_GFURIOUS=' <(
  $WGET_DUMP --header='Cookie: _GFURIOUS=2' $EXTEND_CACHE)

echo TEST: The beacon should include the experiment id.
check grep 'mod_pagespeed_beacon.*exptid=2' <(
  $WGET_DUMP --header='Cookie: _GFURIOUS=2' $EXTEND_CACHE)
check grep 'mod_pagespeed_beacon.*exptid=7' <(
  $WGET_DUMP --header='Cookie: _GFURIOUS=7' $EXTEND_CACHE)

echo TEST: The no-experiment group beacon should not include an experiment id.
check_not grep 'mod_pagespeed_beacon.*exptid' <(
  $WGET_DUMP --header='Cookie: _GFURIOUS=0' $EXTEND_CACHE)

# We expect id=7 to be index=a and id=2 to be index=b because that's the
# order they're defined in the config file.
echo TEST: Resource urls are rewritten to include experiment indexes.
WGET_ARGS="--header 'Cookie:_GFURIOUS=7'" fetch_until $EXTEND_CACHE \
  "fgrep -c .pagespeed.a.ic." 1
WGET_ARGS="--header 'Cookie:_GFURIOUS=2'" fetch_until $EXTEND_CACHE \
  "fgrep -c .pagespeed.b.ic." 1
check fgrep ".pagespeed.a.ic." <(
  $WGET_DUMP --header='Cookie: _GFURIOUS=7' $EXTEND_CACHE)
check fgrep ".pagespeed.b.ic." <(
  $WGET_DUMP --header='Cookie: _GFURIOUS=2' $EXTEND_CACHE)

echo TEST: Images are different when the url specifies different experiments.
# While the images are the same, image B should be smaller because in the config
# file we enable convert_jpeg_to_progressive only for id=2 (side B).  Ideally we
# would check that it was actually progressive, by checking whether "identify
# -verbose filename" produced "Interlace: JPEG" or "Interlace: None", but that
# would introduce a dependency on imagemagick.  This is just as accurate, but
# more brittle (because changes to our compression code would change the
# computed file sizes).
IMG_A="$EXAMPLE/images/xPuzzle.jpg.pagespeed.a.ic.fakehash.jpg"
IMG_B="$EXAMPLE/images/xPuzzle.jpg.pagespeed.b.ic.fakehash.jpg"
fetch_until $IMG_A 'wc -c' 231192
fetch_until $IMG_B 'wc -c' 216942

echo "PASS."
