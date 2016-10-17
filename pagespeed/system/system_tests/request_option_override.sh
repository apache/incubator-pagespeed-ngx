start_test Request Option Override : Correct values are passed
HOST_NAME="http://request-option-override.example.com"
OPTS="?ModPagespeed=on"
OPTS+="&ModPagespeedFilters=+collapse_whitespace,+remove_comments"
OPTS+="&PageSpeedRequestOptionOverride=abc"
URL="$HOST_NAME/mod_pagespeed_test/forbidden.html$OPTS"
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL)"
echo wget $URL
check_not_from "$OUT" grep -q '<!--'

start_test Request Option Override : Incorrect values are passed
HOST_NAME="http://request-option-override.example.com"
OPTS="?ModPagespeed=on"
OPTS+="&ModPagespeedFilters=+collapse_whitespace,+remove_comments"
OPTS+="&PageSpeedRequestOptionOverride=notabc"
URL="$HOST_NAME/mod_pagespeed_test/forbidden.html$OPTS"
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL)"
echo wget $URL
check_from "$OUT" grep -q '<!--'

start_test Request Option Override : Correct values are passed as headers
HOST_NAME="http://request-option-override.example.com"
OPTS="--header=ModPagespeed:on"
OPTS+=" --header=ModPagespeedFilters:+collapse_whitespace,+remove_comments"
OPTS+=" --header=PageSpeedRequestOptionOverride:abc"
URL="$HOST_NAME/mod_pagespeed_test/forbidden.html"
echo wget $OPTS $URL
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $OPTS $URL)"
check_not_from "$OUT" grep -q '<!--'

start_test Request Option Override : Incorrect values are passed as headers
HOST_NAME="http://request-option-override.example.com"
OPTS="--header=ModPagespeed:on"
OPTS+=" --header=ModPagespeedFilters:+collapse_whitespace,+remove_comments"
OPTS+=" --header=PageSpeedRequestOptionOverride:notabc"
URL="$HOST_NAME/mod_pagespeed_test/forbidden.html"
echo wget $OPTS $URL
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $OPTS $URL)"
check_from "$OUT" grep -q '<!--'
