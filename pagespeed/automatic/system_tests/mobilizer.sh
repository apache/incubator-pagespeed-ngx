# Set mobile UA
WGETRC_OLD=$WGETRC
export WGETRC=$TESTTMP/wgetrc-mobile-chrome
cat > $WGETRC <<EOF
user_agent =Mozilla/5.0 (Linux; Android 5.1.1; Nexus 4 Build/LMY47V) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/43.0.2357.93 Mobile Safari/537.36
EOF

start_test mobilize off when XHR
URL=$EXAMPLE_ROOT/rewrite_css.html?PageSpeedFilters=mobilize
# Pretend to be XHR.
OUT=$($WGET_DUMP --header=X-Requested-With:XMLHttpRequest $URL)
# Shouldn't get instrumented.
check_not_from "$OUT" fgrep -q 'pagespeed.Mob.start'

start_test mobilize does work on normal request
# Page got instrumented.
OUT=$($WGET_DUMP $URL)
check_from     "$OUT" fgrep -q 'pagespeed.Mob.start'

export WGETRC=$WGETRC_OLD
