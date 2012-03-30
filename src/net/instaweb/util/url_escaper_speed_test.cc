/*
 * Copyright 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: morlovich@google.com (Maksim Orlovich)
//
//
// On a T3500:
// Benchmark               Time(ns)    CPU(ns) Iterations
// ------------------------------------------------------
// BM_EncodeToUrlSegment      11698      11657      58333

#include "net/instaweb/util/public/url_escaper.h"

#include "net/instaweb/util/public/benchmark.h"
#include "net/instaweb/util/public/string.h"

using net_instaweb::UrlEscaper::EncodeToUrlSegment;

static void BM_EncodeToUrlSegment(int iters) {
  for (int i = 0; i < iters; ++i) {
    GoogleString out;

    // These were randomly selected from inputs to EncodeToUrlSegment when
    // rewriting a slurp of www.att.net with AllFilters
    // (by logging the argument of EncodeToUrlSegment, then just selecting
    //  10 random urls using grep, shuf, cut and head).
    EncodeToUrlSegment(
        "http://icds.portal.att.net/test/yModule/GreyDot.jpg", &out);
    EncodeToUrlSegment(
        "http://icds.portal.att.net/test/yModule/xGreyDot.jpg.pagespeed."
        "ic.I6DpW8JR0H.jpg", &out);
    EncodeToUrlSegment("data-key:RytPS5P4EF@http://www.att.net/", &out);
    EncodeToUrlSegment(
        "http://yrss.api.att.net/cobrand/attportal/css/att_dg_prodGames.css"
        "+marketPlaceStyle.css.pagespeed.cc.JT5aZ1e05e.css", &out);
    EncodeToUrlSegment("data-key:nm_yrDndPc@http://www.att.net/", &out);
    EncodeToUrlSegment("data-key:SS9cJh2qLi@http://www.att.net/", &out);
    EncodeToUrlSegment("data-key:fOUuiM7UUs@http://www.att.net/", &out);
    EncodeToUrlSegment("data-key:RytPS5P4EF@http://www.att.net/", &out);
    EncodeToUrlSegment(
        "http://l.yimg.com/a/i/ww/news/2010/12/08/120810sky-sm.jpg", &out);
    EncodeToUrlSegment("data-key:fOUuiM7UUs@http://www.att.net/", &out);
    EncodeToUrlSegment(
        "http://icds.portal.att.net/oberon/images/24png/xOrg_button.png."
        "pagespeed.ic.tN-0QmjlGb.png", &out);
    EncodeToUrlSegment("data-key:yfVl4rtykl@http://www.att.net/", &out);
    EncodeToUrlSegment(
        "http://icds.portal.att.net/oberon/images/24png/Org_button.png", &out);
    EncodeToUrlSegment(
        "http://icds.portal.att.net/oberon/plants_vs_zombies130x75.gif", &out);
    EncodeToUrlSegment(
        "http://l.yimg.com/a/i/ww/news/2010/12/08/74x42xlennon_imagine3_sm.jpg."
        "pagespeed.ic.SfRHRwKi7r.jpg", &out);
  }
}

BENCHMARK(BM_EncodeToUrlSegment);
