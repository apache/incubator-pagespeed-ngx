start_test inline_css,rewrite_css,sprite_images sprites images in CSS.
FILE=sprite_images.html?PageSpeedFilters=inline_css,rewrite_css,sprite_images
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
echo $WGET_DUMP $URL
fetch_until $URL \
  'grep -c Cuppa.png.*BikeCrashIcn.png.*IronChef2.gif.*.pagespeed.is.*.png' 1

start_test rewrite_css,sprite_images sprites images in CSS.
FILE=sprite_images.html?PageSpeedFilters=rewrite_css,sprite_images
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
fetch_until -save -recursive $URL 'grep -c css.pagespeed.cf' 1

# Extract out the rewritten CSS file from the HTML saved by fetch_until
# above (see -save and definition of fetch_until).  Fetch that CSS
# file and look inside for the sprited image reference (ic.pagespeed.is...).
CSS=$(grep stylesheet "$WGET_DIR/$(basename $URL)" | cut -d\" -f 6)
echo css is $CSS
SPRITE_CSS_OUT="$WGET_DIR/$(basename $CSS)"
echo css file = $SPRITE_CSS_OUT
check [ $(grep -c "ic.pagespeed.is" "$SPRITE_CSS_OUT") -gt 0 ]
