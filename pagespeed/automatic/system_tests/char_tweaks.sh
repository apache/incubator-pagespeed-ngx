test_filter collapse_whitespace removes whitespace, but not from pre tags.
check run_wget_with_args $URL
check [ $(egrep -c '^ +<' $FETCHED) -eq 1 ]

test_filter pedantic adds default type attributes.
check run_wget_with_args $URL
check fgrep -q 'text/javascript' $FETCHED # should find script type
check fgrep -q 'text/css' $FETCHED        # should find style type

test_filter remove_comments removes comments but not IE directives.
check run_wget_with_args $URL
check_not grep removed $FETCHED   # comment, should not find
check grep -q preserved $FETCHED  # preserves IE directives

test_filter remove_quotes does what it says on the tin.
check run_wget_with_args $URL
num_quoted=$(sed 's/ /\n/g' $FETCHED | grep -c '"')
check [ $num_quoted -eq 2 ]       # 2 quoted attrs
check_not grep -q "'" $FETCHED    # no apostrophes

test_filter trim_urls makes urls relative
check run_wget_with_args $URL
check_not grep "mod_pagespeed_example" $FETCHED  # base dir, shouldn't find
check_file_size $FETCHED -lt 153  # down from 157
