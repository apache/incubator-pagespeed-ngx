#!/bin/bash
#
# Copyright 2010 Google Inc.
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
# Author: abliss@google.com (Adam Bliss)
#
# Generic system test, which should work on any implementation of Page Speed
# Automatic.
#
# See system_test_helpers.sh for usage.
#
# The shell script sourcing this one is expected to be implementation specific
# and have its own additional system tests that it runs.  After it finishes,
# the sourcing script should call check_failures_and_exit.  That will print the
# names of any failing tests and exit with status 1 if there are any.
#

# We need to know the directory this file is located in.  Unfortunately,
# if we're 'source'd from a script in a different directory $(dirname $0) gives
# us the directory that *that* script is located in
this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/system_test_helpers.sh" || exit 1

# Exit the script on an undefined variable or a failed command.  Note that this
# means we must run all commands that are intended to fail inside "check_not" or
# "check_not_from", which will prevent the script from exiting.
set -e
set -u
trap 'handle_failure Line:${LINENO}' ERR

# General system tests

IMAGES_QUALITY="PageSpeedImageRecompressionQuality"
JPEG_QUALITY="PageSpeedJpegRecompressionQuality"
WEBP_QUALITY="PageSpeedWebpRecompressionQuality"

SYSTEM_TEST_DIR="$(dirname "${BASH_SOURCE[0]}")/system_tests/"

run_critical_test initial_header_check
run_critical_test initial_sanity_checks

# Add new tests here.
run_test gce_public_cache
run_test query_params_in_resource_flow
run_test ipro
run_test smaxage
run_test add_instrumentation
run_test canonicalize_javascript_libraries
run_test combiners
run_test elide_attributes
run_test extend_cache
run_test js_blacklist
run_test move_css
run_test inliners
run_test outliners
run_test char_tweaks
run_test rewrite_css
run_test rewrite_images
run_test image_quality_generic
run_test image_quality_jpeg
run_test image_quality_webp
run_test image_resize
run_test broken_images
run_test make_show_ads_async
run_test responsive_images
run_test shortcut_icons
run_test hint_preload_subresources

# These have to run after image_rewrite tests. Otherwise it causes some images
# to be loaded into memory before they should be.
# TODO(jefftk): Is this actually a problem?
run_test css_images
run_test fallback_rewrite_css_urls
run_test images_in_styles
run_test rewrite_css_images
run_test css_sprite_images
run_test rewrite_javascript
run_test https
run_test convert_meta_tags
run_test lazyload_images
run_test rewrite_compressed_js
run_test no_cache
run_test defer_javascript
run_test inline_preview_images
run_test local_storage_cache
run_test flatten_css_imports
run_test insert_dns_prefetch
run_test dedup_inlined_images
run_test invalid_host_header

if [ "$SECONDARY_HOSTNAME" != "" ]; then
  run_test cookie_options
  run_test sticky_cookie_options
  run_test signed_urls
  run_test redirect_with_ps_params
  run_test optimize_to_webp
  run_test image_quality_and_response
  run_test follow_flushes
fi

run_test content_length
run_test keep_data_urls
run_test rel_canonical
run_test resource_content_type_html

# Remaining tests aren't converted to run_test, so we need to define the fetch
# variables for them and cleanup the now-shared OUTDIR.
define_fetch_variables

rm -rf $OUTDIR
