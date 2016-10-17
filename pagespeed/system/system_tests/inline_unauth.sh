test_filter inline_javascript inlines a small JS file
start_test no inlining of unauthorized resources
URL="$TEST_ROOT/unauthorized/inline_unauthorized_javascript.html?"
URL+="PageSpeedFilters=inline_javascript,debug"
OUTFILE=$OUTDIR/blocking_rewrite.out.html
$WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest' $URL > $OUTFILE
check egrep -q 'script[[:space:]]src=' $OUTFILE
EXPECTED_COMMENT_LINE="<!--The preceding resource was not rewritten because"
EXPECTED_COMMENT_LINE+=" its domain (www.gstatic.com) is not authorized-->"
check [ $(grep -o "$EXPECTED_COMMENT_LINE" $OUTFILE | wc -l) -eq 1 ]

start_test inline_unauthorized_resources allows inlining
HOST_NAME="http://unauthorizedresources.example.com"
URL="$HOST_NAME/mod_pagespeed_test/unauthorized/"
URL+="inline_unauthorized_javascript.html?PageSpeedFilters=inline_javascript"
http_proxy=$SECONDARY_HOSTNAME \
    fetch_until $URL 'grep -c script[[:space:]]src=' 0

start_test inline_unauthorized_resources does not allow rewriting
URL="$HOST_NAME/mod_pagespeed_test/unauthorized/"
URL+="inline_unauthorized_javascript.html?PageSpeedFilters=rewrite_javascript"
OUTFILE=$OUTDIR/blocking_rewrite.out.html
http_proxy=$SECONDARY_HOSTNAME \
    $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest' $URL > $OUTFILE
check egrep -q 'script[[:space:]]src=' $OUTFILE

# Verify that we can control pagespeed settings via a response
# header passed from an origin to a reverse proxy.
start_test Honor response header direcives from origin
URL="http://rproxy.rmcomments.example.com/"
URL+="mod_pagespeed_example/remove_comments.html"
echo http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL ...
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL)
check_from "$OUT" fgrep -q "remove_comments example"
check_not_from "$OUT" fgrep -q "This comment will be removed"

test_filter inline_css inlines a small CSS file
start_test no inlining of unauthorized resources
URL="$TEST_ROOT/unauthorized/inline_css.html?"
URL+="PageSpeedFilters=inline_css,debug"
OUTFILE=$OUTDIR/blocking_rewrite.out.html
$WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest' $URL > $OUTFILE
check egrep -q 'link[[:space:]]rel=' $OUTFILE
EXPECTED_COMMENT_LINE="<!--The preceding resource was not rewritten because"
EXPECTED_COMMENT_LINE+=" its domain (cse.google.com) is not authorized-->"
check [ $(grep -o "$EXPECTED_COMMENT_LINE" $OUTFILE | wc -l) -eq 1 ]

start_test inline_unauthorized_resources allows inlining
HOST_NAME="http://unauthorizedresources.example.com"
URL="$HOST_NAME/mod_pagespeed_test/unauthorized/"
URL+="inline_css.html?PageSpeedFilters=inline_css"
http_proxy=$SECONDARY_HOSTNAME \
    fetch_until $URL 'grep -c link[[:space:]]rel=' 0

start_test inline_unauthorized_resources does not allow rewriting
URL="$HOST_NAME/mod_pagespeed_test/unauthorized/"
URL+="inline_css.html?PageSpeedFilters=rewrite_css"
OUTFILE=$OUTDIR/blocking_rewrite.out.html
http_proxy=$SECONDARY_HOSTNAME \
    $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest' $URL > $OUTFILE
check egrep -q 'link[[:space:]]rel=' $OUTFILE
