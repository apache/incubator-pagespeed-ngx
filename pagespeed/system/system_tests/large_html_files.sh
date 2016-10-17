# Test handling of large HTML files. We first test with a cold cache, and verify
# that we bail out of parsing and insert a script redirecting to
# ?PageSpeed=off. This should also insert an entry into the property cache so
# that the next time we fetch the file it will not be parsed at all.
echo TEST: Handling of large files.
# Add a timestamp to the URL to ensure it's not in the property cache.
FILE="max_html_parse_size/large_file.html?value=$(date +%s)"
URL=$TEST_ROOT/$FILE
# Enable a filter that will modify something on this page, since we testing that
# this page should not be rewritten.
WGET_EC="$WGET_DUMP --header=PageSpeedFilters:rewrite_images"
echo $WGET_EC $URL
LARGE_OUT=$($WGET_EC $URL)
check_from "$LARGE_OUT" grep -q window.location=".*&PageSpeed=off"

# The file should now be in the property cache so make sure that the page is no
# longer parsed. Use fetch_until because we need to wait for a potentially
# non-blocking write to the property cache from the previous test to finish
# before this will succeed.
fetch_until -save $URL 'grep -c window.location=".*&PageSpeed=off"' 0
check_not fgrep -q pagespeed.ic $FETCH_FILE
