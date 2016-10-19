start_test Encoded absolute urls are not respected
HOST_NAME="http://absolute-urls.example.com"

# Monitor the Apache log; tail -F will catch log rotations.
ABSOLUTE_URLS_LOG_PATH=$TESTTMP/instaweb_apache_absolute_urls.log
echo APACHE_LOG = $APACHE_LOG
tail --sleep-interval=0.1 -F $APACHE_LOG > $ABSOLUTE_URLS_LOG_PATH &
TAIL_PID=$!

# should fail; the example.com isn't us.
http_proxy=$SECONDARY_HOSTNAME check_not $WGET_DUMP \
    "$HOST_NAME/,hexample.com.pagespeed.jm.0.js"

REJECTED="Rejected absolute url reference"

# Wait up to 10 seconds for failure.
for i in {1..100}; do
  REJECTIONS=$(fgrep -c "$REJECTED" $ABSOLUTE_URLS_LOG_PATH || true)
  if [ $REJECTIONS -ge 1 ]; then
    break;
  fi;
  /bin/echo -n "."
  sleep 0.1
done;
/bin/echo "."

# Kill the log monitor silently.
kill $TAIL_PID
wait $TAIL_PID 2> /dev/null || true

check [ $REJECTIONS -eq 1 ]
