start_test Query params and headers are recognized in resource flow.
URL=$REWRITTEN_ROOT/styles/W.rewrite_css_images.css.pagespeed.cf.Hash.css
echo "Image gets rewritten by default."
# TODO(sligocki): Replace this fetch_until with single blocking fetch once
# the blocking rewrite header below works correctly.
WGET_ARGS="--header='X-PSA-Blocking-Rewrite:psatest'"
fetch_until $URL 'fgrep -c BikeCrashIcn.png.pagespeed.ic' 1
echo "Image doesn't get rewritten when we turn it off with headers."
# The space after '-convert_png_to_jpeg,' is to test that we do strip spaces.
OUT=$($WGET_DUMP --header="X-PSA-Blocking-Rewrite:psatest" \
  --header="PageSpeedFilters:-convert_png_to_jpeg, -recompress_png" $URL)
check_not_from "$OUT" fgrep -q "BikeCrashIcn.png.pagespeed.ic"

# TODO(vchudnov): This test is not doing quite what it advertises. It
# seems to be getting the cached rewritten resource from the previous
# test case and not going into image.cc itself. Removing the previous
# test case causes this one to go into image.cc. We should test with a
# different resource.
echo "Image doesn't get rewritten when we turn it off with query params."
OUT=$($WGET_DUMP --header="X-PSA-Blocking-Rewrite:psatest" \
  $URL?PageSpeedFilters=-convert_png_to_jpeg,-recompress_png)
check_not_from "$OUT" fgrep -q "BikeCrashIcn.png.pagespeed.ic"
