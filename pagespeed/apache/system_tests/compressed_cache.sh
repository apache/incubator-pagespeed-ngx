function scrape_secondary_stat {
  http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
    "$SECONDARY_ROOT/mod_pagespeed_statistics/" | \
    scrape_pipe_stat "$1"
}

if [ $statistics_enabled = "1" ]; then
  start_test CompressedCache is racking up savings on the root vhost
  original_size=$(scrape_stat compressed_cache_original_size)
  compressed_size=$(scrape_stat compressed_cache_compressed_size)
  echo original_size=$original_size compressed_size=$compressed_size
  check [ "$compressed_size" -lt "$original_size" ];
  check [ "$compressed_size" -gt 0 ];
  check [ "$original_size" -gt 0 ];

  if [ "$SECONDARY_HOSTNAME" != "" ]; then
    start_test CompressedCache is turned off for the secondary vhost
    original_size=$(scrape_secondary_stat compressed_cache_original_size)
    compressed_size=$(scrape_secondary_stat compressed_cache_compressed_size)
    check [ "$compressed_size" -eq 0 ];
    check [ "$original_size" -eq 0 ];
  fi
else
  echo skipping CompressedCache test because stats is $statistics_enabled
fi
