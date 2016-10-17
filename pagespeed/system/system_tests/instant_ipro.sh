# Tests that we get instant ipro rewrites with LoadFromFile and
# InPlaceWaitForOptimized get us first-pass rewrites.
start_test instant ipro with InPlaceWaitForOptimized and LoadFromFile
echo $WGET_DUMP $TEST_ROOT/ipro/instant/wait/purple.css
OUT=$($WGET_DUMP $TEST_ROOT/ipro/instant/wait/purple.css)
check_from "$OUT" fgrep -q 'body{background:#9370db}'

start_test instant ipro with ModPagespeedInPlaceRewriteDeadline and LoadFromFile
echo $WGET_DUMP $TEST_ROOT/ipro/instant/deadline/purple.css
OUT=$($WGET_DUMP $TEST_ROOT/ipro/instant/deadline/purple.css)
check_from "$OUT" fgrep -q 'body{background:#9370db}'
