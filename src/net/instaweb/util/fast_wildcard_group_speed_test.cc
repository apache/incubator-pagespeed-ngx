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
#include "net/instaweb/util/public/benchmark.h"
#include "net/instaweb/util/public/fast_wildcard_group.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/wildcard_group.h"


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
      case 13:
        Disallow("*tiny_mce*");
      case 12:
        Disallow("*tinymce*");
      case 11:
        Disallow("*scriptaculous.js*");
      case 10:
        Disallow("*connect.facebook.net/*");
      case 9:
        Disallow("*ckeditor*");
      case 8:
        Disallow("*//ajax.googleapis.com/ajax/libs/*");
      case 7:
        Disallow("*//pagead2.googlesyndication.com/pagead/show_ads.js*");
      case 6:
        Disallow("*//partner.googleadservices.com/gampad/google_service.js*");
      case 5:
        Disallow("*//platform.twitter.com/widgets.js*");
      case 4:
        Disallow("*//s7.addthis.com/js/250/addthis_widget.js*");
      case 3:
        Disallow("*//www.google.com/coop/cse/brand*");
      case 2:
        Disallow("*//www.google-analytics.com/urchin.js*");
      case 1:
        Disallow("*//www.googleadservices.com/pagead/conversion.js*");
      default:
        {}  // Fall through
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

static void BM_WildcardGroup(int iters, int size) {
  int actual_size = size / 2;
  bool include_wildcards = (size % 2) == 1;
  UrlBlacklistBenchmark<WildcardGroup>(iters, actual_size, include_wildcards);
}

static void BM_FastWildcardGroup(int iters, int size) {
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
