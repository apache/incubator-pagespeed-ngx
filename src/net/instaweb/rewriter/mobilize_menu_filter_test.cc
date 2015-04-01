/*
 * Copyright 2015 Google Inc.
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

#include "net/instaweb/rewriter/public/mobilize_menu_filter.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/mobilize_menu.pb.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

namespace {

// Some simple string <==> menu conversion routines to make the testing code
// easier to read and understand.  Simple menu grammar:
//  menu = item*
//  item = "(" [name] [ "," [url] ] [ "|" menu ] ")"

// If *s starts with token, return true and strip following whitespace.
bool ConsumeWithSpaces(char token, StringPiece* s) {
  if (!s->empty() && s->data()[0] == token) {
    s->remove_prefix(1);
    TrimLeadingWhitespace(s);
    return true;
  } else {
    return false;
  }
}

// Find first occurrence of a char in cands in *s, and remove and return the
// segment before that, trimming trailing whitespace from the returned string.
// On exit *s will either be empty or start with a char in cands.
StringPiece SplitUntilFirstOf(StringPiece* s, StringPiece cands) {
  int end = s->find_first_of(cands);
  StringPiece result;
  if (end == StringPiece::npos) {
    result = *s;
    s->clear();
  } else {
    result = s->substr(0, end);
    s->remove_prefix(end);
  }
  TrimTrailingWhitespace(&result);
  return result;
}

void MenuFromString(StringPiece* s, MobilizeMenu* menu);

// Parse a single menu item from *s already stripped of its leading '('
void ItemFromString(StringPiece* s, MobilizeMenuItem* item) {
  StringPiece name = SplitUntilFirstOf(s, ",|)");
  if (!name.empty()) {
    name.CopyToString(item->mutable_name());
  }
  if (ConsumeWithSpaces(',', s)) {
    StringPiece url = SplitUntilFirstOf(s, "|)");
    if (!url.empty()) {
      url.CopyToString(item->mutable_url());
    }
  }
  if (ConsumeWithSpaces('|', s)) {
    MenuFromString(s, item->mutable_submenu());
  }
  ConsumeWithSpaces(')', s);
}

// Parse a sequence of menu items from *s.
void MenuFromString(StringPiece* s, MobilizeMenu* menu) {
  while (ConsumeWithSpaces('(', s)) {
    ItemFromString(s, menu->add_entries());
  }
}

// Parse a menu string s and return the menu.
MobilizeMenu Menu(StringPiece s) {
  TrimWhitespace(&s);
  MobilizeMenu result;
  MenuFromString(&s, &result);
  DCHECK(s.empty()) << s << " left from parse.";
  return result;
}

void AppendMenuToString(const MobilizeMenu& menu, GoogleString* result);

// Serialize a menu item and append it to *result.
void AppendItemToString(const MobilizeMenuItem& item, GoogleString* result) {
  StrAppend(result, "(", item.name());
  if (item.has_url()) {
    StrAppend(result, ", ", item.url());
  }
  if (item.has_submenu()) {
    StrAppend(result, " | ");
    AppendMenuToString(item.submenu(), result);
  }
  StrAppend(result, ")");
}

// Serialize a menu and append it to *result.
void AppendMenuToString(const MobilizeMenu& menu, GoogleString* result) {
  int n = menu.entries_size();
  for (int i = 0; i < n; ++i) {
    if (i != 0) {
      StrAppend(result, " ");
    }
    AppendItemToString(menu.entries(i), result);
  }
}

// Serialize a menu to a string.
GoogleString MenuToString(const MobilizeMenu& menu) {
  GoogleString result;
  AppendMenuToString(menu, &result);
  return result;
}

// We begin by testing menu cleanup (cross-checking serialization and
// deserialization as we go to make sure our test code is working as we expect).
// Cleanup is the bulk of the code complexity in the filter, so it gets the bulk
// of the targeted unit testing.
TEST(CleanupTest, EmptyString) {
  MobilizeMenu result = Menu("   ");
  EXPECT_EQ(0, result.entries_size());
  EXPECT_TRUE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_STREQ("", MenuToString(result));
  MobilizeMenuFilter::CleanupMenu(&result);
  EXPECT_EQ(0, result.entries_size());
  EXPECT_TRUE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_STREQ("", MenuToString(result));
}

TEST(CleanupTest, EmptyItem) {
  MobilizeMenu result = Menu(" () ");
  EXPECT_EQ(1, result.entries_size());
  EXPECT_FALSE(result.entries(0).has_name());
  EXPECT_FALSE(result.entries(0).has_url());
  EXPECT_FALSE(result.entries(0).has_submenu());
  EXPECT_FALSE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_STREQ("()", MenuToString(result));
  MobilizeMenuFilter::CleanupMenu(&result);
  EXPECT_EQ(0, result.entries_size());
  EXPECT_TRUE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_STREQ("", MenuToString(result));
}

TEST(CleanupTest, FullItem) {
  const char kMenuString[] ="(a, a.html | )";
  MobilizeMenu result = Menu(kMenuString);
  EXPECT_EQ(1, result.entries_size());
  EXPECT_EQ("a", result.entries(0).name());
  EXPECT_EQ("a.html", result.entries(0).url());
  ASSERT_TRUE(result.entries(0).has_submenu());
  EXPECT_EQ(0, result.entries(0).submenu().entries_size());
  EXPECT_FALSE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_STREQ(kMenuString, MenuToString(result));
  // Cleanup should get rid of the empty submenu.
  MobilizeMenuFilter::CleanupMenu(&result);
  EXPECT_EQ(1, result.entries_size());
  EXPECT_EQ("a", result.entries(0).name());
  EXPECT_EQ("a.html", result.entries(0).url());
  ASSERT_FALSE(result.entries(0).has_submenu());
  EXPECT_TRUE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_STREQ("(a, a.html)", MenuToString(result));
}

TEST(CleanupTest, JustName) {
  const char kMenuString[] = "(a)";
  MobilizeMenu result = Menu(kMenuString);
  EXPECT_EQ(1, result.entries_size());
  EXPECT_EQ("a", result.entries(0).name());
  EXPECT_FALSE(result.entries(0).has_url());
  EXPECT_FALSE(result.entries(0).has_submenu());
  EXPECT_FALSE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_STREQ(kMenuString, MenuToString(result));
  MobilizeMenuFilter::CleanupMenu(&result);
  EXPECT_EQ(0, result.entries_size());
  EXPECT_TRUE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_STREQ("", MenuToString(result));
}

TEST(CleanupTest, JustUrl) {
  const char kMenuString[] = "(, a.html)";
  MobilizeMenu result = Menu(kMenuString);
  EXPECT_EQ(1, result.entries_size());
  EXPECT_FALSE(result.entries(0).has_name());
  EXPECT_EQ("a.html", result.entries(0).url());
  EXPECT_FALSE(result.entries(0).has_submenu());
  EXPECT_FALSE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_STREQ(kMenuString, MenuToString(result));
  MobilizeMenuFilter::CleanupMenu(&result);
  EXPECT_EQ(0, result.entries_size());
  EXPECT_TRUE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_STREQ("", MenuToString(result));
}

TEST(CleanupTest, JustSubmenu) {
  const char kMenuString[] = "( | (a, a.html) (b, b.html))";
  MobilizeMenu result = Menu(kMenuString);
  EXPECT_EQ(1, result.entries_size());
  EXPECT_FALSE(result.entries(0).has_name());
  EXPECT_FALSE(result.entries(0).has_url());
  ASSERT_TRUE(result.entries(0).has_submenu());
  const MobilizeMenu& submenu = result.entries(0).submenu();
  EXPECT_EQ(2, submenu.entries_size());
  EXPECT_FALSE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_TRUE(MobilizeMenuFilter::IsMenuOk(submenu));
  EXPECT_STREQ(kMenuString, MenuToString(result));
  // The lone, untitled submenu should be flattened.
  MobilizeMenuFilter::CleanupMenu(&result);
  ASSERT_EQ(2, result.entries_size());
  EXPECT_EQ("a", result.entries(0).name());
  EXPECT_EQ("a.html", result.entries(0).url());
  EXPECT_FALSE(result.entries(0).has_submenu());
  EXPECT_EQ("b", result.entries(1).name());
  EXPECT_EQ("b.html", result.entries(1).url());
  EXPECT_FALSE(result.entries(1).has_submenu());
  EXPECT_TRUE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_STREQ("(a, a.html) (b, b.html)", MenuToString(result));
}

TEST(CleanupTest, NameUrl) {
  const char kMenuString[] = "(a, a.html)";
  MobilizeMenu result = Menu(kMenuString);
  EXPECT_EQ(1, result.entries_size());
  EXPECT_EQ("a", result.entries(0).name());
  EXPECT_EQ("a.html", result.entries(0).url());
  EXPECT_FALSE(result.entries(0).has_submenu());
  EXPECT_TRUE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_STREQ(kMenuString, MenuToString(result));
  MobilizeMenuFilter::CleanupMenu(&result);
  EXPECT_EQ(1, result.entries_size());
  EXPECT_TRUE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_STREQ(kMenuString, MenuToString(result));
}

TEST(CleanupTest, NameMenu) {
  const char kMenuString[] = "(a | (b, b.html) (c, c.html))";
  MobilizeMenu result = Menu(kMenuString);
  EXPECT_EQ(1, result.entries_size());
  EXPECT_EQ("a", result.entries(0).name());
  EXPECT_FALSE(result.entries(0).has_url());
  ASSERT_TRUE(result.entries(0).has_submenu());
  const MobilizeMenu& submenu = result.entries(0).submenu();
  EXPECT_EQ(2, submenu.entries_size());
  EXPECT_TRUE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_TRUE(MobilizeMenuFilter::IsMenuOk(submenu));
  EXPECT_STREQ(kMenuString, MenuToString(result));
  // The lone titled submenu should be flattened.
  MobilizeMenuFilter::CleanupMenu(&result);
  ASSERT_EQ(2, result.entries_size());
  EXPECT_EQ("b", result.entries(0).name());
  EXPECT_EQ("b.html", result.entries(0).url());
  EXPECT_FALSE(result.entries(0).has_submenu());
  EXPECT_EQ("c", result.entries(1).name());
  EXPECT_EQ("c.html", result.entries(1).url());
  EXPECT_FALSE(result.entries(1).has_submenu());
  EXPECT_TRUE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_STREQ("(b, b.html) (c, c.html)", MenuToString(result));
}

TEST(CleanupTest, UrlMenu) {
  const char kMenuString[] = "(, a.html | (b, b.html) (c, c.html))";
  MobilizeMenu result = Menu(kMenuString);
  EXPECT_EQ(1, result.entries_size());
  EXPECT_FALSE(result.entries(0).has_name());
  EXPECT_EQ("a.html", result.entries(0).url());
  ASSERT_TRUE(result.entries(0).has_submenu());
  const MobilizeMenu& submenu = result.entries(0).submenu();
  EXPECT_EQ(2, submenu.entries_size());
  EXPECT_FALSE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_TRUE(MobilizeMenuFilter::IsMenuOk(submenu));
  EXPECT_STREQ(kMenuString, MenuToString(result));
  // The unlabeled url should be discarded and the submenu flattened.
  MobilizeMenuFilter::CleanupMenu(&result);
  ASSERT_EQ(2, result.entries_size());
  EXPECT_EQ("b", result.entries(0).name());
  EXPECT_EQ("b.html", result.entries(0).url());
  EXPECT_FALSE(result.entries(0).has_submenu());
  EXPECT_EQ("c", result.entries(1).name());
  EXPECT_EQ("c.html", result.entries(1).url());
  EXPECT_FALSE(result.entries(1).has_submenu());
  EXPECT_TRUE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_STREQ("(b, b.html) (c, c.html)", MenuToString(result));
}

TEST(CleanupTest, FullItemWithSubmenu) {
  const char kMenuString[] = "(a, a.html | (b, b.html) (c, c.html))";
  MobilizeMenu result = Menu(kMenuString);
  EXPECT_EQ(1, result.entries_size());
  EXPECT_EQ("a", result.entries(0).name());
  EXPECT_EQ("a.html", result.entries(0).url());
  ASSERT_TRUE(result.entries(0).has_submenu());
  EXPECT_EQ(2, result.entries(0).submenu().entries_size());
  EXPECT_FALSE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_STREQ(kMenuString, MenuToString(result));
  // The name and url on the menu should be discarded and the submenu flattened.
  // This is really a fail safe, as this shouldn't happen in HTML.
  MobilizeMenuFilter::CleanupMenu(&result);
  ASSERT_EQ(2, result.entries_size());
  EXPECT_EQ("b", result.entries(0).name());
  EXPECT_EQ("b.html", result.entries(0).url());
  EXPECT_FALSE(result.entries(0).has_submenu());
  EXPECT_EQ("c", result.entries(1).name());
  EXPECT_EQ("c.html", result.entries(1).url());
  EXPECT_FALSE(result.entries(1).has_submenu());
  EXPECT_TRUE(MobilizeMenuFilter::IsMenuOk(result));
  EXPECT_STREQ("(b, b.html) (c, c.html)", MenuToString(result));
}

TEST(CleanupTest, MultipleEntries) {
  const char kMenuString[] = "(a, a.html) (b) (, c.html) (d, d.html)";
  MobilizeMenu result = Menu(kMenuString);
  EXPECT_EQ(4, result.entries_size());
  EXPECT_STREQ(kMenuString, MenuToString(result));
  MobilizeMenuFilter::CleanupMenu(&result);
  EXPECT_EQ(2, result.entries_size());
  EXPECT_STREQ("(a, a.html) (d, d.html)", MenuToString(result));
}

TEST(CleanupTest, DeeplyNestedSingletons) {
  const char kMenuString[] = "(a | (, b.html | (c, c.html)))";
  MobilizeMenu result = Menu(kMenuString);
  EXPECT_STREQ(kMenuString, MenuToString(result));
  MobilizeMenuFilter::CleanupMenu(&result);
  EXPECT_STREQ("(c, c.html)", MenuToString(result));
}

TEST(CleanupTest, DeeplyNestedEmpty) {
  // Test both an empty nested menu, and an empty entry.
  const char kMenuString[] = "(a | (, b.html | ( | ))) (c | (d | ()))";
  MobilizeMenu result = Menu(kMenuString);
  EXPECT_STREQ(kMenuString, MenuToString(result));
  MobilizeMenuFilter::CleanupMenu(&result);
  EXPECT_STREQ("", MenuToString(result));
}

TEST(CleanupTest, DuplicateRemoval) {
  const char kMenuString[] =
      "(a, a.html) (z, a.html) (y, c.html) "
      "(b | (c, c.html) (d, d.html) (e | (x, c.html) (f, f.html)))";
  const char kExpected[] =
      "(a, a.html) (b | (c, c.html) (d, d.html) (f, f.html))";
  MobilizeMenu result = Menu(kMenuString);
  EXPECT_STREQ(kMenuString, MenuToString(result));
  MobilizeMenuFilter::CleanupMenu(&result);
  EXPECT_STREQ(kExpected, MenuToString(result));
}

// Now test the filter a whole, feeding it HTML and examining the un-cleaned-up
// and cleaned-up results to make sure they're what we would expect.  The
// ActualMenu tests are based on real examples from the wild and point to
// interesting issues with extraction and simplification.
class MobilizeMenuFilterTest : public RewriteTestBase {
 protected:
  MobilizeMenuFilterTest() {}

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    options()->set_mob_always(true);
    mobilize_menu_filter_.reset(new MobilizeMenuFilter(rewrite_driver()));
    html_parse()->AddFilter(mobilize_menu_filter_.get());
  }

  void DoNotCleanup() {
    mobilize_menu_filter_->set_cleanup_menu(false);
  }

  GoogleString MenuString() {
    return MenuToString(mobilize_menu_filter_->menu());
  }

  GoogleString CleanupMenu() {
    MobilizeMenu menu = mobilize_menu_filter_->menu();
    MobilizeMenuFilter::CleanupMenu(&menu);
    return MenuToString(menu);
  }

  scoped_ptr<MobilizeMenuFilter> mobilize_menu_filter_;
};

TEST_F(MobilizeMenuFilterTest, NoNav) {
  DoNotCleanup();
  const char kHtml[] =
      "<body>\n"
      "<nav>Not marked as navigational by labeler</nav>\n"
      "<p>This page has no pagespeed-mobile-role annotations\n"
      "</body>";
  ValidateNoChanges("No nav", kHtml);
  EXPECT_STREQ("", MenuString());
}

const char kActualMenu1[] =
    "<body>"
    "<nav data-mobile-role=navigational>"
    "<a href='/'><img src='logo.jpg'></a>"
    "<ul>"
    // Because the menu titles are themselves links, we end up flattening the
    // submenus.  One thing to consider is whether to instead have a menu titled
    // "Camel" here with a first (or last) entry that points to "Camel Care".
    // Not sure what to call that entry, though.
    " <li><a href='/de/dec'><span>Camel <b></b></span> <p>Camel Call</p> </a>"
    "  <ul>"
    "   <hr>"
    "   <li><a href='/a'>Dromedary</a></li>"
    "   <li><a href='/b/de'><span>Dromedary Brown</span> Camel</a></li>"
    "   <li><a href='/f/de'><span>Dromedary Flight</span> Camel</a></li>"
    "  </ul>"
    " </li>"
    " <li><a href='/m/dm'><span>Paperclip <b></b></span>"
    "                     <p>Paperclip Call</p> </a>"
    "  <ul>"
    "   <li><a href='/derc'>Dromedary Mark Call Waffle</a></li>"
    "   <hr>"
    "   <li><a href='/b/re'><span>Brown</span> Waffle</a></li>"
    "   <li><a href='/f/re'><span>Flight</span> Waffle</a></li>"
    "  </ul>"
    " </li>"
    " <li><a href='/faq'><span>FAQ</span> <p>Question?</p></a></li>"
    " <li><a href='/ph'><p>Question? Call Now</p>"
    "                   <span>800-555-1212</span></a></li>"
    "</ul>"
    "</nav>"
    "</body>";

TEST_F(MobilizeMenuFilterTest, ActualMenu1) {
  DoNotCleanup();
  ValidateNoChanges("Actual menu 1", kActualMenu1);
  // TODO(jmaessen): Extract hierarchical menu with url for Camel Camel Call and
  // Paperclip Paperclip Call.  Also deal with the repetition across elements
  // somehow.
  EXPECT_STREQ("(, /) "
               "( | (Camel Camel Call, /de/dec) "
                   "( | (Dromedary, /a) "
                       "(Dromedary Brown Camel, /b/de) "
                       "(Dromedary Flight Camel, /f/de)) "
                   "(Paperclip Paperclip Call, /m/dm) "
                   "( | (Dromedary Mark Call Waffle, /derc) "
                       "(Brown Waffle, /b/re) "
                       "(Flight Waffle, /f/re)) "
                   "(FAQ Question?, /faq) "
                   "(Question? Call Now 800-555-1212, /ph))", MenuString());
  EXPECT_STREQ("(Camel Camel Call, /de/dec) "
               "(Dromedary, /a) "
               "(Dromedary Brown Camel, /b/de) "
               "(Dromedary Flight Camel, /f/de) "
               "(Paperclip Paperclip Call, /m/dm) "
               "(Dromedary Mark Call Waffle, /derc) "
               "(Brown Waffle, /b/re) "
               "(Flight Waffle, /f/re) "
               "(FAQ Question?, /faq) "
               "(Question? Call Now 800-555-1212, /ph)", CleanupMenu());
}

const char kActualMenu2[] =
    "<body>"
    "<nav data=mobile-role=navigational>"
    "&nbsp;|&nbsp;<a href='l'>Llama</a>"
    "&nbsp;|&nbsp;<a href='a'>Dromedary</a>"
    "&nbsp;|&nbsp;<a href='c'>Call</a>"
    "</nav>"
    "<div data-mobile-role=navigational><div><div>"
    "<ul>"
    "    <li><a href='h'>Homes</a></li>"
    "    <li><a href='a'>Dromedary</a></li>"
    "    <li><a href='s'>Save</a></li>"
    "    <li><a href='f'>Flight</a></li>"
    "    <li><a href='c'>Call&nbsp;</a></li>"
    "</ul>"
    "<div><div>"
    // Note that this search box gets stripped out because we don't retain
    // forms.  We should arguably have a separate method for pulling out search
    // boxes, as this requires rather special treatment (the enclosing form
    // element wasn't even marked).  Note that it's right in the middle of a
    // navigational region.
    "<input type='text' value='Search...'/>"
    "<input type='button' value='Go'/>"
    "</div></div>"
    "<div></div>"
    "</div></div></div>"
    "<div data-mobile-role=navigational>"
    "<div>"
    "  <h6> Giraffe Dromedary </h6>"
    "  <ul>"
    "    <li><a href='s-1'>Dromedary Saddle</a></li>"
    "    <li><a href='s-4'>Dromedaries Salads</a></li>"
    "    <li><a href='s-6'>Bactrian / Eastern</a></li>"
    "  </ul>"
    "</div>"
    "<div>"
    "  <h6> Dromedaries </h6>"
    "  <ul>"
    "    <li><a href='m-10'>Dromedary Saddle</a></li>"
    "    <li><a href='m-18'>Brown</a></li>"
    "  </ul>"
    "</div>"
    "<div>"
    // Unicode characters left here to make sure they get through.
    "<h6> Enter </h6>"
    "  <ul>"
    "    <li><a href='c-4'>Llama Dromedary®</a></li>"
    "    <li><a href='c-1'>Salads®</a></li>"
    "  </ul>"
    "  <ul>"
    "    <li><a href='s-6'>Mark Your Dromedary</a></li>"
    "  </ul>"
    "</div>"
    "</div>"
    "</body>";

TEST_F(MobilizeMenuFilterTest, ActualMenu2) {
  DoNotCleanup();
  ValidateNoChanges("Actual menu 2", kActualMenu2);
  EXPECT_STREQ("( | (Homes, h) "
                   "(Dromedary, a) "
                   "(Save, s) "
                   "(Flight, f) "
                   "(Call&nbsp;, c)) "
               "(Giraffe Dromedary | "
                   "(Dromedary Saddle, s-1) "
                   "(Dromedaries Salads, s-4) "
                   "(Bactrian / Eastern, s-6)) "
               "(Dromedaries | (Dromedary Saddle, m-10) (Brown, m-18)) "
               "(Enter | (Llama Dromedary®, c-4) (Salads®, c-1)) "
               "( | (Mark Your Dromedary, s-6))", MenuString());
  EXPECT_STREQ("(Homes, h) "
               "(Dromedary, a) "
               "(Save, s) "
               "(Flight, f) "
               "(Call&nbsp;, c) "
               "(Giraffe Dromedary | "
                   "(Dromedary Saddle, s-1) "
                   "(Dromedaries Salads, s-4) "
                   "(Bactrian / Eastern, s-6)) "
               "(Dromedaries | (Dromedary Saddle, m-10) (Brown, m-18)) "
               "(Enter | (Llama Dromedary®, c-4) (Salads®, c-1))",
               CleanupMenu());
}

// This third menu is quite a mess coming in.  There are numerous extracted
// navigational regions, because the top menu bar in the page is broken up by
// non-navigational content in the middle of the bar.
//
// The nav regions have a lot of images in them, all of them too large to fit
// comfortably in a touch-style menu.  Luckily each is annotated with text, so
// if we select the version with text we don't lose any information.
const char kActualMenu3[] =
    "<div data-mobile-role=navigational>"
    "<div><p>You can save</p></div>"
    "</div>"
    "<div>"
    "  <ul>"
    "    <li><a href='/m/l/'>Llama</a></li>"
    "  </ul>"
    "</div>"
    "<div data-mobile-role=navigational>"
    "  <ul>"
    "    <li><a href='/r'>Rental Cabin</a></li>"
    "    <li><a href='/d'>Dinner</a></li>"
    "    <li><a href='/p'>Personal</a></li>"
    "    <li><a href='/cs'>Call & Save</a></li>"
    "    <li><a href='/pmp'>Packaging</a></li>"
    "  </ul>"
    "</div>"
    "<div data-mobile-role=navigational>"
    "  <ul>"
    // Each of these would fare best as a submenu, and it'd be nice if the whole
    // business was itself in a submenu (though 3 levels might turn out to be
    // too deep).  Right now they're flattened, again because there's a category
    // link on the parent label.  Actually, there are two, but one is an image
    // and the links are duplicates.
    "<li><a href='/p/'><img src='01.jpg'/></a>"
    "    <span><a href='/p/'>Tour</a></span>"
    "  <ul>"
    "    <li><a href='/p/c/'>Call Personal</a></li>"
    "    <li><a href='/p/v/'>Virtuousity</a></li>"
    "    <li><a href='/p/c/'>Call Personal </a></li>"
    "  </ul></li>"
    "<li><a href='/h/'><img s='02.jpg'/></a>"
    "    <span><a href='/h/'>Homes</a></span>"
    "  <ul>"
    "    <li><a href='/h/w/'>Turf Homes</a></li>"
    "    <li><a href='/h/b/'>Brown Homes</a></li>"
    "    <li><a href='/h/'>Homes</a></li>"
    "  </ul></li>"
    "<li><a href='/twr/'><img src='03.jpg'/></a>"
    "    <span><a href='/twr/'>Tortellini</a></span>"
    "  <ul>"
    "    <li><a href='/bm/'>Broccoli</a></li>"
    "    <li><a href='/pc/d/et/'>Chard</a></li>"
    "    <li><a href='/pc/'>Abandonment</a></li>"
    "  </ul></li>"
    "<li><a href='/p/a/'><img src='04.jpg'/></a>"
    "    <span><a href='/p/a/'>Personal Dromedary</a></span>"
    "  <ul>"
    "    <li><a href='/p/h/'>Dromedary Homes</a></li>"
    "    <li><a href='/p/r/'>Roads</a></li>"
    "    <li><a href='/pc/mg/es/'>Electronica</a></li>"
    "  </ul></li>"
    "<li><a href='/p/m/'><img src='05.jpg'/></a>"
    "    <span><a href='/p/m/'>Mirrors</a></span>"
    "  <ul>"
    "    <li><a href='/p/s/'>Save Personal</a></li>"
    "    <li><a href='/p/c/lr/'>Concave Personal</a></li>"
    "    <li><a href='/p/c/'>Call Personal</a></li>"
    "  </ul></li>"
    "</ul>"
    "</div>"
    // This menu title ends up far too long because we retain all the text.
    // If we kept only the initial span we'd be fine.
    "<div data-mobile-role=navigational>"
    "    <span>Termination Question</span>"
    "    <p>A really long paragraph with lots of text.</p>"
    "  <ul>"
    "    <li><a href='/al'>Short question?</a></li>"
    // Note that we keep this link (2 deep) and discard the duplicate near the
    // top (1 deep).  Doing the reverse makes the menu title a lie, but might
    // otherwise be sensible.
    "    <li><a href='/d'>Long question?</a></li>"
    "    <li><a href='/f'>Even longer question?</a></li>"
    "  </ul>"
    "</div>"
    "<div data-mobile-role=navigational>"
    "  <span>Elephant</span>"
    "  <p><a href='/g/'><img src='04.jpg'/></a>"
    "    Long description </p>"
    "</div>"
    "<div data-mobile-role=navigational>"
    "<div>"
    "  <span>Termination Homes</span>"
    "  <ul>"
    "    <li><a href='/pvl'>"
    "      Buffering <img src='13.jpg'/></a></li>"
    "    <li><a href='/pc'>"
    "       Abandonment <img src='14.jpg'/></a></li>"
    "    <li><a href='/g9d'>"
    "       Execution <img src='15.jpg'/></a></li>"
    "    <li><a href='/h/'>Headache remedies</a></li>"
    "  </ul>"
    "</div>"
    "<div>"
    "  <span>Liberation</span>"
    "  <p><a href='/h/'><img src='16.jpg'/></a>"
    "    Second long description. </div>"
    "</div>"
    "<div data-mobile-role=navigational>"
    "<div>"
    "  <span>Termination Dromedary Homes</span>"
    "  <ul>"
    "    <li><a href='/gsh'>"
    "      Global <img src='09.jpg'/></a></li>"
    "    <li><a href='/pk8h'>"
    "      Apportionment <img src='10.jpg'/></a></li>"
    "    <li><a href='/b6ah'>"
    "      Gorilla <img src='11.jpg'/></a></li>"
    "    <li><a href='/p/h/'>Cotton wool</a></li>"
    "  </ul>"
    "</div>"
    "<div>"
    "  <span>Borderlands</span>"
    "  <p><a href='/p/h/'><img src='12.jpg'/></a>"
    "    Third, really long, description. </div>"
    "</div>"
    "<ul data-mobile-role=navigational>"
    "  <li><a href='/p/c/'>Verdant <strong>plains</strong></a></li>"
    "</ul>"
    "<ul data-mobile-role=navigational>"
    "  <li><a href='/h/'>Verdant <strong>homes</strong></a></li>"
    "</ul>"
    "<ul data-mobile-role=navigational>"
    "  <li><a href='/twr/'>Verdant <strong>mountains</strong></a></li>"
    "</ul>"
    "<ul data-mobile-role=navigational>"
    "  <li><a href='/pc/'>Verdant <strong>coast</strong></a></li>"
    "</ul>";

TEST_F(MobilizeMenuFilterTest, ActualMenu3) {
  DoNotCleanup();
  ValidateNoChanges("Actual menu 3", kActualMenu3);
  EXPECT_STREQ(
      "( | (Rental Cabin, /r) "
          "(Dinner, /d) "
          "(Personal, /p) "
          "(Call & Save, /cs) "
          "(Packaging, /pmp)) "
      "( | (, /p/) (Tour, /p/) "
          "( | (Call Personal, /p/c/) "
              "(Virtuousity, /p/v/) "
              "(Call Personal, /p/c/)) "
          "(, /h/) "
          "(Homes, /h/) "
          "( | (Turf Homes, /h/w/) (Brown Homes, /h/b/) (Homes, /h/)) "
          "(, /twr/) (Tortellini, /twr/) "
          "( | (Broccoli, /bm/) (Chard, /pc/d/et/) (Abandonment, /pc/)) "
          "(, /p/a/) (Personal Dromedary, /p/a/) "
          "( | (Dromedary Homes, /p/h/) "
              "(Roads, /p/r/) "
              "(Electronica, /pc/mg/es/)) "
          "(, /p/m/) (Mirrors, /p/m/) "
          "( | (Save Personal, /p/s/) "
              "(Concave Personal, /p/c/lr/) "
              "(Call Personal, /p/c/))) "
      "(Termination Question A really long paragraph with lots of text. | "
          "(Short question?, /al) "
          "(Long question?, /d) "
          "(Even longer question?, /f)) "
      "(, /g/) "
      "(Termination Homes | "
          "(Buffering, /pvl) "
          "(Abandonment, /pc) "
          "(Execution, /g9d) "
          "(Headache remedies, /h/)) "
      "(, /h/) "
      "(Termination Dromedary Homes | "
          "(Global, /gsh) "
          "(Apportionment, /pk8h) "
          "(Gorilla, /b6ah) "
          "(Cotton wool, /p/h/)) "
      "(, /p/h/) "
      "( | (Verdant plains, /p/c/)) "
      "( | (Verdant homes, /h/)) "
      "( | (Verdant mountains, /twr/)) "
      "( | (Verdant coast, /pc/))",
      MenuString());
  EXPECT_STREQ(
      "(Rental Cabin, /r) "
      "(Personal, /p) "
      "(Call & Save, /cs) "
      "(Packaging, /pmp) "
      "(Tour, /p/) "
      "(Call Personal, /p/c/) "
      "(Virtuousity, /p/v/) "
      "(Turf Homes, /h/w/) "
      "(Brown Homes, /h/b/) "
      "(Tortellini, /twr/) "
      "(Broccoli, /bm/) "
      "(Chard, /pc/d/et/) "
      "(Abandonment, /pc/) "
      "(Personal Dromedary, /p/a/) "
      "(Roads, /p/r/) "
      "(Electronica, /pc/mg/es/) "
      "(Mirrors, /p/m/) "
      "(Save Personal, /p/s/) "
      "(Concave Personal, /p/c/lr/) "
      "(Termination Question A really long paragraph with lots of text. | "
          "(Short question?, /al) "
          "(Long question?, /d) "
          "(Even longer question?, /f)) "
      "(Termination Homes | "
          "(Buffering, /pvl) "
          "(Abandonment, /pc) "
          "(Execution, /g9d) "
          "(Headache remedies, /h/)) "
      "(Termination Dromedary Homes | "
          "(Global, /gsh) "
          "(Apportionment, /pk8h) "
          "(Gorilla, /b6ah) "
          "(Cotton wool, /p/h/))",
      CleanupMenu());
}


}  // namespace

}  // namespace net_instaweb
