start_test quality of jpeg output images
URL="$TEST_ROOT/jpeg_rewriting/rewrite_images.html"
WGET_ARGS="--header PageSpeedFilters:rewrite_images "
WGET_ARGS+="--header ${IMAGES_QUALITY}:85 "
WGET_ARGS+="--header ${JPEG_QUALITY}:70"
fetch_until -save -recursive $URL 'grep -c .pagespeed.ic' 2   # 2 images optimized
#
# If this this test fails because the image size is 7673 bytes it means
# that image_rewrite_filter.cc decided it was a good idea to convert to
# progressive jpeg, and in this case it's not.  See the not above on
# kJpegPixelToByteRatio.
check_file_size "$WGET_DIR/*256x192*Puzzle*" -le 7564   # resized
