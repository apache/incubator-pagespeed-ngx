#!/bin/bash
#
# Copyright 2016 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Runs system tests for system/ and automatic/.
#
# See automatic/system_test_helpers.sh for usage.
#

# Default to not running the controller, unless specified to
# do so via an environment variable.
RUN_CONTROLLER_TEST=${RUN_CONTROLLER_TEST:-off}
IS_FILE_CACHE=false
if [ "${MEMCACHED_PORT:-0}" -eq 0 ] && [ "${REDIS_PORT:-0}" -eq 0 ]; then
  IS_FILE_CACHE=true
fi
FIRST_RUN=${FIRST_RUN:-false}

# To fetch from the secondary test root, we must set
# http_proxy=${SECONDARY_HOSTNAME} during fetches.
SECONDARY_ROOT="http://secondary.example.com"
SECONDARY_TEST_ROOT="$SECONDARY_ROOT/mod_pagespeed_test"

# Run the automatic/ system tests.
#
# We need to know the directory this file is located in.  Unfortunately,
# if we're 'source'd from a script in a different directory $(dirname $0) gives
# us the directory that *that* script is located in
this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/../automatic/system_test.sh" || exit 1

# TODO(jefftk): move all tests from apache/system_test.sh to here except the
# ones that actually are Apache-specific.

# Define a mechanism to start a test before the cache-flush and finish it
# after the cache-flush.  This mechanism is preferable to flushing cache
# within a test as that requires waiting 5 seconds for the poll, so we'd
# like to limit the number of cache flushes and exploit it on behalf of
# multiple tests.

# Variable holding a space-separated lists of bash functions to run after
# flushing cache.
post_cache_flush_test=""

# Adds a new function to run after cache flush.
function on_cache_flush() {
  post_cache_flush_test+=" $1"
}

# Called after cache-flush to run all the functions specified to
# on_cache_flush.
function run_post_cache_flush() {
  for test in $post_cache_flush_test; do
    $test
  done
}

rm -rf $OUTDIR
mkdir -p $OUTDIR

SUDO=${SUDO:-}

SYSTEM_TEST_DIR="$(dirname "${BASH_SOURCE[0]}")/system_tests/"
run_test check_headers
run_test aris
run_test css_combining_authorization
run_test add_instrumentation
run_test cache_partial_html
run_test flush_subresources
run_test respect_custom_options
run_test load_from_file
run_test more_custom_headers
run_test modify_caching_headers
run_test combine_javascript
run_test map_proxy_domain
run_test instant_ipro
if [ "$RUN_CONTROLLER_TEST" = "on" ]; then
  run_test controller
fi
run_test json_content_type
run_test shard_domain
run_test server_side_includes
run_test flush_handling
if [ $statistics_enabled = "1" ]; then
  run_test resource_404_count
  run_test statistics_are_local_only
  run_test ipro_vary_cookie
  run_test authorization_basic
  run_test image_rewrite_locking
fi
run_test prioritize_critical_css
if [ "$SECONDARY_HOSTNAME" != "" ]; then
  run_test pagespeed_on_off_unplugged_standby
  run_test ajax_overrides_experiments

  # The broken_fetch test can only run with a file-cache, not with
  # an external cache.
  if $IS_FILE_CACHE; then
    run_test broken_fetch_ipro_record
  else
    echo Skipping broken_fetch_ipro_record, which only runs with file cache.
  fi
  run_test query_params_dont_enable_core_filters
  run_test optimize_for_bandwidth
  run_test shm_cache
  run_test max_cachable
  run_test x_header_value
  run_test domain_rewrite_hyperlinks
  run_test static_asset_domain_rewrite
  run_test url_valued_attributes
  run_test ipro_load_from_file
  run_test ipro_max_cachable
  run_test experiment_device_types
  run_test downstream_cache_integration_headers
  run_test x_sendfile
  run_test shared_cdn_host_header
  run_test ipro_for_browser
  run_test request_option_override
  run_test ipro_caching
  run_test client_domain_rewrite
  run_test resize_rendered_dimensions
  if [ -z "${DISABLE_PHP_TESTS:-}" ]; then
    run_test cache_compression_pre_gzipping
  fi
  run_test downstream_cache_rebeaconing
  run_test critical_image_beaconing
  run_test no_critical_unauthorized_resources
  run_test mapped_domain_relative_css
  run_test forbid_filters
  run_test embed_config
  run_test preserve_urls
  run_test no_transform
  run_test respect_vary
  run_test inline_unauth
  run_test max_combined_css_bytes
  run_test sane_connection_header
  run_test handler_access_messages
fi
run_test show_cache
run_test message_history_colors
run_test large_html_files
run_test ipro_fixed_size
run_test bad_query_params_and_headers
run_test no_respect_vary
run_test cross_site_fetch
run_test source_maps
run_test resource_ext_corruption
run_test outline_javascript_limit
run_test retain_comment
run_test ipro_source_map
run_test ipro_noop
if [ "$CACHE_FLUSH_TEST" = "on" ]; then
  run_test cache_purge
  run_test rewrite_deadline_ipro
fi
run_test add_resource_headers
run_test long_url_handling
run_test controller_process_handling
run_test strip_subresources
run_test protocol_relative_urls
