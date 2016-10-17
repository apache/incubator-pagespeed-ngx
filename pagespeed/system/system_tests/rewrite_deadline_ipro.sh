start_test 2-pass ipro with long ModPagespeedInPlaceRewriteDeadline
cmd="$WGET_DUMP $TEST_ROOT/ipro/wait/long/purple.css"
echo $cmd; OUT=$($cmd)
echo first pass: the fetch  must occur first regardless of the deadline.
check_from "$OUT" fgrep -q 'background: MediumPurple;'
echo second pass: a long deadline and an easy optimization
echo make an optimized result very likely on the second pass.
echo $cmd; OUT=$($cmd)
check_from "$OUT" fgrep -q 'body{background:#9370db}'

start_test 3-pass ipro with short ModPagespeedInPlaceRewriteDeadline
cmd="$WGET_DUMP $TEST_ROOT/ipro/wait/short/Puzzle.jpg "
echo $cmd; bytes=$($cmd | wc -c)
echo first pass: the fetch  must occur first regardless of the deadline.
check [ $bytes -gt 100000 ]
echo second pass: a short deadline and an image optimization
echo make an unoptimized result very likely on the second pass.
echo $cmd; bytes=$($cmd | wc -c)
check [ $bytes -gt 100000 ]
echo Finally make sure the image gets optimized eventually.
# We don't know how long it will take; if you do the fetch with
# no delay it will probably fail because bash is faster than image
# optimization, so use fetch_until.
fetch_until $TEST_ROOT/ipro/wait/short/Puzzle.jpg 'wc -c' 100000 "" -lt
