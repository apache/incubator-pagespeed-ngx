echo Test that we can rewrite resources that are served with
echo Cache-Control: no-cache with on-the-fly filters.  Tests that the
echo no-cache header is preserved.
test_filter extend_cache with no-cache js origin
URL="$REWRITTEN_TEST_ROOT/no_cache/hello.js.pagespeed.ce.0.js"
echo run_wget_with_args $URL
run_wget_with_args $URL
check fgrep -q "'Hello'" $WGET_DIR/hello.js.pagespeed.ce.0.js
check fgrep -q "no-cache" $WGET_OUTPUT

echo Test that we can rewrite Cache-Control: no-cache resources with
echo non-on-the-fly filters.
test_filter rewrite_javascript with no-cache js origin
URL="$REWRITTEN_TEST_ROOT/no_cache/hello.js.pagespeed.jm.0.js"
echo run_wget_with_args $URL
run_wget_with_args $URL
check fgrep -q "'Hello'" $WGET_DIR/hello.js.pagespeed.jm.0.js
check fgrep -q "no-cache" $WGET_OUTPUT
