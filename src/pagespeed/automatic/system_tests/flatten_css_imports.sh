# Fetch with the default limit so our test file is inlined.
test_filter flatten_css_imports,rewrite_css default limit
# Force the request to be rewritten with all applicable filters.
WGET_ARGS="${WGET_ARGS} --header=X-PSA-Blocking-Rewrite:psatest"
echo run_wget_with_args $URL
check run_wget_with_args $URL
check_not grep @import.url $FETCHED
check grep -q "yellow.background-color:" $FETCHED

# Fetch with a tiny limit so no file can be inlined.
test_filter flatten_css_imports,rewrite_css tiny limit
WGET_ARGS="${WGET_ARGS} --header=PageSpeedCssFlattenMaxBytes:5"
# Force the request to be rewritten with all applicable filters.
WGET_ARGS="${WGET_ARGS} --header=X-PSA-Blocking-Rewrite:psatest"
echo run_wget_with_args $URL
check run_wget_with_args $URL
check grep -q @import.url $FETCHED
check_not grep "yellow.background-color:" $FETCHED

# Fetch with a medium limit so any one file can be inlined but not all of them.
test_filter flatten_css_imports,rewrite_css medium limit
WGET_ARGS="${WGET_ARGS} --header=PageSpeedCssFlattenMaxBytes:50"
# Force the request to be rewritten with all applicable filters.
WGET_ARGS="${WGET_ARGS} --header=X-PSA-Blocking-Rewrite:psatest"
echo run_wget_with_args $URL
check run_wget_with_args $URL
check grep -q @import.url $FETCHED
check_not grep "yellow.background-color:" $FETCHED
