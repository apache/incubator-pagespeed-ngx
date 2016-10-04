# Checks that inline_preview_images injects compiled javascript
test_filter inline_preview_images optimize mode
FILE=delay_images.html?PageSpeedFilters=$FILTER_NAME
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
WGET_ARGS="${WGET_ARGS} --user-agent=iPhone"
echo run_wget_with_args $URL
fetch_until $URL 'grep -c pagespeed.delayImagesInit' 1
fetch_until $URL 'grep -c /\*' 0
check run_wget_with_args $URL

# Checks that inline_preview_images,debug injects from javascript
# in non-compiled mode
test_filter inline_preview_images,debug debug mode
FILE=delay_images.html?PageSpeedFilters=$FILTER_NAME
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
WGET_ARGS="${WGET_ARGS} --user-agent=iPhone"
fetch_until $URL 'grep -c pagespeed.delayImagesInit' 3
check run_wget_with_args $URL
