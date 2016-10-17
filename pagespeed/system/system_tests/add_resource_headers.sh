start_test AddResourceHeaders works for pagespeed resources.
URL="$TEST_ROOT/compressed/hello_js.custom_ext.pagespeed.ce.HdziXmtLIV.txt"
fetch_until -save "$URL" 'fgrep -c text/javascript' 1 --save-headers
HTML_HEADERS=$(extract_headers $FETCH_FILE)
check_from "$HTML_HEADERS" grep -q "^X-Foo: Bar"
