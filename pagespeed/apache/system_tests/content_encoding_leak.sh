# Regression test for a bug we used to have; a leak on nonsense
# Content-Encoding. This will not trigger immediately, but at process exit.

start_test Exercising codepath of bad content-encoding
OUT=$(http_proxy=$SECONDARY_HOSTNAME $CURL -Si http://content-encoding.example.com)
check_from "$OUT" fgrep -q "Content-Encoding: nonsense"
