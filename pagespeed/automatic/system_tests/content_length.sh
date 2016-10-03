start_test PageSpeed resources should have a content length.
HTML_URL="$EXAMPLE_ROOT/rewrite_css_images.html?PageSpeedFilters=rewrite_css"
fetch_until -save "$HTML_URL" "fgrep -c rewrite_css_images.css.pagespeed.cf" 1
# Pull the rewritten resource name out so we get an accurate hash.
REWRITTEN_URL=$(grep rewrite_css_images.css $FETCH_UNTIL_OUTFILE | \
                awk -F'"' '{print $(NF-1)}')
if [[ $REWRITTEN_URL == *//* ]]; then
  URL="$REWRITTEN_URL"
else
  URL="$REWRITTEN_ROOT/$REWRITTEN_URL"
fi
# This will use REWRITE_DOMAIN as an http_proxy if set, otherwise no proxy.
OUT=$(http_proxy=${REWRITE_DOMAIN:-} $WGET_DUMP $URL)
check_from "$OUT" grep "^Content-Length:"
check_not_from "$OUT" grep "^Transfer-Encoding: chunked"
check_not_from "$OUT" grep "^Cache-Control:.*private"
