test_filter add_instrumentation adds 2 script tags
check run_wget_with_args $URL
# Counts occurances of '<script' in $FETCHED
# See: http://superuser.com/questions/339522
check [ $(fgrep -o '<script' $FETCHED | wc -l) -eq 2 ]

start_test "We don't add_instrumentation if URL params tell us not to"
FILE=add_instrumentation.html?PageSpeedFilters=
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
check run_wget_with_args $URL
check [ $(fgrep -o '<script' $FETCHED | wc -l) -eq 0 ]

# http://code.google.com/p/modpagespeed/issues/detail?id=170
start_test "Make sure 404s aren't rewritten"
# Note: We run this in the add_instrumentation section because that is the
# easiest to detect which changes every page
THIS_BAD_URL=$BAD_RESOURCE_URL?PageSpeedFilters=add_instrumentation
# We use curl, because wget does not save 404 contents
OUT=$($CURL --silent $THIS_BAD_URL)
check_not_from "$OUT" fgrep "/mod_pagespeed_beacon"
