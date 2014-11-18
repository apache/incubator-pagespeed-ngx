start_test fallback_rewrite_css_urls works.
FILE=fallback_rewrite_css_urls.html?\
PageSpeedFilters=fallback_rewrite_css_urls,rewrite_css,extend_cache
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
fetch_until $URL 'grep -c Cuppa.png.pagespeed.ce.' 1  # image cache extended
fetch_until -save $URL 'grep -c fallback_rewrite_css_urls.css.pagespeed.cf.' 1
# Test this was fallback flow -> no minification.
check grep -q "body { background" $FETCH_FILE
