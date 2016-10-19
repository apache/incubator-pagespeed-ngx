# Determine whether statistics are enabled or not.  If not, don't test them,
# but do an additional regression test that tries harder to get a cache miss.
if [ $statistics_enabled = "1" ]; then
  if [ "$SECONDARY_HOSTNAME" != "" ]; then
    function gunzip_grep_0ff() {
      gunzip - | fgrep -q "color:#00f"
      echo $?
    }

    start_test ipro with mod_deflate
    fetch_until -gzip $TEST_ROOT/ipro/mod_deflate/big.css gunzip_grep_0ff 0

    start_test ipro with reverse proxy of compressed content
    http_proxy=$SECONDARY_HOSTNAME \
        fetch_until -gzip http://ipro-proxy.example.com/big.css \
            gunzip_grep_0ff 0

    # Also test the .pagespeed. version, to make sure we didn't
    # accidentally gunzip stuff above when we shouldn't have.
    OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET -q -O - \
          http://ipro-proxy.example.com/A.big.css.pagespeed.cf.0.css)
    check_from "$OUT" fgrep -q "big{color:#00f}"
  fi
else
  start_test 404s are served.  Statistics are disabled so not checking them.
  OUT=$(check_not $WGET -O /dev/null $BAD_RESOURCE_URL 2>&1)
  check_from "$OUT" fgrep -q "404 Not Found"

  start_test 404s properly on uncached invalid resource.
  OUT=$(check_not $WGET -O /dev/null $BAD_RND_RESOURCE_URL 2>&1)
  check_from "$OUT" fgrep -q "404 Not Found"
fi
