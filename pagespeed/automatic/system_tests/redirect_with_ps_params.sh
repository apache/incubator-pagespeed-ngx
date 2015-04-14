# Test that redirecting to the same domain retains MPS query parameters.
# The test domain is configured for collapse_whitepsace,add_instrumentation
# so if the QPs are retained we should get the former but not the latter.

start_test Redirecting to the same domain retains PageSpeed query parameters.
URL="$(generate_url redirect.example.com /mod_pagespeed_test/forbidden.html)"
WGET_STDERR="${WGET_OUTPUT}.stderr"
MY_WGET_DUMP="$WGET -O $WGET_OUTPUT --server-response"
OPTS="?PageSpeedFilters=-add_instrumentation"
# First, fetch with add_instrumentation enabled (default) to ensure it is on.
echo wget $URL
http_proxy=$SECONDARY_HOSTNAME $MY_WGET_DUMP $URL 2>$WGET_STDERR
check_not_from "$(< $WGET_STDERR)" egrep -q ' 301 Moved Permanentl| 302 Found'
check_not_from "$(< $WGET_STDERR)" grep -q '^ *Location:'
check_from     "$(< $WGET_OUTPUT)" grep -q 'pagespeed.addInstrumentationInit'
check_not_from "$(< $WGET_OUTPUT)" grep -q '  '
# Then, fetch with add_instrumentation disabled and the URL not redirected.
echo wget $URL$OPTS
http_proxy=$SECONDARY_HOSTNAME $MY_WGET_DUMP $URL$OPTS 2>$WGET_STDERR
check_not_from "$(< $WGET_STDERR)" egrep -q ' 301 Moved Permanentl| 302 Found'
check_not_from "$(< $WGET_STDERR)" grep -q '^ *Location:'
check_not_from "$(< $WGET_OUTPUT)" grep -q 'pagespeed.addInstrumentationInit'
check_not_from "$(< $WGET_OUTPUT)" grep -q '  '
# Finally, fetch with add_instrumentation disabled and the URL redirected.
URL="$(generate_url redirect.example.com \
                    /redirect/mod_pagespeed_test/forbidden.html)"
echo wget $URL$OPTS
http_proxy=$SECONDARY_HOSTNAME $MY_WGET_DUMP $URL$OPTS 2>$WGET_STDERR
check_from     "$(< $WGET_STDERR)" egrep -q ' 301 Moved Permanentl| 302 Found'
check_from     "$(< $WGET_STDERR)" grep -q '^ *Location:.*=-add_instrumentati'
check_not_from "$(< $WGET_OUTPUT)" grep -q 'pagespeed.addInstrumentationInit'
check_not_from "$(< $WGET_OUTPUT)" grep -q '  '
