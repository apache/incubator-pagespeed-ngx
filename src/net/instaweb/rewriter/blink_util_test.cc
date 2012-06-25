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

// Author: gagansingh@google.com (Gagan Singh)

#include "net/instaweb/rewriter/public/blink_util.h"

#include "base/logging.h"               // for operator<<, etc
#include "net/instaweb/rewriter/panel_config.pb.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "testing/base/public/gunit.h"

namespace net_instaweb {

namespace {

const char kSamplePageJsonData[] =
    "[{"

      "\"contiguous\":false,"
      "\"panel-id.0\":[{"
        "\"contiguous\":false,"
        "\"instance_html\":\"<div>0 instance</div>\","
        "\"images\":{\"image0\":\"Lowres\"},"
        "\"panel-id.1\":[{"
          "\"contiguous\":false,"
          "\"instance_html\":\"<div>0.0 instance</div>\","
          "\"images\":{\"image0.0\":\"Lowres\"}"
        "},{"
          "\"contiguous\":true,"
          "\"instance_html\":\"<div>0.1 instance</div>\","
          "\"images\":{\"image0.1\":\"Lowres\"}"
        "},{"
          "\"contiguous\":true,"
          "\"instance_html\":\"<div>0.2 instance</div>\","
          "\"images\":{\"image0.2\":\"Lowres\"}"
        "}]"
      "},"
      "{"
        "\"contiguous\":true,"
        "\"instance_html\":\"<div>1 instance</div>\","
        "\"images\":{\"image1\":\"Lowres\"},"
        "\"panel-id.1\":[{"
          "\"contiguous\":false,"
          "\"instance_html\":\"<div>1.0 instance</div>\","
          "\"images\":{\"image1.1\":\"Lowres\"}"
        "},"
        "{"
          "\"contiguous\":true,"
          "\"instance_html\":\"<div>1.1 instance</div>\","
          "\"images\":{\"image1.2\":\"Lowres\"}"
        "}]"
      "}]"
    "}]\n";

class BlinkUtilTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // TODO(rahulbansal): Share the configs between tests.
    cname_config_.set_web_site("www.cname.com");
    Layout* layout = cname_config_.add_layout();
    layout->add_relative_url_patterns("/lunr.py\\?type=.*");
    layout->add_relative_url_patterns("/lunr.py\\?q=.*");
    layout->set_page_max_age_s(1000);
    layout->set_layout_max_age_s(100);
    PanelSet* panel_set = layout->mutable_panel_set();
    Panel* panel = panel_set->add_panels();
    panel->set_start_xpath("//div[@id = \"container\"]");
    panel->set_num_critical_instances(10);

    example_config_.set_web_site("www.example.com");
    layout = example_config_.add_layout();
    layout->add_relative_url_patterns("/.*");
    layout->set_page_max_age_s(10000);
    layout->set_layout_max_age_s(1000);
    panel_set = layout->mutable_panel_set();
    panel = panel_set->add_panels();
    panel->set_start_xpath("//div[@id = \"container\"]");
    panel->set_num_critical_instances(1);
    panel = panel_set->add_panels();
    panel->set_start_xpath("//div[@class = \"item\"]");
    panel->set_num_critical_instances(2);
    panel->set_cacheability_in_minutes(0);
    panel = panel_set->add_panels();
    panel->set_start_xpath("//div[@class = \"inspiration\"]");
    panel->set_num_critical_instances(1);
    panel = panel_set->add_panels();
    panel->set_start_xpath("//img[@class = \"image\"]");
    panel->set_end_marker_xpath("//h1[@id = \"footer\"]");
    panel->set_num_critical_instances(1);
  }

  PublisherConfig cname_config_;
  PublisherConfig example_config_;
};

TEST_F(BlinkUtilTest, FindLayout_CorrectUrl) {
  GoogleUrl url("http://www.cname.com/lunr.py?type=blah");

  const Layout* layout = BlinkUtil::FindLayout(cname_config_, url);
  EXPECT_TRUE(layout != NULL);
  EXPECT_EQ(10, layout->panel_set().panels(0).num_critical_instances());
}

TEST_F(BlinkUtilTest, FindLayout_IncorrectUrl) {
  GoogleUrl url("http://www.cname.com/blah?q=bluh");

  const Layout* layout = BlinkUtil::FindLayout(cname_config_, url);
  EXPECT_TRUE(layout == NULL);
}

TEST_F(BlinkUtilTest, SplitCritical) {
  const char expected_critical_json[] =
      "{"
        "\"contiguous\":false,"
        "\"panel-id.0\":[{"
          "\"contiguous\":false,"
          "\"instance_html\":\"<div>0 instance</div>\""
        "}]"
      "}";

  const char expected_non_critical_json[] =
      "{"
        "\"contiguous\":false,"
        "\"panel-id.0\":[{"
          "\"contiguous\":false,"
          "\"panel-id.1\":[{"
            "\"contiguous\":false"
          "},{"
            "\"contiguous\":true"
          "},{"
            "\"contiguous\":true,"
            "\"instance_html\":\"<div>0.2 instance</div>\""
          "}]"
        "},{"
          "\"contiguous\":true,"
          "\"instance_html\":\"<div>1 instance</div>\","
          "\"panel-id.1\":[{"
            "\"contiguous\":false,"
            "\"instance_html\":\"<div>1.0 instance</div>\""
          "},{"
            "\"contiguous\":true,"
            "\"instance_html\":\"<div>1.1 instance</div>\""
          "}]"
        "}]"
      "}";

  const char expected_pushed_content[] =
      "{"
        "\"image0\":\"Lowres\""
      "}";

  const PanelSet* panel_set = &(example_config_.layout(0).panel_set());
  PanelIdToSpecMap panel_id_to_spec;
  BlinkUtil::ComputePanels(panel_set, &panel_id_to_spec);
  Json::Value complete_json;
  Json::Reader json_reader;
  if (!json_reader.parse(kSamplePageJsonData, complete_json)) {
    LOG(FATAL) << "Couldn't parse Json "<< kSamplePageJsonData;
  }

  GoogleString critical_json_str, non_critical_json_str, pushed_images_str;
  BlinkUtil::SplitCritical(complete_json, panel_id_to_spec, &critical_json_str,
                           &non_critical_json_str, &pushed_images_str);

  EXPECT_EQ(expected_pushed_content, pushed_images_str);
  EXPECT_EQ(expected_critical_json, critical_json_str);
  EXPECT_EQ(expected_non_critical_json, non_critical_json_str);
}

TEST_F(BlinkUtilTest, SplitCritical_NoImages) {
  Json::Value val(Json::arrayValue);
  Json::Value val_obj(Json::objectValue);
  val_obj["instance_html"] = "blah";

  val_obj["contiguous"] = "blah";

  val.append(val_obj);
  PanelIdToSpecMap panel_id_to_spec;
  GoogleString critical_json_str, non_critical_json_str, pushed_images_str;
  BlinkUtil::SplitCritical(val, panel_id_to_spec, &critical_json_str,
                           &non_critical_json_str, &pushed_images_str);
  EXPECT_EQ("{}", pushed_images_str);
}

TEST_F(BlinkUtilTest, ClearArrayIfAllEmpty) {
  Json::Value val(Json::arrayValue);
  Json::Value val_obj(Json::objectValue);
  val_obj["contiguous"] = "blah";

  val.append(val_obj);
  val.append(val_obj);
  val.append(val_obj);

  BlinkUtil::ClearArrayIfAllEmpty(&val);
  EXPECT_EQ(0, val.size());

  val.append(val_obj);

  val_obj["instance_html"] = "blah";
  val.append(val_obj);
  BlinkUtil::ClearArrayIfAllEmpty(&val);
  EXPECT_EQ(2, val.size());
}

TEST_F(BlinkUtilTest, IsJsonEmpty) {
  Json::Value val_obj(Json::objectValue);
  EXPECT_TRUE(BlinkUtil::IsJsonEmpty(val_obj));
  val_obj["contiguous"] = "blah";

  EXPECT_TRUE(BlinkUtil::IsJsonEmpty(val_obj));

  val_obj["instance_html"] = "blah";
  EXPECT_FALSE(BlinkUtil::IsJsonEmpty(val_obj));
}

TEST_F(BlinkUtilTest, EscapeString) {
  GoogleString str1 = "<stuff\xe2\x80\xa8>\n\\n";
  BlinkUtil::EscapeString(&str1);
  EXPECT_EQ("__psa_lt;stuff\\u2028__psa_gt;\n\\n", str1);
  GoogleString str2 = "<|  |\\n";  // Has couple of U+2028's betwen the |
  BlinkUtil::EscapeString(&str2);
  EXPECT_EQ("__psa_lt;|\\u2028\\u2028|\\n", str2);
}

}  // namespace
}  // namespace net_instaweb
