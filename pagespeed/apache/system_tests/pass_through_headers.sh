start_test Pass through headers when Cache-Control is set early on HTML.
http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
    http://issue809.example.com/mod_pagespeed_example/index.html \
    -O $TESTTMP/issue809.http
check_from "$(extract_headers $TESTTMP/issue809.http)" \
    grep -q "Issue809: Issue809Value"

start_test Pass through common headers from origin on combined resources.
URL="http://issue809.example.com/mod_pagespeed_example/combine_css.html"
http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
    "grep -c css.pagespeed.cc." 1

# Extract out the rewritten CSS URL from the HTML saved by fetch_until
# above (see -save and definition of fetch_until).  Fetch that CSS
# file and look inside for the sprited image reference (ic.pagespeed.is...).
CSS=$(grep stylesheet "$FETCH_UNTIL_OUTFILE" | cut -d\" -f 6)
if [ ${CSS:0:7} != "http://" ]; then
  CSS="http://issue809.example.com/mod_pagespeed_example/$CSS"
fi
http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $CSS -O $TESTTMP/combined.http
check_from "$(extract_headers $TESTTMP/combined.http)" \
    grep -q "Issue809: Issue809Value"
