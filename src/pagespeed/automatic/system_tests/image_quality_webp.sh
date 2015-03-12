start_test quality of webp output images
rm -rf $OUTDIR
mkdir $OUTDIR
IMG_REWRITE="$TEST_ROOT/webp_rewriting/rewrite_images.html"
REWRITE_URL="$IMG_REWRITE?PageSpeedFilters=rewrite_images"
URL="$REWRITE_URL,convert_jpeg_to_webp&$IMAGES_QUALITY=75&$WEBP_QUALITY=65"
fetch_until -save -recursive $URL \
  'fgrep -c 256x192xPuzzle.jpg.pagespeed.ic' 1 \
   --user-agent=webp
check_file_size "$WGET_DIR/*256x192*Puzzle*webp" -le 5140   # resized, webp'd

rm -rf $WGET_DIR
fetch_until -save -recursive $URL \
  'fgrep -c 256x192xPuzzle.jpg.pagespeed.ic' 1 \
  --header='Accept:image/webp'
check_file_size "$WGET_DIR/*256x192*Puzzle*webp" -le 5140   # resized, webp'd
