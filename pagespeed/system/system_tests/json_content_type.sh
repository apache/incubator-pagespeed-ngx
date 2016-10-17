start_test json keeps its content type
URL="$TEST_ROOT/example.json"
OUT=$($WGET_DUMP "$URL?PageSpeed=off")
# Verify that it's application/json without PageSpeed touching it.
check_from "$OUT" grep '^Content-Type: application/json'
OUT=$($WGET_DUMP "$URL")
# Verify that it's application/json on the first PageSpeed load.
check_from "$OUT" grep '^Content-Type: application/json'
# Fetch it repeatedly until it's been IPRO-optimized.  This grep command is kind
# of awkward, because fetch_until doesn't do quoting well.
WGET_ARGS="--save-headers" fetch_until -save "$URL" \
  "grep -c .title.:.example.json" 1
OUT=$(cat $FETCH_UNTIL_OUTFILE)
# Make sure we didn't change the content type to application/javascript.
check_from "$OUT" grep '^Content-Type: application/json'
