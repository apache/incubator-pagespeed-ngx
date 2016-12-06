#!/bin/bash

set -e
set -u

APACHE_SERVER="$1"
APACHE_SLURP_ORIGIN_PORT="$2"
APACHE_SLURP_PORT="$3"
WGET="$4"
TMP_SLURP_DIR="$5"
PAGESPEED_TEST_HOST="$6"

this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/../pagespeed/automatic/system_test_helpers.sh" \
       "$APACHE_SERVER"

EXTEND_CACHE_URL="http://www.modpagespeed.com/extend_cache.html"

DEVEL_DIR="$(dirname "${BASH_SOURCE[0]}")"

start_test "Testing slurping (read only, via proxy)"
http_proxy="$APACHE_SERVER" "$WGET" -q -O /dev/null \
  "$EXTEND_CACHE_URL?PageSpeedFilters=extend_cache"

# TODO(sligocki): Use something like fetch_until rather than
# always waiting 2 seconds :/
sleep 2

OUT="$(http_proxy="$APACHE_SERVER" "$WGET" -q -O - \
  "$EXTEND_CACHE_URL?PageSpeedFilters=extend_cache")"
check_from "$OUT" fgrep "images/Puzzle.jpg.pagespeed.ce."

OUT="$(http_proxy="$APACHE_SERVER" "$WGET" -q -O - \
  "$EXTEND_CACHE_URL?PageSpeed=off")"
check_from "$OUT" fgrep '"images/Puzzle.jpg"'

start_test "Testing slurping (dns mode, mimicing webpagetest)"
OUT="$("$WGET" --header="Host: www.modpagespeed.com" -q -O - --save-headers \
  "$EXTEND_CACHE_URL?PageSpeedFilters=extend_cache")"
check_from "$OUT" grep -q 'HTTP/1.[01] 200 OK'

start_test "Testing slurping http://www.example.com expecting index.html ..."
echo "rewrite will not happen"
OUT="$(http_proxy="$APACHE_SERVER" "$WGET" -q -O - http://www.example.com/)"
check_from "$OUT" fgrep "example.com expected body"

start_test "Connection-close stripping:"
echo 'First check we get "Connection:close"'

echo "straight from the origin -- no proxy."
rm -rf "$TMP_SLURP_DIR"

slurp_origin_url="http://localhost:$APACHE_SLURP_ORIGIN_PORT"
slurp_origin_url+="/close_connection/close_connection.html"

OUT="$("$WGET" --no-proxy -q --save-headers -O - --header="Connection:" \
  "$slurp_origin_url")"
check_from "$OUT" fgrep "Connection: close"

echo "Now check that Connection:close is stripped from a writing slurp."
OUT=$(http_proxy=localhost:$APACHE_SLURP_PORT "$WGET" -q --save-headers -O - \
  --header="Connection:" "$slurp_origin_url" || true)
check_not_from "$OUT" fgrep -q "Connection: close"

start_test "Testing slurp-proxying of a POST"
rm -rf "$TMP_SLURP_DIR"
OUT="$(http_proxy=localhost:$APACHE_SLURP_PORT "$WGET" -q -O - \
  --post-data="a=b&c=d" \
  http://$PAGESPEED_TEST_HOST/do_not_modify/cgi/verify_post.cgi)"
check_from "$OUT" grep "PASS"
