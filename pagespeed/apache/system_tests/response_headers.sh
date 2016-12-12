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
start_test Request Headers affect MPS options

# Get the special file response_headers.html and test the result.
# This file has Apache request_t headers_out and err_headers_out modified
# according to the query parameter.  The modification happens in
# instaweb_handler when the handler == kGenerateResponseWithOptionsHandler.
# Possible query flags include: headers_out, headers_errout, headers_override,
# and headers_combine.
function response_header_test() {
    query=$1
    mps_on=$2
    comments_removed=$3
    rm -rf $OUTDIR
    mkdir -p $OUTDIR

    # Get the file
    echo $WGET -q -S -O - \
      "$TEST_ROOT/response_headers.html?$query" '>&' $OUTDIR/header_out
    check $WGET -q -S -O - \
      "$TEST_ROOT/response_headers.html?$query" >& $OUTDIR/header_out

    # Make sure that any MPS option headers were stripped
    check_not grep -q ^PageSpeed: $OUTDIR/header_out
    check_not grep -q ^ModPagespeed: $OUTDIR/header_out

    # Verify if MPS is on or off
    if [ $mps_on = "no" ]; then
      # Verify that PageSpeed was off
      check_not fgrep -q 'X-Mod-Pagespeed:' $OUTDIR/header_out
      check_not fgrep -q '<script' $OUTDIR/header_out
    else
      # Verify that PageSpeed was on
      check fgrep -q 'X-Mod-Pagespeed:' $OUTDIR/header_out
      check fgrep -q '<script' $OUTDIR/header_out
    fi

    # Verify if comments were stripped
    if [ $comments_removed = "no" ]; then
      # Verify that comments were not removed
      check fgrep -q '<!--' $OUTDIR/header_out
    else
      # Verify that comments were removed
      check_not fgrep -q '<!--' $OUTDIR/header_out
    fi
}

# headers_out =     MPS: off
# err_headers_out =
response_header_test headers_out no no

# headers_out =
# err_headers_out = MPS: on
response_header_test headers_errout no no

# Note: The next two tests will break if remove_comments gets into the
# CoreFilter set.

# headers_out     = MPS: off, Filters: -remove_comments
# err_headers_out = MPS: on,  Filters: +remove_comments
# err_headers should is processed after headers_out, and so it should override
# but disabling a filter trumps enabling one. The overriding is described in
# the code for build_context_for_request.
response_header_test headers_override yes no

# headers_out     = MPS: on
# err_headers_out = Filters: +remove_comments
response_header_test headers_combine yes yes
