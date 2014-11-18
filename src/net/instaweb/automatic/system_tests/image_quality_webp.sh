start_test quality of webp output images
rm -rf $OUTDIR
mkdir $OUTDIR
IMG_REWRITE="$TEST_ROOT/webp_rewriting/rewrite_images.html"
REWRITE_URL="$IMG_REWRITE?PageSpeedFilters=rewrite_images"
URL="$REWRITE_URL,convert_jpeg_to_webp&$IMAGES_QUALITY=75&$WEBP_QUALITY=65"
check run_wget_with_args \
  --header 'X-PSA-Blocking-Rewrite: psatest' --user-agent=webp $URL
check_file_size "$WGET_DIR/*256x192*Puzzle*webp" -le 5140   # resized, webp'd
rm -rf $WGET_DIR
check run_wget_with_args \
  --header 'X-PSA-Blocking-Rewrite: psatest' --header='Accept: image/webp' $URL
check_file_size "$WGET_DIR/*256x192*Puzzle*webp" -le 5140   # resized, webp'd
