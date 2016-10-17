# Tests that an origin header with a Vary header other than Vary:Accept-Encoding
# loses that header when we are not respecting vary.
start_test Vary:User-Agent on resources is held by our cache.
URL="$TEST_ROOT/vary/no_respect/index.html"
fetch_until -save $URL 'fgrep -c .pagespeed.cf.' 1

# Extract out the rewritten CSS file from the HTML saved by fetch_until
# above (see -save and definition of fetch_until).  Fetch that CSS
# file with headers and make sure the Vary is stripped.
CSS_URL=$(grep stylesheet $FETCH_UNTIL_OUTFILE | cut -d\" -f 4)
CSS_URL="$TEST_ROOT/vary/no_respect/$(basename $CSS_URL)"
echo CSS_URL=$CSS_URL
CSS_OUT=$($WGET_DUMP $CSS_URL)
check_from "$CSS_OUT" fgrep -q "Vary: Accept-Encoding"
check_not_from "$CSS_OUT" fgrep -q "User-Agent"
