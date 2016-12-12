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
test_filter hint_preload_subresources works, and finds indirects

# Expect 5 resources to be hinted
fetch_until -save $URL 'grep -c ^Link:' 5 --save-headers
OUT=$(cat $FETCH_UNTIL_OUTFILE)

check_from "$OUT" fgrep \
  'Link: </mod_pagespeed_example/styles/all_using_imports.css>; rel=preload; as=style; nopush'
check_from "$OUT" fgrep \
  'Link: </mod_pagespeed_example/styles/yellow.css>; rel=preload; as=style; nopush'
check_from "$OUT" fgrep \
  'Link: </mod_pagespeed_example/styles/blue.css>; rel=preload; as=style; nopush'
check_from "$OUT" fgrep \
  'Link: </mod_pagespeed_example/styles/bold.css>; rel=preload; as=style; nopush'
check_from "$OUT" fgrep \
  'Link: </mod_pagespeed_example/inline_javascript.js>; rel=preload; as=script; nopush'
