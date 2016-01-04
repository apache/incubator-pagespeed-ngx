start_test Optimize images to webp
function test_optimize_to_webp() {
  HTML="$TEST_ROOT/optimize_for_bandwidth/$1"
  URL="$HTML?PageSpeedFilters=$2"
  OUT=$($WGET -q -O - --header=X-PSA-Blocking-Rewrite:psatest \
    --user-agent=$3 --header=Accept:image/webp $URL)
  check_from "$OUT" grep -q "$4"
  check_from "$OUT" grep -q "$5"
}
# Most recent Chrome. Both lossless and animated WebP will be used.
test_optimize_to_webp webp_urls/rewrite_webp.html \
  "convert_to_webp_lossless,convert_to_webp_animated,recompress_png" \
  "Chrome/32." \
  "/xCuppa.png.pagespeed.ic.*.webp\"/>" \
  "/xPageSpeedAnimationSmall.gif.pagespeed.ic.*.webp\"/>"
# Less recent Chrome. Only lossless WebP will be used.
test_optimize_to_webp webp_urls/rewrite_webp.html \
  "convert_to_webp_lossless,convert_to_webp_animated,recompress_png" \
  "Chrome/31." \
  "/xCuppa.png.pagespeed.ic.*.webp\"/>" \
  "/PageSpeedAnimationSmall.gif\"/>"
# Old chrome. No WebP will be used. Single frame image will be converted to
# PNG.
test_optimize_to_webp webp_urls/rewrite_webp.html \
  "convert_to_webp_lossless,convert_to_webp_animated,recompress_png" \
  "Chrome/22." \
  "/xCuppa.png.pagespeed.ic.*.png\"/>" \
  "/PageSpeedAnimationSmall.gif\"/>"
