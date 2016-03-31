/*
 * Copyright 2012 Google Inc.
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

// Author: jmaessen@google.com (Jan-Willem Maessen)

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/benchmark.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/fast_wildcard_group.h"
#include "pagespeed/kernel/base/wildcard_group.h"

//
// (8 X 2262 MHz CPUs); 2012/07/11-19:20:51
// CPU: Intel Nehalem with HyperThreading (4 cores) dL1:32KB dL2:256KB
// ***WARNING*** CPU scaling is enabled, the benchmark timings may be
// noisy, see http://www/eng/howto/testing/microbenchmarks.html
//
// Benchmark                 Time(ns)    CPU(ns) Iterations
// --------------------------------------------------------
// BM_WildcardGroup/0             169        168    4117647
// BM_WildcardGroup/1             623        620    1000000
// BM_WildcardGroup/2            1431       1440     500000
// BM_WildcardGroup/3            2034       1980     333333
// BM_WildcardGroup/6            4581       4600     152174
// BM_WildcardGroup/7            5336       5300     100000
// BM_WildcardGroup/12           8585       8614      77778
// BM_WildcardGroup/13           9028       9000      77778
// BM_WildcardGroup/18          12903      13000      53846
// BM_WildcardGroup/19          13233      13000      50000
// BM_WildcardGroup/20          14773      14786      46667
// BM_WildcardGroup/21          15128      15214      46667
// BM_WildcardGroup/22          16332      16272      41176
// BM_WildcardGroup/23          16665      16686      43750
// BM_WildcardGroup/28          22773      22671      30435
// BM_WildcardGroup/29          22996      23000      30435
// BM_FastWildcardGroup/0         168        168    4117647
// BM_FastWildcardGroup/1         593        590    1000000
// BM_FastWildcardGroup/2        1467       1457     466667
// BM_FastWildcardGroup/3        2065       2070     333333
// BM_FastWildcardGroup/6        4532       4500     155556
// BM_FastWildcardGroup/7        5391       5400     100000
// BM_FastWildcardGroup/12       8520       8571      87500
// BM_FastWildcardGroup/13       9057       9000      77778
// BM_FastWildcardGroup/18      12880      13029      58333
// BM_FastWildcardGroup/19      13326      13371      53846
// BM_FastWildcardGroup/20      14688      14357      46667
// BM_FastWildcardGroup/21      15101      15000      46667
// BM_FastWildcardGroup/22      15940      16000      43750
// BM_FastWildcardGroup/23      16423      16457      43750
// BM_FastWildcardGroup/28      17720      17486      38889
// BM_FastWildcardGroup/29      18363      18000      38889
// Note that the above scaling data was used to set kMinPatterns.
//
// Disclaimer: comparing runs over time and across different machines
// can be misleading.  When contemplating an algorithm change, always do
// interleaved runs with the old & new algorithm.

namespace net_instaweb {

namespace {

template<class G>
class UrlBlacklistTest {
 public:
  UrlBlacklistTest(int size, bool include_wildcards)
      : size_(size) {
    if (include_wildcards) {
      Disallow("");
      Allow("?*");
    }
    // See also RewriteOptions::DisallowTroublesomeResources.
    // Here we fall through all statements as each case is inclusive of the ones
    // that follow.
    switch (size) {
      case 14:
        Disallow("*js_tinyMCE*");  // js_tinyMCE.js
        FALLTHROUGH_INTENDED;
      case 13:
        Disallow("*tiny_mce*");
        FALLTHROUGH_INTENDED;
      case 12:
        Disallow("*tinymce*");
        FALLTHROUGH_INTENDED;
      case 11:
        Disallow("*scriptaculous.js*");
        FALLTHROUGH_INTENDED;
      case 10:
        Disallow("*connect.facebook.net/*");
        FALLTHROUGH_INTENDED;
      case 9:
        Disallow("*ckeditor*");
        FALLTHROUGH_INTENDED;
      case 8:
        Disallow("*//ajax.googleapis.com/ajax/libs/*");
        FALLTHROUGH_INTENDED;
      case 7:
        Disallow("*//pagead2.googlesyndication.com/pagead/show_ads.js*");
        FALLTHROUGH_INTENDED;
      case 6:
        Disallow("*//partner.googleadservices.com/gampad/google_service.js*");
        FALLTHROUGH_INTENDED;
      case 5:
        Disallow("*//platform.twitter.com/widgets.js*");
        FALLTHROUGH_INTENDED;
      case 4:
        Disallow("*//s7.addthis.com/js/250/addthis_widget.js*");
        FALLTHROUGH_INTENDED;
      case 3:
        Disallow("*//www.google.com/coop/cse/brand*");
        FALLTHROUGH_INTENDED;
      case 2:
        Disallow("*//www.google-analytics.com/urchin.js*");
        FALLTHROUGH_INTENDED;
      case 1:
        Disallow("*//www.googleadservices.com/pagead/conversion.js*");
        FALLTHROUGH_INTENDED;
      default:
        break;
    }
  }
  ~UrlBlacklistTest() { }

  void PerformLookups() {
    CHECK(IsAllowed("http://platform.linkedin.com/in.js"));
    CHECK(IsAllowed("http://www.minecraftdl.com/wp-content/w3tc/min/"
                    "f2077/default.include.849527.js"));
    CHECK(IsAllowed("http://www.minecraftdl.com/wp-includes/js/jquery/"
                    "jquery.js,qver=1.7.1"));
    CHECK(IsAllowed("http://www.lijit.com/delivery/fp,"
                    "qu=ittikorns,ai=lijit_region_143587,az=143587,an=4"));
    CHECK(IsAllowed("http://www.priceindia.in/cj/js/script.js"));
    CHECK_EQ(
        size_ < 8,
        IsAllowed("http://ajax.googleapis.com/ajax/libs/"
                  "jquery/1.6.4/jquery.min.js"));
    CHECK(IsAllowed("http://annoncesgirls.com/wp-includes/js/jquery/"
                    "ui/jquery.ui.mouse.min.js"));
    CHECK_EQ(
        size_ < 1,
        IsAllowed("http://www.googleadservices.com/pagead/conversion.js"));
    CHECK(IsAllowed("http://anticariatultau.ro/catalog/view/javascript/"
                    "common.js"));
    CHECK(IsAllowed("http://blog.gooera.com/wp-content/plugins/"
                    "search-google/js/search-google.js,qver=1.4"));
    CHECK_EQ(
        size_ < 7,
        IsAllowed("http://pagead2.googlesyndication.com/pagead/"
                  "show_ads.js?_=1339538917578"));
    CHECK(IsAllowed("http://cellcustomize.com/wp-content/themes/yoo_balance_wp/"
                    "js/template.js"));
    CHECK_EQ(
        size_ < 6,
        IsAllowed("http://partner.googleadservices.com/gampad/"
                  "google_service.js"));
    CHECK(IsAllowed("http://cb.yebhi.com/js/combo.js"));
    CHECK(IsAllowed("http://chunchu.org/syntaxhighlighter/scripts/"
                    "shBrushClojure.js"));
    CHECK(IsAllowed("http://angel.ge/templates/moxeve/js/geo.js"));
    CHECK_EQ(
        size_ < 7,
        IsAllowed("http://pagead2.googlesyndication.com/pagead/"
                     "show_ads.js"));
    CHECK(IsAllowed("http://education.ge/SpryAssets/SpryMenuBar.js"));
    CHECK(IsAllowed("http://anticariatultau.ro/catalog/view/javascript/"
                    "common.js"));
    CHECK_EQ(
        size_ < 5,
        IsAllowed("http://platform.twitter.com/widgets.js"));
    CHECK(IsAllowed("http://jishinyochi.net/js/glossy.js"));
    CHECK(IsAllowed("http://mblaze.websiteforever.com/dashboard120607/js/"
                    "region.js"));
    CHECK(IsAllowed("http://members.lovingfromadistance.com/clientscript/"
                    "vbulletin_ajax_htmlloader.js"));
    CHECK(IsAllowed("http://movie-renamer.fr/js/roundabout_shapes.js"));
  }

 private:
  void Allow(const StringPiece& s) {
    blacklist_.Allow(s);
  }
  void Disallow(const StringPiece& s) {
    blacklist_.Disallow(s);
  }
  bool IsAllowed(const StringPiece& s) {
    return blacklist_.Match(s, true);
  }

  G blacklist_;
  int size_;
};

template<class G> static void UrlBlacklistBenchmark(
    int iters, int size, bool include_wildcards) {
  UrlBlacklistTest<G> test_object(size, include_wildcards);
  for (int i = 0; i < iters; ++i) {
    test_object.PerformLookups();
  }
}

void BM_WildcardGroup(int iters, int size) {
  int actual_size = size / 2;
  bool include_wildcards = (size % 2) == 1;
  UrlBlacklistBenchmark<WildcardGroup>(iters, actual_size, include_wildcards);
}

void BM_FastWildcardGroup(int iters, int size) {
  int actual_size = size / 2;
  bool include_wildcards = (size % 2) == 1;
  UrlBlacklistBenchmark<FastWildcardGroup>(
      iters, actual_size, include_wildcards);
}



// Test version of this code, designed to make sure larger wildcard groups are
// routinely exercised.
class FastWildcardGroupScaleTest : public testing::Test {
};

TEST_F(FastWildcardGroupScaleTest, LargeWildcardGroup) {
  UrlBlacklistBenchmark<WildcardGroup>(1, 14, true);
}

TEST_F(FastWildcardGroupScaleTest, LargeFastWildcardGroup) {
  UrlBlacklistBenchmark<FastWildcardGroup>(1, 14, true);
}

}  // namespace

}  // namespace net_instaweb
