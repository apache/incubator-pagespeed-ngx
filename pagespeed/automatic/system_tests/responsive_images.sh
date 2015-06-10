test_filter responsive_images,rewrite_images,-inline_images adds srcset for \
  Puzzle.jpg and Cuppa.png
fetch_until $URL 'grep -c srcset=' 3
# Make sure all Puzzle URLs are rewritten.
fetch_until -save $URL 'grep -c [^x]Puzzle.jpg' 0
check egrep -q 'xPuzzle.jpg.pagespeed.+srcset="([^ ]*images/([0-9]+x[0-9]+)?xPuzzle.jpg.pagespeed.ic.[0-9a-zA-Z_-]+.jpg [0-9.]+x,?)+"' $FETCH_FILE
# Make sure all Cuppa URLs are rewritten.
fetch_until -save $URL 'grep -c [^x]Cuppa.png' 0
check egrep -q 'xCuppa.png.pagespeed.+srcset="([^ ]*images/([0-9]+x[0-9]+)?xCuppa.png.pagespeed.ic.[0-9a-zA-Z_-]+.png [0-9.]+x,?)+"' $FETCH_FILE

test_filter responsive_images,rewrite_images,+inline_images adds srcset for \
  Puzzle.jpg, but not Cuppa.png
# Cuppa.png will be inlined, so we should not get a srcset for it.
fetch_until $URL 'grep -c Cuppa.png' 0  # Make sure Cuppa.png is inlined.
fetch_until $URL 'grep -c srcset=' 2    # And only two srcsets (for Puzzle.jpg).
# Make sure all Puzzle URLs are rewritten.
fetch_until -save $URL 'grep -c [^x]Puzzle.jpg' 0
check egrep -q 'xPuzzle.jpg.pagespeed.+srcset="([^ ]*images/([0-9]+x[0-9]+)?xPuzzle.jpg.pagespeed.ic.[0-9a-zA-Z_-]+.jpg [0-9.]+x,?)+"' $FETCH_FILE
