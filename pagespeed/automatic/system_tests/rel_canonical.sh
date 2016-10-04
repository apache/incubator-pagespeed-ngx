start_test rel-canonical

# .pagespeed. resources should have Link rel=canonical headers, IPRO resources
# should not have them.

start_test link rel=canonical header not present with IPRO resources

REL_CANONICAL_REGEXP='Link:.*rel.*canonical'

URL=$EXAMPLE_ROOT/images/Puzzle.jpg
# Fetch it a few times until IPRO is done and has given it an ipro ("aj") etag.
fetch_until -save "$URL" 'grep -c E[Tt]ag:.W/.PSA-aj.' 1 --save-headers
# rel=canonical should not be present.
check [ $(grep -c "$REL_CANONICAL_REGEXP" $FETCH_FILE) = 0 ]

start_test link rel=canonical header present with pagespeed.ce resources

URL=$REWRITTEN_ROOT/images/Puzzle.jpg.pagespeed.ce.HASH.jpg
OUT=$($CURL -D- -o/dev/null -sS $URL)
check_from "$OUT" grep "$REL_CANONICAL_REGEXP"

start_test link rel=canonical header present with pagespeed.ic resources

URL=$REWRITTEN_ROOT/images/xPuzzle.jpg.pagespeed.ic.HASH.jpg
OUT=$($CURL -D- -o/dev/null -sS  $URL)
check_from "$OUT" grep "$REL_CANONICAL_REGEXP"
