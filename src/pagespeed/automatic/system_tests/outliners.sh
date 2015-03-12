test_filter outline_css outlines large styles, but not small ones.
check run_wget_with_args $URL
check egrep -q '<link.*text/css.*large' $FETCHED  # outlined
check egrep -q '<style.*small' $FETCHED           # not outlined

test_filter outline_javascript outlines large scripts, but not small ones.
check run_wget_with_args $URL
check egrep -q '<script.*large.*src=' $FETCHED       # outlined
check egrep -q '<script.*small.*var hello' $FETCHED  # not outlined

start_test compression is enabled for rewritten JS.
JS_URL=$(egrep -o http://.*.pagespeed.*.js $FETCHED)
echo "JS_URL=\$\(egrep -o http://.*[.]pagespeed.*[.]js $FETCHED\)=\"$JS_URL\""
JS_HEADERS=$($WGET -O /dev/null -q -S --header='Accept-Encoding: gzip' \
  $JS_URL 2>&1)
check_200_http_response "$JS_HEADERS"
check_from "$JS_HEADERS" fgrep -qi 'Content-Encoding: gzip'
#check_from "$JS_HEADERS" fgrep -qi 'Vary: Accept-Encoding'
check_from "$JS_HEADERS" egrep -qi '(Etag: W/"0")|(Etag: W/"0-gzip")'
check_from "$JS_HEADERS" fgrep -qi 'Last-Modified:'

