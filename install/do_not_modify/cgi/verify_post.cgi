#!/bin/sh
# This cgi script just checks whether we get a POST produced by
# a test right.
echo Content-type: text/html
echo
FORM_DATA=$(cat /dev/stdin)
if [ "$FORM_DATA" = "a=b&c=d" ]; then
    echo "PASS"
else
    echo "FAIL"
fi
