start_test rewrite_css,rewrite_images rewrites and inlines images in CSS.
FILE='rewrite_css_images.html?PageSpeedFilters=rewrite_css,rewrite_images'
FILE+='&ModPagespeedCssImageInlineMaxBytes=2048'
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
fetch_until $URL 'grep -c url.data:image/png;base64,' 1  # image inlined
fetch_until $URL 'grep -c rewrite_css_images.css.pagespeed.cf.' 1
check run_wget_with_args $URL
