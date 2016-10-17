start_test Custom headers remain on HTML, but cache should be disabled.
URL=$TEST_ROOT/rewrite_compressed_js.html
echo $WGET_DUMP $URL
HTML_HEADERS=$($WGET_DUMP $URL)
check_from "$HTML_HEADERS" egrep -q "X-Extra-Header: 1"
# The extra header should only be added once, not twice.
check_not_from "$HTML_HEADERS" egrep -q "X-Extra-Header: 1, 1"
check_from "$HTML_HEADERS" egrep -q 'Cache-Control: max-age=0, no-cache'

start_test Custom headers remain on resources, but cache should be 1 year.
URL="$TEST_ROOT/compressed/hello_js.custom_ext.pagespeed.ce.HdziXmtLIV.txt"
echo $WGET_DUMP $URL
RESOURCE_HEADERS=$($WGET_DUMP $URL)
check_from "$RESOURCE_HEADERS"  egrep -q 'X-Extra-Header: 1'
# The extra header should only be added once, not twice.
check_not_from "$RESOURCE_HEADERS"  egrep -q 'X-Extra-Header: 1, 1'
check [ "$(echo "$RESOURCE_HEADERS" | grep -c '^X-Extra-Header: 1')" = 1 ]
check_from "$RESOURCE_HEADERS"  egrep -q 'Cache-Control: max-age=31536000'
