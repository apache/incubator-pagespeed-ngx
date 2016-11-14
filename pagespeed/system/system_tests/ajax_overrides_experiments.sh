start_test Ajax overrides experiments

# Normally, we expect this <head>less HTML file to have a <head> added
# by the experimental infrastructure, and for the double-space between
# "two  spaces" to be collapsed to one, due to the experiment.
URL="http://experiment.ajax.example.com/mod_pagespeed_test/ajax/ajax.html"
http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
  'fgrep -c .pagespeed.' 3
OUT=$(cat "$FETCH_FILE")
check_from "$OUT" fgrep -q '<head'
check_from "$OUT" fgrep -q 'Two spaces.'

# However, we must not add a '<head' in an Ajax request, rewrite any URLs, or
# execute the collapse_whitespace experiment.
start_test Experiments not injected on ajax.html with an Ajax header
AJAX="--header=X-Requested-With:XmlHttpRequest"
http_proxy=$SECONDARY_HOSTNAME fetch_until -save -expect_time_out "$URL" \
  'fgrep -c \.pagespeed\.' 3  "$AJAX"
OUT=$(cat "$FETCH_FILE")
check_not_from "$OUT" fgrep -q '<head'
check_not_from "$OUT" fgrep -q '.pagespeed.'
check_from "$OUT" fgrep -q 'Two  spaces.'

start_test Ajax disables any filters that add head.

# While we are in here, check also that Ajax requests don't get a 'head',
# even if we are not in an experiment.
URL="http://ajax.example.com/mod_pagespeed_test/ajax/ajax.html"
http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
  'fgrep -c .pagespeed.' 3
OUT=$(cat "$FETCH_FILE")
check_from "$OUT" fgrep -q '<head'

# However, we must not add a '<head' in an Ajax request or rewrite any URLs.
http_proxy=$SECONDARY_HOSTNAME fetch_until -save -expect_time_out "$URL" \
  'fgrep -c \.pagespeed\.' 3  "$AJAX"
OUT=$(cat "$FETCH_FILE")
check_not_from "$OUT" fgrep -q '<head'
check_not_from "$OUT" fgrep -q '.pagespeed.'
