BAD_IMG_URL=$REWRITTEN_ROOT/images/xBadName.jpg.pagespeed.ic.Zi7KMNYwzD.jpg
start_test rewrite_images fails broken image
echo run_wget_with_args $BAD_IMG_URL
check_not run_wget_with_args $BAD_IMG_URL  # fails
check grep "404 Not Found" $WGET_OUTPUT

start_test "rewrite_images doesn't 500 on unoptomizable image."
IMG_URL=$REWRITTEN_ROOT/images/xOptPuzzle.jpg.pagespeed.ic.Zi7KMNYwzD.jpg
run_wget_with_args -q $IMG_URL
check_200_http_response_file "$WGET_OUTPUT"
