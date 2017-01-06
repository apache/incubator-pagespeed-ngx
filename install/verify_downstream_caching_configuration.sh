#!/bin/bash
#
# Copyright 2013 Google Inc.
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
# Author: anupama@google.com (Anupama Dutta)
#
# This script can be used to verify whether downstream caching
# (https://modpagespeed.com/doc/downstream-caching)
# has been configured correctly on your system by doing wgets with a sample URL
# and looking at the response headers.
#
# This script currently verifies the working of a mod_pagespeed enabled Apache
# server with an Nginx proxy_cache server acting as a downstream caching layer.
#
# Example invocation for this script:
<<SCRIPT_EXAMPLE
./verify_downstream_caching_configuration.sh \
 -u "http://localhost:8051/mod_pagespeed_test/cachable_rewritten_html/downstream_caching.html" \
 -h "proxy_cache.example.com" \
 -s "localhost:8080" \
 -m "GET" \
 -p "http://localhost:8051/purge" \
 -c "/usr/local/apache2/pagespeed_cache"
SCRIPT_EXAMPLE

SAMPLE_URL_TO_WGET=""
WGET_HOST_HEADER=""
APACHE_MOD_PAGESPEED_SERVER=""
PURGE_METHOD=""
PURGE_LOCATION_PREFIX=""
MOD_PAGESPEED_FILE_CACHE_PATH=""

function usage() {
  cat - >&2 <<EOF
  usage: $0
    -u sample_url
    -h host_header
    -s apache_server_host_port
    -m purge_http_method
    -p purge_location_prefix
    -c mod_pagespeed_file_cache_path
EOF
  exit 1
}

# Helper method for issuing a wget for the specified URL (argument).
# Argument 1 represents the URL to be fetched.
# Argument 2 represents any identifier to be appended to generated log
# and output files.
function wget_url() {
  wget "$1" --header="$WGET_HOST_HEADER" -O $TMPDIR/tmp$2.gz \
   -o $TMPDIR/log$2 -S --user-agent="Chrome/26.0" \
   --header="Accept-Encoding: gzip"
}

# Helper method for issuing a wget for the specified URL (argument) and
# returning 1 if there was a MISS and 0 otherwise.
# Argument 1 represents the URL to be fetched.
function wget_url_and_test_for_miss() {
  wget_url $1
  grep -q "X-Cache: MISS" $TMPDIR/log
}

# Helper method for invalidating mod_pagespeed cache by deleting the
# $MOD_PAGESPEED_FILE_CACHE_PATH directory
function invalidate_mod_pagespeed_cache() {
  rm -rf "$MOD_PAGESPEED_FILE_CACHE_PATH"
}

# Helper method for removing a specific URL from proxy_cache (using purges
# issued from Apache server).
function cleanup_url_from_proxy_cache() {
  # Invalidate the mod_pagespeed cache, so that rewriting can start from
  # scratch for the next request.
  invalidate_mod_pagespeed_cache
  # Do a wget on the Apache server, which should indirectly trigger a PURGE in
  # the background. This cleans up the proxy_cache.
  wget_url "$APACHE_URL"
  # Sleep for 2 seconds waiting for the PURGE to happen.
  sleep 2
}

# Prints out the error message to be used as a last resort, if we are baffled
# about how the checks are failing.
function print_last_resort_error_message_and_exit() {
  echo "Looks like this got into the cache while we were not looking! Check if"
  echo "you are getting live traffic for this URL. If yes, try running this"
  echo "script with a not-so-popular URL. If everything else looks correct,"
  echo "mail mod-pagespeed-discuss@googlegroups.com!"
  exit 1
}

# Helper method to validate URLs to make sure they start with http://.
check_and_exit_on_invalid_urls() {
  case $1 in
    http://*) ;;
    *) echo "$1 is not a valid URL. Valid URLs must begin with http://" >&2
       exit 1 ;;
  esac
}

# Parse commandline arguments.
while getopts u:h:s:m:p:c: opt
do
    case "$opt" in
      u)  SAMPLE_URL_TO_WGET="$OPTARG";;
      h)  WGET_HOST_HEADER="$OPTARG";;
      s)  APACHE_MOD_PAGESPEED_SERVER="$OPTARG";;
      m)  PURGE_METHOD="$OPTARG";;
      p)  PURGE_LOCATION_PREFIX="$OPTARG";;
      c)  MOD_PAGESPEED_FILE_CACHE_PATH="$OPTARG";;
      *)  usage;;
    esac
done
shift `expr $OPTIND - 1`

if [ -z "$SAMPLE_URL_TO_WGET" ] || \
   [ -z "$WGET_HOST_HEADER" ] || \
   [ -z "$APACHE_MOD_PAGESPEED_SERVER" ] || \
   [ -z "$PURGE_METHOD" ] || \
   [ -z "$PURGE_LOCATION_PREFIX" ] || \
   [ -z "$MOD_PAGESPEED_FILE_CACHE_PATH" ]; then
  echo "Error in usage!"
  usage
fi

# Validate URL arguments.
check_and_exit_on_invalid_urls "$SAMPLE_URL_TO_WGET"
check_and_exit_on_invalid_urls "$PURGE_LOCATION_PREFIX"

WGET_HOST_HEADER="Host: $WGET_HOST_HEADER"

TMPDIR="/tmp/verify-downstream-caching-configuration-$$"
rm -rf $TMPDIR
mkdir $TMPDIR

# Prompt about invalidating mod_pagespeed cache. This invalidation needs to be
# done at multiple points in the script to aid in verifying the setup.
echo "---------------------------------------------------------------"
echo "This script needs to delete the mod_pagespeed cache located at"
echo "$MOD_PAGESPEED_FILE_CACHE_PATH."
echo -n "Enter Y to proceed or anything else to abort:"
read agreement
if [ "$agreement" != "Y" ]; then
  exit 0
fi
invalidate_mod_pagespeed_cache

# Replace the http://host:port piece of the sample URL with
# http://APACHE_MOD_PAGESPEED_SERVER to get a sample Apache server URL.
URL_PATH=${SAMPLE_URL_TO_WGET#http://*/}
APACHE_URL="http://$APACHE_MOD_PAGESPEED_SERVER/$URL_PATH"

# Use the PURGE_LOCATION_PREFIX to get a Purge URL for the caching layer.
PURGE_URL="$PURGE_LOCATION_PREFIX/$URL_PATH"

# TODO(anupama): More checks to add here: Check that purge location prefix is
# a substring of the sample url to wget. Else something is wrong.

# Check if APACHE_MOD_PAGESPEED_SERVER is running.
if ! wget_url "$APACHE_URL"; then
  echo "---------------------------------------------------------------"
  echo "Wget didn't succeed. Check if your mod_pagespeed enabled apache"
  echo "server is running correctly at "$APACHE_MOD_PAGESPEED_SERVER
  exit 1
fi

# Check if APACHE_MOD_PAGESPEED_SERVER is running mod_pagespeed.
if ! grep -q "X-Mod-Pagespeed" $TMPDIR/log; then
  echo "---------------------------------------------------------------"
  echo "Your apache server at $APACHE_MOD_PAGESPEED_SERVER does not"
  echo "seem to be running mod_pagespeed. Please recheck your configuration."
  exit 1
fi

# Check if APACHE_MOD_PAGESPEED_SERVER has been configured to output non-zero
# max-age values in its Cache-Control headers (via ModifyCachingHeaders off).
grep -q "Cache-Control: .*no-cache" $TMPDIR/log
IS_APACHE_SERVING_NO_CACHE_HEADERS=$?
if [ $IS_APACHE_SERVING_NO_CACHE_HEADERS = 0 ]; then
  echo "---------------------------------------------------------------"
  echo "You don't seem to be using \"ModPagespeedModifyCachingHeaders off\""
  echo "to allow non-zero max-age values to be propagated to and respected"
  echo "by your proxy_cache servers. Press Enter if you want to continue with"
  echo "this and fix things in your proxy_cache layer. Otherwise press"
  echo -n "Ctrl+C and fix your mod_pagespeed-enabled apache server:"
  read ignored
fi

# Do wgets to verify that proxy_pass directive in proxy_cache configuration
# is pointing to a valid Apache backend with mod_pagespeed enabled on it.
# Note that since we already primed the mod_pagespeed cache with a direct wget
# to APACHE_MOD_PAGESPEED_SERVER, this response ought to be rewritten fully,
# and hence cached for the next wget to use as a HIT.
for i in 1 2; do
  wget_url "$SAMPLE_URL_TO_WGET" $i
done

# Check if the backend that proxy_pass is pointing to is running.
if [ ! -s $TMPDIR/tmp1.gz ]; then
  echo "---------------------------------------------------------------"
  echo "Wget didn't succeed. Check if proxy_pass directive in your proxy_cache"
  echo "config is set correctly to point to your mod_pagespeed-enabled Apache"
  echo "server."
  exit 1
fi

# Check if the backend that proxy_pass is pointing to has mod_pagespeed enabled.
if ! grep -q "X-Mod-Pagespeed" $TMPDIR/log1; then
  echo "---------------------------------------------------------------"
  echo "The backend server referenced in proxy_pass in your proxy_cache"
  echo "config seems to be not running mod_pagespeed. Please recheck to see"
  echo "if it is referencing $APACHE_MOD_PAGESPEED_SERVER"
  exit 1
fi

# Confirm that outgoing headers from the proxy_cache server are set to no-cache
# for HTML response irrespective of whether they are cached in proxy_cache or
# not. Etag, Last-Modified and upstream Cache-Control headers should be removed
# from the client response for correct operation.
if ! grep -q "Cache-Control: .*no-cache" $TMPDIR/log1; then
  echo "---------------------------------------------------------------"
  echo "You should set your outgoing headers from proxy_cache to be "
  echo "no-cache for HTML responses. Please do this by adding the following"
  echo "lines to your http block:"
  echo '  map $upstream_http_content_type $new_cache_control_header_val {'
  echo '       default $upstream_http_cache_control;'
  echo '       "~*text/html" "no-cache, max-age=0";'
  echo "  }"
  echo "and adding the following lines to your 'location /' block:"
  echo "  proxy_hide_header Last-Modified;"
  echo "  proxy_hide_header ETag;"
  echo "  proxy_hide_header Cache-Control;"
  echo '  add_header Cache-Control $new_cache_control_header_val;'
  echo "Press Enter if you want to skip this. Otherwise press"
  echo -n "Ctrl+C and fix your proxy_cache config:"
  read ignored
fi

# Check if X-Cache status headers are present. This script needs these
# headers for verification purposes.
if ! grep -q "X-Cache: " $TMPDIR/log1; then
  echo "---------------------------------------------------------------"
  echo "This script relies on the cache status header for many of its "
  echo "checks. Please add the following line to your 'location /' block:"
  echo '  add_header X-Cache $upstream_cache_status;'
  exit 1
fi

# Check if we incorrectly got a BYPASS X-Cache status for the first wget through
# the proxy_cache layer.
if grep -q "X-Cache: BYPASS" $TMPDIR/log1; then
  echo "---------------------------------------------------------------"
  echo "Cache is being bypassed for this request. Please check for the"
  echo "following line in the 'location /' block of your proxy_cache_config:"
  echo "  proxy_cache_bypass <SHOULD_CACHE_BE_BYPASSED>;"
  echo "and confirm that <SHOULD_CACHE_BE_BYPASSED> is defined correctly"
  echo "as per the documentation. Alternately, verify that the URL you have"
  echo "provided for testing is not meant to be bypassed by the cache."
  exit 1
fi

# Check if the first wget through the caching layer resulted in a MISS.
if ! grep -q "X-Cache: MISS" $TMPDIR/log1; then
  echo "---------------------------------------------------------------"
  echo "Looks like your cache already has the response (HIT) or the response"
  echo "has EXPIRED. Please restart your proxy_cache servers (so that the"
  echo "downstream cache is cleared for this URL)."
  exit 1
fi

# Check if the second wget through the caching layer resulted in a HIT.
if ! grep -q "X-Cache: HIT" $TMPDIR/log2; then
  if [ $IS_APACHE_SERVING_NO_CACHE_HEADERS = 0 ]; then
    # MPS is serving no-cache. This will work only if proxy_cache_valid
    # and proxy_ignore_headers are both defined
    echo "---------------------------------------------------------------"
    echo "If you want to cache HTML that is originally marked with no-cache in"
    echo "proxy_cache, you must have the following line in your 'location /'"
    echo "block of your proxy_cache config:"
    echo "  proxy_ignore_headers Cache-Control;"
    echo "You must also add a proxy_cache_valid line to this block to indicate"
    echo "how long you want to cache the content:"
    echo "  proxy_cache_valid <TIME>;"
  else
    # The second wget (effecively third because the first direct wget on the
    # Apache server would have caused background rewriting to happen) should
    # have a HIT. If there are no HITs, there is something very wrong!
    echo "---------------------------------------------------------------"
    echo "Looks like your earlier response did not get cached, or the response"
    echo "has EXPIRED due to very small caching durations, or your cache"
    echo "somehow got purged very quickly. Check the size of your cache. If"
    echo "everything else looks correct, contact"
    echo "mod-pagespeed-discuss@googlegroups.com!"
  fi
  exit 1
fi

# Check if the purge location prefix and purge method have been specified
# correctly by executing curl with a purge URL that directly goes to the caching
# layer. Note: wget does not allow http request methods to be specified, so we
# use curl here.
curl -X "$PURGE_METHOD" "$PURGE_URL" -H "User-Agent: Chrome/26.0" \
 -H "Host: proxy_cache.example.com" >& $TMPDIR/trace
if ! grep -q "Successful purge" $TMPDIR/trace; then
  # Unsuccessful purges could result from incorrect values for
  # purge_location_prefix or purge_method, ACLs for purge block/command being
  # incorrect, zone named for purging being incorrect or cache key used for
  # purging not matching with the proxy_cache_key directive.
  echo "---------------------------------------------------------------"
  echo "Check your location block for handling purges as follows:"
  echo ""
  echo "1) The --purge_location_prefix specified for this script should match"
  echo "   the regexp for the block handling purges."
  echo ""
  echo "2) The --purge_method specified for this script should be correct."
  echo ""
  echo "3) The ACL on the purge-handling block should be correct. Example ACLs:"
  echo "     allow localhost;"
  echo "     allow 127.0.0.1;"
  echo "     allow <YOUR-SERVER-IP>;"
  echo "     deny all;"
  echo ""
  echo "3) Check that the zone named for purging and the cache key match the"
  echo "   variables used in the 'location /' block:"
  echo "     proxy_cache_purge <ZONE_NAME_FOR_CACHING> " \
       '       <PS_CAPABILITY_LIST>$1$is_args$args;'
  echo ""
  echo "   Ideally, the 'location /' block will be using these variables as"
  echo "   follows:"
  echo "     proxy_cache <ZONE_NAME_FOR_CACHING>;"
  echo '     proxy_cache_key <PS_CAPABILITY_LIST>$uri$is_args$args;'
  echo "   Note that <PS_CAPABILITY_LIST> should be defined as suggested in the"
  echo "   documentation."
  exit 1
fi

# Do a series of wgets to check if PURGES issued via mod_pagespeed are being
# received and processed correctly by the proxy_cache server.
# Wget through the caching layer to get a MISS because the previous request
# was a successful purge.
if ! wget_url_and_test_for_miss "$SAMPLE_URL_TO_WGET"; then
  echo "---------------------------------------------------------------"
  print_last_resort_error_message_and_exit
fi
# Now, purge the URL from proxy cache.
cleanup_url_from_proxy_cache "$APACHE_URL"
# Now do a wget through the caching layer and verify that this is a MISS.
# It should be because of the previous background PURGE that should have gotten
# triggered.
if ! wget_url_and_test_for_miss "$SAMPLE_URL_TO_WGET"; then
  echo "---------------------------------------------------------------"
  echo "Looks like the purge from your Apache server was not executed on your"
  echo "proxy_cache server."
  echo "1) Check if the mod_pagespeed-enabled Apache server IPs are part of the"
  echo "   ACLS you specify for your purge-handling block in proxy_cache"
  echo "   config."
  echo "2) Check your ModPagespeedDownstreamCache* directives and make sure"
  echo "   that they match your proxy_cache config."
  echo "If both of the above things are correct, then ..."
  print_last_resort_error_message_and_exit
fi

# Do a series of wgets to check if requests are being sent upstream with all of
# the required info, so that subsequent PURGEs can be succesful.
cleanup_url_from_proxy_cache "$APACHE_URL"
# Invalidate mod_pagespeed cache to allow rewriting to start from scratch for
# this URL.
invalidate_mod_pagespeed_cache
# Do a wget through the caching layer and verify that this is a MISS before
# proceeding to the final wget.
if ! wget_url_and_test_for_miss "$SAMPLE_URL_TO_WGET"; then
  echo "---------------------------------------------------------------"
  print_last_resort_error_message_and_exit
fi
# Sleep for 2 seconds waiting for the PURGE to happen.
sleep 2
# Do a wget through the caching layer and verify that this is a MISS because of
# the previous PURGE that ought to have been triggered.
if ! wget_url_and_test_for_miss "$SAMPLE_URL_TO_WGET"; then
  echo "---------------------------------------------------------------"
  echo "Looks like the purge didn't succeed. Check the 'location /' block to"
  echo "see if the following directives are defined correctly:"
  echo '  proxy_set_header Host $host;'
  echo "  proxy_set_header PS-CapabilityList <PS_CAPABILITY_LIST>;"
  echo "If all of this is correct, then ..."
  print_last_resort_error_message_and_exit
fi

echo "---------------------------------------------------------------"
echo "Your downstream caching setup looks good!"

# Cleanup
rm -rf $TMPDIR
