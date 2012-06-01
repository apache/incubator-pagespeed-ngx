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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/association_transformer.h"

#include "net/instaweb/rewriter/public/css_url_counter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

namespace {

class DummyResource : public Resource {
 public:
  DummyResource() : Resource(NULL, NULL) {}
  virtual ~DummyResource() {}

  void set_url(const StringPiece& url) {
    url_ = url.as_string();
  }
  virtual GoogleString url() const { return url_; }

  virtual const RewriteOptions* rewrite_options() const { return NULL; }
  virtual bool Load(MessageHandler* handler) { return false; }

 private:
  GoogleString url_;

  DISALLOW_COPY_AND_ASSIGN(DummyResource);
};

class DummyTransformer : public CssTagScanner::Transformer {
 public:
  DummyTransformer() {}
  virtual ~DummyTransformer() {}

  virtual TransformStatus Transform(const StringPiece& in, GoogleString* out) {
    *out = StrCat("Dummy:", in);
    return kSuccess;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyTransformer);
};

}  // namespace

class AssociationTransformerTest : public ::testing::Test {
 protected:
  template <class T>
  void ExpectValue(const std::map<GoogleString, T>& map,
                   const StringPiece& key, const T& expected_value) {
    typename std::map<GoogleString, T>::const_iterator iter =
        map.find(key.as_string());
    ASSERT_NE(map.end(), iter) << "map does not have key " << key;
    EXPECT_EQ(expected_value, iter->second)
        << "map[\"" << key << "\"] not as expected";
  }
};

TEST_F(AssociationTransformerTest, TransformsCorrectly) {
  const char css_template[] =
      "blah fwe.fwei ofe w {{{ "
      "url('%s') fafwe"
      "@import '%s';829hqbr23b"
      "url()"  // Empty URLs are left alone.
      "url(%s)"
      "url(%s)"
      "url(%s)";
  const GoogleString css_before = StringPrintf(
      css_template, "image.gif", "before.css", "http://example.com/before.css",
      "http://other.org/foo.ttf", "data:text/plain,Foobar");

  GoogleUrl base_url("http://example.com/");
  NullMessageHandler handler;
  CssUrlCounter url_counter(&base_url, &handler);
  DummyTransformer backup_trans;
  AssociationTransformer trans(&base_url, &backup_trans, &handler);

  // Run first pass.
  EXPECT_TRUE(url_counter.Count(css_before));

  // Check that 1 URL was discovered and absolutified correctly.
  EXPECT_EQ(4, url_counter.url_counts().size());
  ExpectValue(url_counter.url_counts(), "http://example.com/image.gif", 1);
  ExpectValue(url_counter.url_counts(), "http://example.com/before.css", 2);
  ExpectValue(url_counter.url_counts(), "http://other.org/foo.ttf", 1);
  ExpectValue(url_counter.url_counts(), "data:text/plain,Foobar", 1);

  // Provide URL association.
  DummyResource* resource = new DummyResource;
  ResourcePtr resource_ptr(resource);
  ResourceSlotPtr slot(new AssociationSlot(
      resource_ptr, trans.map(), "http://example.com/before.css"));
  resource->set_url("http://example.com/after.css");
  slot->Render();

  // Check that the association was registered.
  EXPECT_EQ(1, trans.map()->size());
  ExpectValue<GoogleString>(*trans.map(), "http://example.com/before.css",
                            "http://example.com/after.css");

  // Run second pass.
  GoogleString out;
  StringWriter out_writer(&out);
  EXPECT_TRUE(CssTagScanner::TransformUrls(css_before, &out_writer, &trans,
                                           &handler));

  // Check that contents was rewritten correctly.
  const GoogleString css_after = StringPrintf(
      css_template,
      // image.gif did not have an association set, so it was passed to
      // DummyTransformer.
      "Dummy:image.gif",
      // before.css was rewritten in both places to after.css.
      "http://example.com/after.css",
      "http://example.com/after.css",
      // Passed through DummyTransformer.
      "Dummy:http://other.org/foo.ttf",
      "Dummy:data:text/plain,Foobar");
  EXPECT_EQ(css_after, out);
}

TEST_F(AssociationTransformerTest, FailsOnInvalidUrl) {
  const char css_before[] = "url(////)";

  GoogleUrl base_url("http://example.com/");
  DummyTransformer backup_trans;
  NullMessageHandler handler;
  AssociationTransformer trans(&base_url, &backup_trans, &handler);

  // Transform fails because there is an invalid URL.
  GoogleString out;
  StringWriter out_writer(&out);
  EXPECT_FALSE(CssTagScanner::TransformUrls(css_before, &out_writer, &trans,
                                            &handler));
}

}  // namespace net_instaweb
