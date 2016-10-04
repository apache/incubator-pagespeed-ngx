start_test quality of jpeg output images with generic quality flag
URL="$TEST_ROOT/image_rewriting/rewrite_images.html"
WGET_ARGS="--header PageSpeedFilters:rewrite_images "
WGET_ARGS+="--header ${IMAGES_QUALITY}:75 "
fetch_until -save -recursive $URL 'grep -c .pagespeed.ic' 2   # 2 images optimized
# This filter produces different images on 32 vs 64 bit builds. On 32 bit, the
# size is 8157B, while on 64 it is 8155B. Initial investigation showed no
# visible differences between the generated images.
# TODO(jmaessen) Verify that this behavior is expected.
#
# Note that if this test fails with 8251 it means that you have managed to get
# progressive jpeg conversion turned on in this testcase, which makes the output
# larger.  The threshold factor kJpegPixelToByteRatio in image_rewrite_filter.cc
# is tuned to avoid that.
check_file_size "$WGET_DIR/*256x192*Puzzle*" -le 8157   # resized
