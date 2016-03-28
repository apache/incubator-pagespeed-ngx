# Usage: verify_nosniff leaf content_type1 content_type2 ...
#
# Checks that the response has a "nosniff" header and one of the supplied
# content types.  For exammple:
#
#    verify_nosniff foo.js application/javascript text/javascript \
#                          application/x-javascript
function verify_nosniff {
  leaf="$1"
  shift
  acceptable_content_types="$@"
  URL=$REWRITTEN_ROOT/$leaf
  OUT=$($CURL -D- -o/dev/null -sS "$URL")
  check_from "$OUT" grep '^HTTP.* 200 OK'
  found=false
  for content_type in $acceptable_content_types; do
    echo looking for $content_type
    if echo "$OUT" | grep '^Content-Type: '"$content_type"'' > /dev/null; then
      echo found it
      found=true
    else
      echo not yet
    fi
  done
  if ! $found; then
    echo "Check failed: no acceptable content types found"
    echo "Acceptable types: $acceptable_content_types"
    echo "FAILed Input: $OUT"
    fail
  fi
  check_from "$OUT" grep '^X-Content-Type-Options: nosniff'
}

# Checks that the response is a 404.
function verify_404 {
  leaf="$1"
  URL=$REWRITTEN_ROOT/$leaf
  OUT=$($CURL -D- -o/dev/null -sS "$URL")
  check_from "$OUT" grep '^HTTP.* 404 Not Found'
}

# test that all the filters do fine with one of our content types
start_test js minification css
verify_nosniff styles/big.css.pagespeed.jm.0.foo \
  text/css application/javascript

start_test image spriting css
verify_nosniff styles/big.css.pagespeed.is.0.foo text/css

start_test image compression css
verify_nosniff styles/xbig.css.pagespeed.ic.0.foo text/css

start_test cache extension css
verify_nosniff styles/big.css.pagespeed.ce.0.foo text/css


# test that we also do fine with the other content types we generate
start_test js minification js
verify_nosniff rewrite_javascript.js.pagespeed.jm.0.foo \
  application/javascript application/x-javascript

start_test js minification png
verify_nosniff images/Cuppa.png.pagespeed.jm.0.foo image/png

start_test js minification gif
verify_nosniff images/IronChef2.gif.pagespeed.jm.0.foo image/gif

start_test js minification jpg
verify_nosniff images/Puzzle.jpg.pagespeed.jm.0.foo image/jpeg

start_test js minification webp
verify_nosniff images/gray_saved_as_rgb.webp.pagespeed.jm.0.foo image/webp

start_test js minification pdf
verify_nosniff example.pdf.pagespeed.jm.0.foo application/pdf


# test that we 404 html
start_test js minification html
verify_404 index.html.pagespeed.jm.0.foo

start_test image spriting html
verify_404 index.html.pagespeed.is.0.foo

start_test image compression html
verify_404 xindex.html.pagespeed.ic.0.foo

start_test cache extension html
verify_404 index.html.pagespeed.ce.0.foo


# test that we 404 svgs too
start_test js minification svg
verify_404 images/schedule_event.svg.pagespeed.jm.0.foo

