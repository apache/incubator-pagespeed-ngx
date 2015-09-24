# Set mobile UA
WGETRC_OLD=$WGETRC
export WGETRC=$TESTTMP/wgetrc-mobile-chrome
cat > $WGETRC <<EOF
user_agent =Mozilla/5.0 (Linux; Android 5.1.1; Nexus 4 Build/LMY47V) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/43.0.2357.93 Mobile Safari/537.36
EOF

start_test mobilize off when XHR
URL=$EXAMPLE_ROOT/index.html?PageSpeedFilters=mobilize
# Pretend to be XHR.
OUT=$($WGET_DUMP --header=X-Requested-With:XMLHttpRequest $URL)
# Shouldn't get instrumented.
check_not_from "$OUT" fgrep -q 'psStartMobilization'
check_not_from "$OUT" fgrep -q '<header id="psmob-header-bar"'
check_not_from "$OUT" fgrep -q '<nav id="psmob-nav-panel"'
check_not_from "$OUT" fgrep -q '<iframe id="psmob-iframe"'

start_test mobilize does work on normal request
# Page got instrumented.
OUT=$($WGET_DUMP $URL)
check_from "$OUT" fgrep -q 'psStartMobilization'
check_from "$OUT" fgrep -q '<header id="psmob-header-bar"'
check_from "$OUT" fgrep -q '<nav id="psmob-nav-panel"'
check_not_from "$OUT" fgrep -q '<iframe id="psmob-iframe"'

start_test mobilize get disabled by noscript mode
# Page got instrumented.
URL=$EXAMPLE_ROOT/index.html?PageSpeedFilters=mobilize\&PageSpeed=noscript
OUT=$($WGET_DUMP $URL)
check_not_from "$OUT" fgrep -q 'psStartMobilization'
check_not_from "$OUT" fgrep -q '<header id="psmob-header-bar"'
check_not_from "$OUT" fgrep -q '<nav id="psmob-nav-panel"'
check_not_from "$OUT" fgrep -q '<iframe id="psmob-iframe"'

export WGETRC=$WGETRC_OLD
