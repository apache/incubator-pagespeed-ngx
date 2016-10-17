start_test max cacheable content length with ipro
URL="http://max-cacheable-content-length.example.com/mod_pagespeed_example/"
URL+="images/BikeCrashIcn.png"
# This used to check-fail the server; see ngx_pagespeed issue #771.
http_proxy=$SECONDARY_HOSTNAME check $WGET -t 1 -O /dev/null $URL
