start_test PageSpeed Unplugged and Off
SPROXY="http://localhost:$APACHE_SECONDARY_PORT"
VHOST_MPS_OFF="http://mpsoff.example.com"
VHOST_MPS_UNPLUGGED="http://mpsunplugged.example.com"
SITE="mod_pagespeed_example"
ORIGINAL="$SITE/styles/yellow.css"
FILTERED="$SITE/styles/A.yellow.css.pagespeed.cf.KM5K8SbHQL.css"

# PageSpeed unplugged does not serve .pagespeed. resources.
http_proxy=$SPROXY check_not $WGET -O /dev/null \
  $VHOST_MPS_UNPLUGGED/$FILTERED
# PageSpeed off does serve .pagespeed. resources.
http_proxy=$SPROXY check $WGET -O /dev/null $VHOST_MPS_OFF/$FILTERED

# PageSpeed unplugged doesn't rewrite HTML, even when asked via query.
OUT=$(http_proxy=$SPROXY check $WGET -S -O - \
  $VHOST_MPS_UNPLUGGED/$SITE/?PageSpeed=on 2>&1)
check_not_from "$OUT" grep "X-Mod-Pagespeed:"
# PageSpeed off does rewrite HTML if asked.
OUT=$(http_proxy=$SPROXY check $WGET -S -O - \
  $VHOST_MPS_OFF/$SITE/?PageSpeed=on 2>&1)
check_from "$OUT" grep "X-Mod-Pagespeed:"
