start_test rewrite_css,extend_cache extends cache of images in CSS.
FILE=rewrite_css_images.html?PageSpeedFilters=rewrite_css,extend_cache
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
fetch_until $URL 'grep -c Cuppa.png.pagespeed.ce.' 1  # image cache extended
fetch_until $URL 'grep -c rewrite_css_images.css.pagespeed.cf.' 1
check run_wget_with_args $URL
