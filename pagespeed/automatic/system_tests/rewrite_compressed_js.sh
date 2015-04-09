echo Testing whether we can rewrite javascript resources that are served
echo gzipped, even though we generally ask for them clear.  This particular
echo js file has "alert('Hello')" but is checked into source control in gzipped
echo format and served with the gzip headers, so it is decodable.  This tests
echo that we can inline and minify that file.
test_filter rewrite_javascript,inline_javascript with gzipped js origin
URL="$TEST_ROOT/rewrite_compressed_js.html"
QPARAMS="PageSpeedFilters=rewrite_javascript,inline_javascript"
fetch_until "$URL?$QPARAMS" "grep -c Hello'" 1
