start_test ModifyCachingHeaders
URL=$TEST_ROOT/retain_cache_control/index.html
OUT=$($WGET_DUMP $URL)
check_from "$OUT" grep -q "Cache-Control: private, max-age=3000"
check_from "$OUT" grep -q "Last-Modified:"

start_test ModifyCachingHeaders with DownstreamCaching enabled.
URL=$TEST_ROOT/retain_cache_control_with_downstream_caching/index.html
OUT=$($WGET_DUMP -S $URL)
check_not_from "$OUT" grep -q "Last-Modified:"
check_from "$OUT" grep -q "Cache-Control: private, max-age=3000"
