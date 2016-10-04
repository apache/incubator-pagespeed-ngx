test_filter make_show_ads_async works
OUT=$($WGET_DUMP $URL)
check_from     "$OUT" grep -q 'data-ad'
check_not_from "$OUT" grep -q 'google_ad'
check_from     "$OUT" grep -q 'adsbygoogle.js'
check_not_from "$OUT" grep -q 'show_ads.js'
check_from     "$OUT" fgrep -q "<script>(adsbygoogle = window.adsbygoogle || []).push({})</script>"
