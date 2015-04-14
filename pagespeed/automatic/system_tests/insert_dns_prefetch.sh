# Test DNS prefetching. DNS prefetching is dependent on user agent, but is
# enabled for Wget UAs, allowing this test to work with our default wget params.
test_filter insert_dns_prefetch
fetch_until $URL 'fgrep -ci //ref.pssdemos.com' 2
fetch_until $URL 'fgrep -ci //ajax.googleapis.com' 2
