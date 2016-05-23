// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: morlovich@google.com (Maksim Orlovich)

// Unit-test base-class url naming.

#include "net/instaweb/rewriter/public/measurement_proxy_url_namer.h"

#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

class MeasurementProxyUrlNamerTest : public RewriteTestBase {
 protected:
  MeasurementProxyUrlNamerTest() : namer_("https://www.example.com/", "pwd") {}
  MeasurementProxyUrlNamer namer_;
};

TEST_F(MeasurementProxyUrlNamerTest, DecodePathDetails) {
  StringPiece config, config_domain, password;
  GoogleString res_url;

  GoogleUrl url1("https://www.example.com/h/c1/s1/modpagespeed.com/a/b/c.d?e");
  EXPECT_TRUE(MeasurementProxyUrlNamer::DecodePathDetails(
                  url1, &config, &config_domain, &password, &res_url));
  EXPECT_EQ("c1", config);
  EXPECT_EQ("modpagespeed.com", config_domain);
  EXPECT_EQ("s1", password);
  EXPECT_EQ("http://modpagespeed.com/a/b/c.d?e", res_url);

  GoogleUrl url2(
      "https://www.example.com/x/c2/s2/ngxpagespeed.com/foo.com/b/c.d?e");
  EXPECT_TRUE(MeasurementProxyUrlNamer::DecodePathDetails(
                  url2, &config, &config_domain, &password, &res_url));
  EXPECT_EQ("c2", config);
  EXPECT_EQ("ngxpagespeed.com", config_domain);
  EXPECT_EQ("s2", password);
  EXPECT_EQ("http://foo.com/b/c.d?e", res_url);

  GoogleUrl url3("https://www.example.com/s/c3/s3/modpagespeed.com/b/");
  EXPECT_TRUE(MeasurementProxyUrlNamer::DecodePathDetails(
                  url3, &config, &config_domain, &password, &res_url));
  EXPECT_EQ("c3", config);
  EXPECT_EQ("modpagespeed.com", config_domain);
  EXPECT_EQ("s3", password);
  EXPECT_EQ("https://modpagespeed.com/b/", res_url);

  GoogleUrl url4(
      "https://www.example.com/t/c4/s4/ngxpagespeed.com/foo.com/b");
  EXPECT_TRUE(MeasurementProxyUrlNamer::DecodePathDetails(
                  url4, &config, &config_domain, &password, &res_url));
  EXPECT_EQ("c4", config);
  EXPECT_EQ("ngxpagespeed.com", config_domain);
  EXPECT_EQ("s4", password);
  EXPECT_EQ("https://foo.com/b", res_url);

  GoogleUrl url5(
      "https://www.example.com/x/c5/s5/ngxpagespeed.com/foo.com/");
  EXPECT_TRUE(MeasurementProxyUrlNamer::DecodePathDetails(
                  url5, &config, &config_domain, &password, &res_url));
  EXPECT_EQ("c5", config);
  EXPECT_EQ("ngxpagespeed.com", config_domain);
  EXPECT_EQ("s5", password);
  EXPECT_EQ("http://foo.com/", res_url);

  GoogleUrl url6(
      "https://www.example.com/s/c6/s6/modpagespeed.com/");
  EXPECT_TRUE(MeasurementProxyUrlNamer::DecodePathDetails(
                  url6, &config, &config_domain, &password, &res_url));
  EXPECT_EQ("c6", config);
  EXPECT_EQ("modpagespeed.com", config_domain);
  EXPECT_EQ("s6", password);
  EXPECT_EQ("https://modpagespeed.com/", res_url);

  GoogleUrl url7(
      "https://www.example.com/x/c6/s6/modpagespeed.com");
  EXPECT_FALSE(MeasurementProxyUrlNamer::DecodePathDetails(
                   url7, &config, &config_domain, &password, &res_url));

  GoogleUrl url8(
      "https://www.example.com/s/c6/s6//");
  EXPECT_FALSE(MeasurementProxyUrlNamer::DecodePathDetails(
                   url8, &config, &config_domain, &password, &res_url));

  GoogleUrl url9(
       "https://www.example.com/x/c6/s6/modpagespeed.com/");
  EXPECT_FALSE(MeasurementProxyUrlNamer::DecodePathDetails(
                   url9, &config, &config_domain, &password, &res_url));

  // Wrong code.
  GoogleUrl url10(
      "https://www.example.com/q/c6/s6/modpagespeed.com/");
  EXPECT_FALSE(MeasurementProxyUrlNamer::DecodePathDetails(
                   url10, &config, &config_domain, &password, &res_url));

  GoogleUrl url11(
      "https://www.example.com/s/c6/s6/");
  EXPECT_FALSE(MeasurementProxyUrlNamer::DecodePathDetails(
                   url11, &config, &config_domain, &password, &res_url));
}

TEST_F(MeasurementProxyUrlNamerTest, Decode) {
  GoogleString decoded;
  GoogleUrl good_url(
      "https://www.example.com/h/c1/s1/modpagespeed.com/a/b/c.d?e");
  EXPECT_TRUE(namer_.Decode(good_url, nullptr, &decoded));
  EXPECT_EQ("http://modpagespeed.com/a/b/c.d?e", decoded);

  GoogleUrl bad_url("https://www.example.com/s/c6/");
  EXPECT_FALSE(namer_.Decode(bad_url, nullptr, &decoded));
}

TEST_F(MeasurementProxyUrlNamerTest, Encode) {
  ResourceNamer full_name;
  full_name.set_id(RewriteOptions::kCacheExtenderId);
  full_name.set_name("foo.css");
  full_name.set_ext("css");
  full_name.set_hash("0");

  OutputResourcePtr same_domain(new OutputResource(
      rewrite_driver(),
      "http://www.modpagespeed.com/",
      "http://www.modpagespeed.com/",
      "http://www.modpagespeed.com/",
      full_name,
      kRewrittenResource));
  EXPECT_EQ("http://www.modpagespeed.com/foo.css.pagespeed.ce.0.css",
            namer_.Encode(options(), *same_domain.get(), UrlNamer::kSharded));

  OutputResourcePtr cross_domain(new OutputResource(
      rewrite_driver(),
      "http://cdn.modpagespeed.com/",
      "http://cdn.modpagespeed.com/",
      "http://www.modpagespeed.com/",
      full_name,
      kRewrittenResource));
  EXPECT_EQ(
      "http://cdn.modpagespeed.com/foo.css.pagespeed.ce.0.css",
      namer_.Encode(options(), *cross_domain.get(), UrlNamer::kSharded));

  OutputResourcePtr same_domain_ssl(new OutputResource(
      rewrite_driver(),
      "https://www.modpagespeed.com/",
      "https://www.modpagespeed.com/",
      "https://www.modpagespeed.com/",
      full_name,
      kRewrittenResource));
  EXPECT_EQ(
      "https://www.modpagespeed.com/foo.css.pagespeed.ce.0.css",
      namer_.Encode(options(), *same_domain_ssl.get(), UrlNamer::kSharded));

  OutputResourcePtr cross_domain_ssl(new OutputResource(
      rewrite_driver(),
      "https://cdn.modpagespeed.com/",
      "https://cdn.modpagespeed.com/",
      "http://www.modpagespeed.com/",
      full_name,
      kRewrittenResource));
  EXPECT_EQ(
      "https://cdn.modpagespeed.com/foo.css.pagespeed.ce.0.css",
      namer_.Encode(options(), *cross_domain_ssl.get(), UrlNamer::kSharded));
}

TEST_F(MeasurementProxyUrlNamerTest, IsProxyEncoded) {
  GoogleUrl good_url(
    "https://www.example.com/h/c1/pwd/modpagespeed.com/a/b/c.d?e");
  EXPECT_TRUE(namer_.IsProxyEncoded(good_url));

  GoogleUrl almost_good_url1(
    "https://www.example.com/h/c1/notpwd/modpagespeed.com/a/b/c.d?e");
  EXPECT_FALSE(namer_.IsProxyEncoded(almost_good_url1));

  GoogleUrl almost_good_url2(
    "http://www.example.com/h/c1/pwd/modpagespeed.com/a/b/c.d?e");
  EXPECT_FALSE(namer_.IsProxyEncoded(almost_good_url2));

  GoogleUrl bad_url(
    "https://www.example.com/sadly/wrong");
  EXPECT_FALSE(namer_.IsProxyEncoded(bad_url));
}

}  // namespace net_instaweb
