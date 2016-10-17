start_test Protocol relative urls
URL="$PRIMARY_SERVER//www.modpagespeed.com/"
URL+="styles/A.blue.css.pagespeed.cf.0.css"
OUT=$($CURL -D- -o/dev/null -sS "$URL")
check_from "$OUT" grep "^HTTP.* 404 Not Found"
