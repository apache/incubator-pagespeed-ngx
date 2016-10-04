# Make sure we don't blank url(data:...) in CSS.
start_test CSS data URLs
URL=$REWRITTEN_ROOT/styles/A.data.css.pagespeed.cf.Hash.css
OUT=$($WGET_DUMP $URL)
check_from "$OUT" fgrep -q 'data:image/png'
