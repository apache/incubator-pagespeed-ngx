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

// Author: matterbury@google.com (Matt Atterbury)

#include "net/instaweb/rewriter/public/css_hierarchy.h"

#include <algorithm>

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/css_minify.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/url_input_resource.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/string_util.h"
#include "webutil/css/parser.h"

namespace net_instaweb {

class MessageHandler;

namespace {

static const char kTestDomain[] = "http://test.com/";

// The @import hierarchy is:
// Top
//  +- TopChild1
//      +- TopChild1Child1
//  +- TopChild2
//      +- TopChild2Child1
static const char kTopCss[] =
    ".background_red{background-color:red}"
    ".foreground_yellow{color:#ff0}";
static const char kTopChild1Css[] =
    ".background_blue{background-color:#00f}"
    ".foreground_gray{color:gray}";
static const char kTopChild1Child1Css[] =
    ".background_cyan{background-color:#0ff}"
    ".foreground_pink{color:#ffc0cb}";
static const char kTopChild2Css[] =
    ".background_white{background-color:#fff}"
    ".foreground_black{color:#000}";
static const char kTopChild2Child1Css[] =
    ".background_green{background-color:#0f0}"
    ".foreground_rose{color:rose}";

}  // namespace

class CssHierarchyTest : public ::testing::Test {
 protected:
  CssHierarchyTest()
      : top_url_(kTestDomain),
        top_child1_url_(top_url_, "nested1.css"),
        top_child2_url_(top_url_, "nested2.css"),
        top_child1_child1_url_(top_url_, "nested/nested1.css"),
        top_child2_child1_url_(top_url_, "nested/nested2.css") {
  }

  // Initialize our CSS contents with the given, optional, media.
  void InitializeCss(const StringPiece top_media,
                     const StringPiece child_media);

  // Initialize a flat root - top-level CSS with no @imports.
  void InitializeFlatRoot(CssHierarchy* top) {
    InitializeCss("", "");
    top->InitializeRoot(top_url_, top_url_, flat_top_css_,
                        false /* is_xhtml */, NULL /* stylesheet */,
                        message_handler());
  }

  // Initialize a nested root - top-level CSS with @imports.
  void InitializeNestedRoot(CssHierarchy* top) {
    InitializeCss("", "");
    top->InitializeRoot(top_url_, top_url_, nested_top_css_,
                        false /* is_xhtml */, NULL /* stylesheet */,
                        message_handler());
  }

  // Initialize a nested root with the given media.
  void InitializeNestedRootWithMedia(CssHierarchy* top,
                                     const StringPiece top_media,
                                     const StringPiece child_media) {
    InitializeCss(top_media, child_media);
    top->InitializeRoot(top_url_, top_url_, nested_top_css_,
                        false /* is_xhtml */, NULL /* stylesheet */,
                        message_handler());
  }

  // Expand the hierarchy using ExpandChildren. Expands the top then adds
  // each child's contents and expands it, and so on for entire hierarchy.
  void ExpandHierarchy(CssHierarchy* top);

  // Create the given number of children under the given hierarchy.
  void ResizeChildren(CssHierarchy* top, int n) {
    top->children().resize(n);
    for (int i = 0; i < n; ++i) {
      top->children()[i] = new CssHierarchy();
    }
  }

  // This version populates the hierarchy manually, deliberately NOT using
  // ExpandChildren to ensure it ends up as we expect so that we can then
  // compare against and so test ExpandChildren.
  void PopulateHierarchy(CssHierarchy* top);

  // Are these two instances equivalent? Shallow comparison only: does not
  // check parent and only checks that they have the same number of children.
  bool AreEquivalent(const CssHierarchy& one, const CssHierarchy& two);

  GoogleString MakeAtImport(StringPiece url, StringPiece media) {
    return StrCat("@import url(", url, ") ", media, ";");
  }

  MessageHandler* message_handler() { return &handler_; }

  const GoogleUrl& top_url() const { return top_url_; }
  const GoogleString& flat_top_css() const { return flat_top_css_; }
  const GoogleString& nested_top_css() const { return nested_top_css_; }
  const GoogleString& nested_child1_css() const { return nested_child1_css_; }
  const GoogleString& nested_child2_css() const { return nested_child2_css_; }
  const GoogleString& flattened_css() const { return flattened_css_; }

 private:
  MockMessageHandler handler_;
  GoogleUrl top_url_;
  GoogleUrl top_child1_url_;
  GoogleUrl top_child2_url_;
  GoogleUrl top_child1_child1_url_;
  GoogleUrl top_child2_child1_url_;
  GoogleString flat_top_css_;    // top-level without any @imports.
  GoogleString nested_top_css_;  // top-level with @imports.
  GoogleString nested_child1_css_;
  GoogleString nested_child2_css_;
  GoogleString flattened_css_;   // Flattened version of the entire hierarchy.

  DISALLOW_COPY_AND_ASSIGN(CssHierarchyTest);
};

void CssHierarchyTest::InitializeCss(const StringPiece top_media,
                                     const StringPiece child_media) {
  if (flat_top_css_.empty()) {
    flat_top_css_ = kTopCss;
    nested_top_css_ = StrCat(
        MakeAtImport(top_child1_url_.Spec(), top_media),
        MakeAtImport(top_child2_url_.Spec(), top_media),
        kTopCss);
    nested_child1_css_ = StrCat(
        MakeAtImport(top_child1_child1_url_.Spec(), child_media),
        kTopChild1Css);
    nested_child2_css_ = StrCat(
        MakeAtImport(top_child2_child1_url_.Spec(), child_media),
        kTopChild2Css);
    flattened_css_ = StrCat(kTopChild1Child1Css, kTopChild1Css,
                            kTopChild2Child1Css, kTopChild2Css,
                            kTopCss);
  }
}


void CssHierarchyTest::ExpandHierarchy(CssHierarchy* top) {
  EXPECT_TRUE(top->Parse());
  EXPECT_TRUE(top->ExpandChildren());

  GoogleString child_contents[] = {
    nested_child1_css_,
    nested_child2_css_
  };
  GoogleString grandchild_contents[] = {
    kTopChild1Child1Css,
    kTopChild2Child1Css
  };

  for (int i = 0, n = top->children().size(); i < n && i < 2; ++i) {
    CssHierarchy* child = top->children()[i];
    if (child->NeedsRewriting()) {
      child->set_input_contents(child_contents[i]);
      EXPECT_TRUE(child->Parse());
      child->ExpandChildren();

      if (child->children().size() > 0 &&
          child->children()[0]->NeedsRewriting()) {
        CssHierarchy* grandchild = child->children()[0];
        grandchild->set_input_contents(grandchild_contents[i]);
        EXPECT_TRUE(grandchild->Parse());
        EXPECT_FALSE(grandchild->ExpandChildren());
      }
    }
  }
}

void CssHierarchyTest::PopulateHierarchy(CssHierarchy* top) {
  ResizeChildren(top, 2);

  CssHierarchy* top_child1 = top->children()[0];
  top_child1->InitializeNested(*top, top_child1_url_);
  top_child1->set_input_contents(nested_child1_css_);
  ResizeChildren(top_child1, 1);

  CssHierarchy* top_child2 = top->children()[1];
  top_child2->InitializeNested(*top, top_child2_url_);
  top_child2->set_input_contents(nested_child2_css_);
  ResizeChildren(top_child2, 1);

  CssHierarchy* top_child1_child1 = top_child1->children()[0];
  top_child1_child1->InitializeNested(*top_child1, top_child1_child1_url_);
  top_child1_child1->set_input_contents(kTopChild1Child1Css);

  CssHierarchy* top_child2_child1 = top_child2->children()[0];
  top_child2_child1->InitializeNested(*top_child2, top_child2_child1_url_);
  top_child2_child1->set_input_contents(kTopChild2Child1Css);
}

bool CssHierarchyTest::AreEquivalent(const CssHierarchy& one,
                                     const CssHierarchy& two) {
  if (one.url() != two.url()) {
    return false;
  }
  if (one.css_base_url() != two.css_base_url()) {
    return false;
  }
  if (one.css_trim_url() != two.css_trim_url()) {
    return false;
  }
  if (one.children().size() != two.children().size()) {
    return false;
  }
  if (one.input_contents() != two.input_contents()) {
    return false;
  }
  if (one.minified_contents() != two.minified_contents()) {
    return false;
  }
  if (one.charset() != two.charset()) {
    return false;
  }
  if (one.flattening_succeeded() != two.flattening_succeeded()) {
    return false;
  }
  // It would be nice to check parent_ but it's private so skip it.

  // Sigh. We need to check the stylesheet data manually.
  const Css::Stylesheet* stylesheet_one = one.stylesheet();
  const Css::Stylesheet* stylesheet_two = two.stylesheet();
  if ((stylesheet_one == NULL && stylesheet_two != NULL) ||
      (stylesheet_one != NULL && stylesheet_two == NULL)) {
    return false;
  }
  if (stylesheet_one != NULL && stylesheet_two != NULL) {
    // The easiest way to compare two stylesheets is to textify them and
    // compare the texts. Not inefficient but simple and effective. If either
    // textification fails though we give up and treat the as different.
    GoogleString text_one;
    StringWriter writer_one(&text_one);
    if (!CssMinify::Stylesheet(*stylesheet_one, &writer_one, &handler_)) {
      return false;
    }
    GoogleString text_two;
    StringWriter writer_two(&text_two);
    if (!CssMinify::Stylesheet(*stylesheet_two, &writer_two, &handler_)) {
      return false;
    }
    if (text_one != text_two) {
      return false;
    }
  }
  // And the same for the media though it's much easier.
  const StringVector& media_one = one.media();
  const StringVector& media_two = two.media();
  if (media_one.size() != media_two.size() ||
      !std::equal(media_one.begin(), media_one.end(), media_two.begin())) {
    return false;
  }
  return true;
}

TEST_F(CssHierarchyTest, ParseFlat) {
  CssHierarchy top;

  InitializeFlatRoot(&top);
  EXPECT_EQ("", top.minified_contents());
  EXPECT_TRUE(NULL == top.stylesheet());

  EXPECT_TRUE(top.Parse());
  EXPECT_EQ("", top.minified_contents());
  EXPECT_TRUE(NULL != top.stylesheet());
  EXPECT_TRUE(top.stylesheet()->imports().empty());
}

TEST_F(CssHierarchyTest, ExpandFlat) {
  CssHierarchy top;

  InitializeFlatRoot(&top);
  EXPECT_TRUE(NULL == top.stylesheet());

  EXPECT_TRUE(top.Parse());
  EXPECT_TRUE(NULL != top.stylesheet());
  EXPECT_TRUE(top.stylesheet()->imports().empty());
  EXPECT_TRUE(top.children().empty());

  // No imports to expand => no change in these checks.
  EXPECT_FALSE(top.ExpandChildren());
  EXPECT_TRUE(NULL != top.stylesheet());
  EXPECT_TRUE(top.stylesheet()->imports().empty());
  EXPECT_TRUE(top.children().empty());
}

TEST_F(CssHierarchyTest, RollUpContentsFlat) {
  CssHierarchy top;

  InitializeFlatRoot(&top);
  EXPECT_EQ("", top.minified_contents());
  EXPECT_TRUE(NULL == top.stylesheet());

  top.RollUpContents();
  EXPECT_EQ(flat_top_css(), top.minified_contents());
  EXPECT_TRUE(NULL != top.stylesheet());
}

TEST_F(CssHierarchyTest, RollUpStylesheetsFlat) {
  CssHierarchy top;

  InitializeFlatRoot(&top);
  EXPECT_EQ("", top.minified_contents());
  EXPECT_TRUE(NULL == top.stylesheet());

  top.RollUpStylesheets();
  EXPECT_EQ("", top.minified_contents());
  EXPECT_TRUE(NULL != top.stylesheet());
  EXPECT_TRUE(top.stylesheet()->imports().empty());

  // Re-serialize stylesheet and check it matches.
  GoogleString out_text;
  StringWriter writer(&out_text);
  CssMinify::Stylesheet(*top.stylesheet(), &writer, message_handler());
  EXPECT_EQ(flat_top_css(), out_text);
}

TEST_F(CssHierarchyTest, ParseNested) {
  CssHierarchy top;

  InitializeNestedRoot(&top);
  ExpandHierarchy(&top);
  EXPECT_EQ("", top.minified_contents());
  EXPECT_TRUE(NULL != top.stylesheet());
  EXPECT_EQ("", top.minified_contents());
  EXPECT_EQ(2, top.stylesheet()->imports().size());
}

TEST_F(CssHierarchyTest, ExpandNested) {
  CssHierarchy top;

  InitializeNestedRoot(&top);
  ExpandHierarchy(&top);

  EXPECT_TRUE(NULL != top.stylesheet());
  EXPECT_EQ(2, top.stylesheet()->imports().size());
  EXPECT_EQ(2, top.children().size());

  for (int i = 0, n = top.children().size(); i < n; ++i) {
    CssHierarchy* child = top.children()[i];
    EXPECT_TRUE(NULL != child->stylesheet());
    EXPECT_EQ(1, child->stylesheet()->imports().size());
    EXPECT_EQ(1, child->children().size());

    CssHierarchy* grandchild = child->children()[0];
    EXPECT_TRUE(NULL != grandchild->stylesheet());
    EXPECT_TRUE(grandchild->stylesheet()->imports().empty());
    EXPECT_TRUE(grandchild->children().empty());
  }
}

TEST_F(CssHierarchyTest, ExpandEqualsPopulate) {
  CssHierarchy top1;
  CssHierarchy top2;

  InitializeNestedRoot(&top1);
  ExpandHierarchy(&top1);

  InitializeNestedRoot(&top2);
  PopulateHierarchy(&top2);

  // Since PopulateHierarchy doesn't parse the stylesheets, do it here so
  // that the comparisons are fair.
  EXPECT_TRUE(top2.Parse());
  EXPECT_TRUE(top2.children()[0]->Parse());
  EXPECT_TRUE(top2.children()[1]->Parse());
  EXPECT_TRUE(top2.children()[0]->children()[0]->Parse());
  EXPECT_TRUE(top2.children()[1]->children()[0]->Parse());

  EXPECT_TRUE(AreEquivalent(top1, top2));
}

TEST_F(CssHierarchyTest, FailOnDirectRecursion) {
  InitializeCss("", "");  // to initialize top_url().

  CssHierarchy top;
  GoogleString recursive_import = StrCat("@import '", top_url().Spec(), "' ;");
  top.InitializeRoot(top_url(), top_url(), recursive_import,
                     false /* is_xhtml */, NULL /* stylesheet */,
                     message_handler());

  // The top-level normally doesn't have an URL so we won't catch it recursing
  // until the grandchild level, but we -do- catch it, eventually.
  EXPECT_TRUE(top.Parse());
  EXPECT_TRUE(top.ExpandChildren());
  EXPECT_TRUE(top.flattening_succeeded());
  EXPECT_EQ(1, top.children().size());

  CssHierarchy* child = top.children()[0];
  child->set_input_contents(recursive_import);
  EXPECT_TRUE(child->NeedsRewriting());
  EXPECT_TRUE(child->Parse());
  EXPECT_FALSE(child->ExpandChildren());
  EXPECT_TRUE(child->flattening_succeeded());
  EXPECT_EQ(1, child->children().size());

  // THIS is the one who's flattening has failed, at last.
  CssHierarchy* grandchild = child->children()[0];
  EXPECT_FALSE(grandchild->flattening_succeeded());
}

TEST_F(CssHierarchyTest, FailOnIndirectRecursion) {
  CssHierarchy top;

  InitializeNestedRoot(&top);

  // Manually expand the hierarchy so we can introduce recursion.
  EXPECT_TRUE(top.Parse());
  EXPECT_TRUE(top.ExpandChildren());
  EXPECT_TRUE(top.flattening_succeeded());

  CssHierarchy* child1 = top.children()[0];
  child1->set_input_contents(nested_child1_css());
  EXPECT_TRUE(child1->Parse());
  EXPECT_TRUE(child1->ExpandChildren());
  EXPECT_TRUE(child1->flattening_succeeded());

  CssHierarchy* child2 = top.children()[1];
  child2->set_input_contents(nested_child2_css());
  EXPECT_TRUE(child2->Parse());
  EXPECT_TRUE(child2->ExpandChildren());
  EXPECT_TRUE(child2->flattening_succeeded());

  CssHierarchy* grandchild1 = child1->children()[0];
  grandchild1->set_input_contents(kTopChild1Child1Css);
  EXPECT_TRUE(grandchild1->Parse());
  EXPECT_FALSE(grandchild1->ExpandChildren());
  EXPECT_TRUE(grandchild1->flattening_succeeded());

  CssHierarchy* grandchild2 = child2->children()[0];
  grandchild2->set_input_contents(nested_top_css());  // Same as root so ...
  EXPECT_TRUE(grandchild2->Parse());
  EXPECT_TRUE(grandchild2->ExpandChildren());
  EXPECT_EQ(2, grandchild2->children().size());
  CssHierarchy* greatgrandchild2 = grandchild2->children()[1];
  EXPECT_FALSE(greatgrandchild2->flattening_succeeded());  // ... should fail.
}

TEST_F(CssHierarchyTest, ExpandElidesImportsWithNoMedia) {
  CssHierarchy top;

  InitializeNestedRootWithMedia(&top, "screen", "print");
  ExpandHierarchy(&top);

  EXPECT_TRUE(NULL != top.stylesheet());
  EXPECT_EQ(2, top.stylesheet()->imports().size());
  EXPECT_EQ(2, top.children().size());

  for (int i = 0, n = top.children().size(); i < n; ++i) {
    CssHierarchy* child = top.children()[i];
    EXPECT_TRUE(NULL != child->stylesheet());
    EXPECT_EQ(1, child->stylesheet()->imports().size());
    EXPECT_EQ(1, child->children().size());

    CssHierarchy* grandchild = child->children()[0];
    EXPECT_TRUE(NULL == grandchild->stylesheet());
    EXPECT_TRUE(grandchild->children().empty());
    EXPECT_FALSE(grandchild->NeedsRewriting());
  }

  top.RollUpContents();
  GoogleString flattened_css = StrCat(
      StrCat("@media screen{", kTopChild1Css, "}"),
      StrCat("@media screen{", kTopChild2Css, "}"),
      kTopCss);
  EXPECT_EQ(flattened_css, top.minified_contents());
}

TEST_F(CssHierarchyTest, CompatibleCharset) {
  CssHierarchy top;

  InitializeNestedRoot(&top);
  ExpandHierarchy(&top);

  // Construct a resource without a charset.
  RewriteOptions options;
  ResourcePtr resource(new UrlInputResource(NULL, &options, &kContentTypeCss,
                                            top_url().Spec()));
  ResponseHeaders* response_headers = resource->response_headers();

  // First check that with no charsets anywhere we match.
  CssHierarchy* child = top.children()[0];
  EXPECT_TRUE(child->CheckCharsetOk(resource));

  // Now set both the charsets to something compatible.
  StringPiece charset("iso-8859-1");
  response_headers->MergeContentType(StrCat(kContentTypeCss.mime_type(),
                                            "; charset=", charset));
  charset.CopyToString(top.mutable_charset());
  EXPECT_TRUE(child->CheckCharsetOk(resource));
  EXPECT_EQ(charset, child->charset());
}

TEST_F(CssHierarchyTest, IncompatibleCharset) {
  CssHierarchy top;

  InitializeNestedRoot(&top);
  ExpandHierarchy(&top);

  // Construct a resource with an incompatible charset.
  RewriteOptions options;
  ResourcePtr resource(new UrlInputResource(NULL, &options, &kContentTypeCss,
                                            top_url().Spec()));
  ResponseHeaders* response_headers = resource->response_headers();
  response_headers->MergeContentType(StrCat(kContentTypeCss.mime_type(),
                                            "; charset=utf-8"));

  StringPiece charset("iso-8859-1");
  charset.CopyToString(top.mutable_charset());
  CssHierarchy* child = top.children()[0];
  EXPECT_FALSE(child->CheckCharsetOk(resource));
  EXPECT_EQ("utf-8", child->charset());
}

TEST_F(CssHierarchyTest, RollUpContentsNested) {
  CssHierarchy top;

  InitializeNestedRoot(&top);
  ExpandHierarchy(&top);
  EXPECT_EQ("", top.minified_contents());
  EXPECT_TRUE(NULL != top.stylesheet());

  top.RollUpContents();
  EXPECT_EQ(flattened_css(), top.minified_contents());
}

TEST_F(CssHierarchyTest, RollUpStylesheetsNested) {
  CssHierarchy top;

  InitializeNestedRoot(&top);
  ExpandHierarchy(&top);
  EXPECT_EQ("", top.minified_contents());
  EXPECT_TRUE(NULL != top.stylesheet());

  top.RollUpStylesheets();
  EXPECT_EQ("", top.minified_contents());
  EXPECT_TRUE(NULL != top.stylesheet());
  EXPECT_TRUE(top.stylesheet()->imports().empty());

  // Re-serialize stylesheet and check it matches.
  GoogleString out_text;
  StringWriter writer(&out_text);
  CssMinify::Stylesheet(*top.stylesheet(), &writer, message_handler());
  EXPECT_EQ(flattened_css(), out_text);
}

TEST_F(CssHierarchyTest, RollUpStylesheetsNestedWithoutRollUpContents) {
  CssHierarchy top;

  InitializeNestedRoot(&top);
  PopulateHierarchy(&top);  // ExpandHierarchy does too much.
  EXPECT_EQ("", top.minified_contents());
  EXPECT_TRUE(NULL == top.stylesheet());

  top.RollUpStylesheets();
  EXPECT_EQ("", top.minified_contents());
  EXPECT_EQ(2, top.stylesheet()->imports().size());  // 2 => unflattened => bad.

  // Re-serialize stylesheet and check it matches.
  GoogleString out_text;
  StringWriter writer(&out_text);
  CssMinify::Stylesheet(*top.stylesheet(), &writer, message_handler());
  EXPECT_EQ(nested_top_css(), out_text);  // unchanged => unflattened => bad
}

TEST_F(CssHierarchyTest, RollUpStylesheetsNestedWithChildrenRollUpContents) {
  CssHierarchy top;

  InitializeNestedRoot(&top);
  PopulateHierarchy(&top);  // ExpandHierarchy does too much.
  EXPECT_EQ("", top.minified_contents());
  EXPECT_TRUE(NULL == top.stylesheet());

  // Per the contract, make sure our CSS is already parsed.
  EXPECT_TRUE(top.Parse());

  // Roll up all the children's contents manually. This is the contract so
  // we test that here. Later we roll up our own contents and test that case.
  for (int i = 0, n = top.children().size(); i < n; ++i) {
    top.children()[i]->RollUpContents();
  }

  top.RollUpStylesheets();
  EXPECT_EQ("", top.minified_contents());
  EXPECT_TRUE(NULL != top.stylesheet());
  EXPECT_TRUE(top.stylesheet()->imports().empty());

  // Re-serialize stylesheet and check it matches.
  GoogleString out_text;
  StringWriter writer(&out_text);
  CssMinify::Stylesheet(*top.stylesheet(), &writer, message_handler());
  EXPECT_EQ(flattened_css(), out_text);
}

TEST_F(CssHierarchyTest, RollUpStylesheetsNestedAfterRollUpContents) {
  CssHierarchy top;

  InitializeNestedRoot(&top);
  PopulateHierarchy(&top);  // ExpandHierarchy does too much.
  EXPECT_EQ("", top.minified_contents());
  EXPECT_TRUE(NULL == top.stylesheet());

  // Roll up our own contents which should manually roll-up all our children's
  // thereby meeting the contract for RollUpStylesheets(). This implicitly
  // parses our CSS so no need to do it explicitly.
  top.RollUpContents();
  EXPECT_EQ(flattened_css(), top.minified_contents());

  top.RollUpStylesheets();
  EXPECT_TRUE(NULL != top.stylesheet());
  EXPECT_TRUE(top.stylesheet()->imports().empty());

  // Re-serialize stylesheet and check it matches.
  GoogleString out_text;
  StringWriter writer(&out_text);
  CssMinify::Stylesheet(*top.stylesheet(), &writer, message_handler());
  EXPECT_EQ(flattened_css(), out_text);
}

}  // namespace net_instaweb
