# Verify rendered image dimensions test.
start_test resize_rendered_image_dimensions with critical images beacon
HOST_NAME="http://renderedimagebeacon.example.com"
URL="$HOST_NAME/mod_pagespeed_test/image_rewriting/image_resize_using_rendered_dimensions.html"
http_proxy=$SECONDARY_HOSTNAME \
    fetch_until -save -recursive $URL 'fgrep -c "data-pagespeed-url-hash"' 2 \
    '--header=X-PSA-Blocking-Rewrite:psatest'
check [ $(grep -c "^pagespeed\.CriticalImages\.Run" \
  $WGET_DIR/image_resize_using_rendered_dimensions.html) = 1 ];
OPTIONS_HASH=$(\
  awk -F\' '/^pagespeed\.CriticalImages\.Run/ {print $(NF-3)}' \
  $WGET_DIR/image_resize_using_rendered_dimensions.html)
NONCE=$(awk -F\' '/^pagespeed\.CriticalImages\.Run/ {print $(NF-1)}' \
        $WGET_DIR/image_resize_using_rendered_dimensions.html)

# Send a beacon response using POST indicating that OptPuzzle.jpg is
# critical and has rendered dimensions.
BEACON_URL="$HOST_NAME/$BEACON_HANDLER"
BEACON_URL+="?url=http%3A%2F%2Frenderedimagebeacon.example.com%2Fmod_pagespeed_test%2F"
BEACON_URL+="image_rewriting%2Fimage_resize_using_rendered_dimensions.html"
BEACON_DATA="oh=$OPTIONS_HASH&n=$NONCE&ci=1344500982&rd=%7B%221344500982%22%3A%7B%22rw%22%3A150%2C%22rh%22%3A100%2C%22ow%22%3A256%2C%22oh%22%3A192%7D%7D"
OUT=$(env http_proxy=$SECONDARY_HOSTNAME \
  $CURL -sSi -d "$BEACON_DATA" "$BEACON_URL")
check_from "$OUT" egrep -q "HTTP/1[.]. 204"
http_proxy=$SECONDARY_HOSTNAME \
  fetch_until -save -recursive $URL \
  'fgrep -c 150x100xOptPuzzle.jpg.pagespeed.ic.' 1
