start_test Strip subresources default behaviour
URL="$TEST_ROOT/strip_subresource_hints/default/index.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_not_from "$OUT" grep -q -F "rel=\"subresource"

start_test Strip multiple subresources default behaviour
URL="$TEST_ROOT/strip_subresource_hints/default/multiple_subresource_hints.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_not_from "$OUT" grep -q -F "rel=\"subresource"

start_test Strip subresources default behaviour disallow
URL="$TEST_ROOT/strip_subresource_hints/default/disallowtest.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_from "$OUT" grep -q -F "rel=\"subresource"

start_test Strip subresources preserve on
URL="$TEST_ROOT/strip_subresource_hints/preserve_on/index.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_from "$OUT" grep -q -F "rel=\"subresource"

start_test Strip subresources preserve off
URL="$TEST_ROOT/strip_subresource_hints/preserve_off/index.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_not_from "$OUT" grep -q -F "rel=\"subresource"

start_test Strip subresources rewrite level passthrough
URL="$TEST_ROOT/strip_subresource_hints/default_passthrough/index.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_from "$OUT" grep -q -F "rel=\"subresource"
