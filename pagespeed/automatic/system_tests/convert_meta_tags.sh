# This filter convert the meta tags in the html into headers.
test_filter convert_meta_tags
run_wget_with_args $URL

echo Checking for Charset header.
check grep -qi "CONTENT-TYPE: text/html; *charset=UTF-8" $WGET_OUTPUT
