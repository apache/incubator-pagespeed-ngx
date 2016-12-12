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
# Test a scenario where a multi-domain installation is using a
# single CDN for all hosts, and uses a subdirectory in the CDN to
# distinguish hosts.  Some of the resources may already be mapped to
# the CDN in the origin HTML, but we want to fetch them directly
# from localhost.  If we do this successfully (see the MapOriginDomain
# command in customhostheader.example.com in the configuration), we will
# inline a small image.
start_test shared CDN short-circuit back to origin via host-header override
URL="http://customhostheader.example.com/map_origin_host_header.html"
http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
    "grep -c data:image/png;base64" 1
