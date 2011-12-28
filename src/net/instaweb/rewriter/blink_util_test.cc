// Copyright 2011 Google Inc. All Rights Reserved.
// Author: gagansingh@google.com (Gagan Singh)

#include "net/instaweb/rewriter/public/blink_util.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/panel_config.pb.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/proto2/public/text_format.h"
#include "testing/base/public/gunit.h"

namespace net_instaweb {

namespace {

class BlinkUtilTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    GoogleString panel_config_str =
        "web_site: \"www.cname.com\"\n"
        "layout {\n"
        "  relative_url_patterns: \"/lunr.py\\?type=*\"\n"
        "  relative_url_patterns: \"/lunr.py\\?q=*\"\n"
        "  page_max_age_s: 1000\n"
        "  layout_max_age_s: 100\n"
        "  reference_page_url_path: \"/lunr.py?type=root.clothing\"\n"
        "  panel_set {\n"
        "    panels {\n"
        "      start_xpath: \"//div[@id = \\\"container\\\"]\"\n"
        "      num_critical_instances: 10\n"
        "    }\n"
        "  }\n"
        "}\n";
    ASSERT_TRUE(proto2::TextFormat::ParseFromString(panel_config_str,
                                                    &config_));
  }

  PublisherConfig config_;
};

TEST_F(BlinkUtilTest, FindLayout_CorrectUrl) {
  GoogleUrl url("http://www.cname.com/lunr.py?type=blah");

  const Layout* layout = PanelUtil::FindLayout(config_, url);
  EXPECT_TRUE(layout != NULL);
  EXPECT_EQ(10, layout->panel_set().panels(0).num_critical_instances());
}

TEST_F(BlinkUtilTest, FindLayout_IncorrectUrl) {
  GoogleUrl url("http://www.cname.com/blah?q=bluh");

  const Layout* layout = PanelUtil::FindLayout(config_, url);
  EXPECT_TRUE(layout == NULL);
}

}  // namespace
}  // namespace net_instaweb
