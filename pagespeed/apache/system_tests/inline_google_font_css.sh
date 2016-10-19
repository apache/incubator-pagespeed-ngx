if ! "$SKIP_EXTERNAL_RESOURCE_TESTS"; then
  start_test inline_google_font_css before move_to_head and move_above_scripts
  URL="$TEST_ROOT/move_font_css_to_head.html"
  URL+="?PageSpeedFilters=inline_google_font_css,"
  URL+="move_css_to_head,move_css_above_scripts"
  # Make sure the font CSS link tag is eliminated.
  fetch_until -save $URL 'grep -c link' 0
  # Check that we added fonts to the page.
  check [ $(fgrep -c '@font-face' $FETCH_FILE) -gt 0 ]
  # Make sure last style line is before first script line.
  last_style=$(fgrep -n '<style>' $FETCH_FILE | tail -1 | grep -o '^[^:]*')
  first_script=$(\
    fgrep -n '<script>' $FETCH_FILE | tail -1 | grep -o '^[^:]*')
  check [ "$last_style" -lt "$first_script" ]
fi
