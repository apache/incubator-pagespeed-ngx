start_test Embed image configuration in rewritten image URL.
# The embedded configuration is placed between the "pagespeed" and "ic", e.g.
# *xPuzzle.jpg.pagespeed.gp+jp+pj+js+rj+rp+rw+ri+cp+md+iq=73.ic.oFXPiLYMka.jpg
# We use a regex matching "gp+jp+pj+js+rj+rp+rw+ri+cp+md+iq=73" rather than
# spelling it out to avoid test regolds when we add image filter IDs.
http_proxy=$SECONDARY_HOSTNAME fetch_until -save -recursive \
    http://embed-config-html.example.org/embed_config.html \
    'fgrep -c .pagespeed.' 3 --save-headers

# With the default rewriters in vhost embed-config-resources.example.com
# the image will be >200k.  But by enabling resizing & compression 73
# as specified in the HTML domain, and transmitting that configuration via
# image URL query param, the image file (including headers) is 8341 bytes.
# We check against 10000 here so this test isn't sensitive to
# image-compression tweaks (we have enough of those elsewhere).
check_file_size "$WGET_DIR/256x192xPuz*.pagespeed.*iq=*.ic.*" -lt 10000

# The CSS file gets rewritten with embedded options, and will have an
# embedded image in it as well.
check_file_size \
  "$WGET_DIR/*rewrite_css_images.css.pagespeed.*+ii+*+iq=*.cf.*" -lt 600

# The JS file is rewritten but has no related options set, so it will
# not get the embedded options between "pagespeed" and "jm".
check_file_size "$WGET_DIR/rewrite_javascript.js.pagespeed.jm.*.js" -lt 500

# Count how many bytes there are of body, skipping the initial headers.
function body_size {
  fname="$1"
  tail -n+$(($(extract_headers $fname | wc -l) + 1)) $fname | wc -c
}

# One flaw in the above test is that it short-circuits the decoding
# of the query-params because when pagespeed responds to the recursive
# wget fetch of the image, it finds the rewritten resource in the
# cache.  The two vhosts are set up with the same cache.  If they
# had different caches we'd have a different problem, which is that
# the first load of the image-rewrite from the resource vhost would
# not be resized.  To make sure the decoding path works, we'll
# "finish" this test below after performing a cache flush, saving
# the encoded image and expected size.
EMBED_CONFIGURATION_IMAGE="http://embed-config-resources.example.com/images/"
EMBED_CONFIGURATION_IMAGE_TAIL=$(ls $WGET_DIR | grep 256x192xPuz | grep iq=)
EMBED_CONFIGURATION_IMAGE+="$EMBED_CONFIGURATION_IMAGE_TAIL"
EMBED_CONFIGURATION_IMAGE_LENGTH=$(
  body_size "$WGET_DIR/$EMBED_CONFIGURATION_IMAGE_TAIL")

# Grab the URL for the CSS file.
EMBED_CONFIGURATION_CSS_LEAF=$(ls $WGET_DIR | \
    grep '\.pagespeed\..*+ii+.*+iq=.*\.cf\..*')
EMBED_CONFIGURATION_CSS_LENGTH=$(
  body_size $WGET_DIR/$EMBED_CONFIGURATION_CSS_LEAF)

EMBED_CONFIGURATION_CSS_URL="http://embed-config-resources.example.com/styles"
EMBED_CONFIGURATION_CSS_URL+="/$EMBED_CONFIGURATION_CSS_LEAF"

# Grab the URL for that embedded image; it should *also* have the embedded
# configuration options in it, though wget/recursive will not have pulled
# it to a file for us (wget does not parse CSS) so we'll have to request it.
EMBED_CONFIGURATION_CSS_IMAGE=$WGET_DIR/*images.css.pagespeed.*+ii+*+iq=*.cf.*
EMBED_CONFIGURATION_CSS_IMAGE_URL=$(egrep -o \
  'http://.*iq=[0-9]*\.ic\..*\.jpg' \
  $EMBED_CONFIGURATION_CSS_IMAGE)
# fetch that file and make sure it has the right cache-control
http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
   $EMBED_CONFIGURATION_CSS_IMAGE_URL > "$WGET_DIR/img"
CSS_IMAGE_HEADERS=$(head -10 "$WGET_DIR/img")
check_from "$CSS_IMAGE_HEADERS" fgrep -q "Cache-Control: max-age=31536000"
EMBED_CONFIGURATION_CSS_IMAGE_LENGTH=$(body_size "$WGET_DIR/img")

function embed_image_config_post_flush() {
  # Finish off the url-params-.pagespeed.-resource tests with a clear
  # cache.  We split the test like this to avoid having multiple
  # places where we flush cache, which requires sleeps since the
  # cache-flush is poll driven.
  start_test Embed image/css configuration decoding with clear cache.
  echo Looking for $EMBED_CONFIGURATION_IMAGE expecting \
      $EMBED_CONFIGURATION_IMAGE_LENGTH bytes
  http_proxy=$SECONDARY_HOSTNAME fetch_until "$EMBED_CONFIGURATION_IMAGE" \
      "wc -c" $EMBED_CONFIGURATION_IMAGE_LENGTH

  echo Looking for $EMBED_CONFIGURATION_CSS_IMAGE_URL expecting \
      $EMBED_CONFIGURATION_CSS_IMAGE_LENGTH bytes
  http_proxy=$SECONDARY_HOSTNAME fetch_until \
      "$EMBED_CONFIGURATION_CSS_IMAGE_URL" \
      "wc -c" $EMBED_CONFIGURATION_CSS_IMAGE_LENGTH

  echo Looking for $EMBED_CONFIGURATION_CSS_URL expecting \
      $EMBED_CONFIGURATION_CSS_LENGTH bytes
  http_proxy=$SECONDARY_HOSTNAME fetch_until \
      "$EMBED_CONFIGURATION_CSS_URL" \
      "wc -c" $EMBED_CONFIGURATION_CSS_LENGTH
}
on_cache_flush embed_image_config_post_flush
