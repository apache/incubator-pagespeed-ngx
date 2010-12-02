/**
 * Copyright 2010 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test the html reader/writer to ensure that a few tricky
// constructs come through without corruption.

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/htmlparse/html_event.h"
#include "net/instaweb/htmlparse/html_testing_peer.h"

namespace net_instaweb {

class HtmlParseTest : public HtmlParseTestBase {
  virtual bool AddBody() const { return true; }
};

class HtmlParseTestNoBody : public HtmlParseTestBase {
  virtual bool AddBody() const { return false; }
};

TEST_F(HtmlParseTest, AvoidFalseXmlComment) {
  ValidateNoChanges("avoid_false_xml_comment",
     "<script type=\"text/javascript\">\n"
     "// <!-- this looks like a comment but is not\n"
     "</script>");
}

TEST_F(HtmlParseTest, RetainBogusEndTag) {
  ValidateNoChanges("bogus_end_tag",
     "<script language=\"JavaScript\" type=\"text/javascript\">\n"
     "<!--\n"
     "var s = \"</retain_bogus_end_tag>\";\n"
     "// -->\n"
     "</script>");
}

TEST_F(HtmlParseTest, AmpersandInHref) {
  // Note that we will escape the "&" in the href.
  ValidateNoChanges("ampersand_in_href",
      "<a href=\"http://myhost.com/path?arg1=val1&arg2=val2\">Hello</a>");
}

TEST_F(HtmlParseTest, CorrectTaggify) {
  // Don't turn <2 -> <2>
  ValidateNoChanges("no_taggify_digit", "<p>1<2</p>");
  ValidateNoChanges("no_taggify_unicode", "<p>☃<☕</p>");
  ValidateExpected("taggify_letter", "<p>x<y</p>", "<p>x<y></p>");

  ValidateExpected("taggify_letter+digit", "<p>x1<y2</p>", "<p>x1<y2></p>");
  ValidateExpected("taggify_letter+unicode", "<p>x☃<y☕</p>", "<p>x☃<y☕></p>");

  ValidateNoChanges("no_taggify_digit+letter", "<p>1x<2y</p>");
  ValidateNoChanges("no_taggify_unicode+letter", "<p>☃x<☕y</p>");

  // Found on http://www.taobao.com/
  // Don't turn <1... -> <1...>
  ValidateNoChanges("taobao", "<a>1+1<1母婴全场加1元超值购</a>");
}

TEST_F(HtmlParseTest, BooleanSpaceCloseInTag) {
  ValidateExpected("bool_space_close", "<a b >foo</a>", "<a b>foo</a>");
  ValidateNoChanges("bool_close", "<a b>foo</a>");
  ValidateExpected("space_close_sq", "<a b='c' >foo</a>", "<a b='c'>foo</a>");
  ValidateExpected("space_close_dq",
                   "<a b=\"c\" >foo</a>", "<a b=\"c\">foo</a>");
  ValidateExpected("space_close_nq", "<a b=c >foo</a>", "<a b=c>foo</a>");
  // Distilled from http://www.gougou.com/
  // Unclear exactly what we should do here, maybe leave it as it was without
  // the space?
  ValidateExpected("allow_semicolon",
                   "<a onclick='return m(this)'; >foo</a>",
                   "<a onclick='return m(this)' ;>foo</a>");
}

class AttrValuesSaverFilter : public EmptyHtmlFilter {
 public:
  AttrValuesSaverFilter() { }

  virtual void StartElement(HtmlElement* element) {
    for (int i = 0; i < element->attribute_size(); ++i) {
      value_ += element->attribute(i).value();
    }
  }

  const std::string& value() { return value_; }
  virtual const char* Name() const { return "attr_saver"; }

 private:
  std::string value_;

  DISALLOW_COPY_AND_ASSIGN(AttrValuesSaverFilter);
};

TEST_F(HtmlParseTest, EscapedSingleQuote) {
  AttrValuesSaverFilter attr_saver;
  html_parse_.AddFilter(&attr_saver);
  Parse("escaped_single_quote",
        "<img src='my&#39;single_quoted_image.jpg'/>");
  EXPECT_EQ("my'single_quoted_image.jpg", attr_saver.value());
}

TEST_F(HtmlParseTest, UnclosedQuote) {
  // In this test, the system automatically closes the 'a' tag, which
  // didn't really get closed in the input text.  The exact syntax
  // of the expected results not critical, as long as the parser recovers
  // and does not crash.
  //
  // TODO(jmarantz): test error reporting.
  ValidateNoChanges("unclosed_quote",
     "<div>\n"
     "  <a href=\"http://myhost.com/path?arg1=val1&arg2=val2>Hello</a>\n"
     "</div>\n"
     "<p>next token</p>"
     "</body></html>\n"
     "\"></a></div>");
}

TEST_F(HtmlParseTest, NestedDivInBr) {
  ValidateNoChanges("nested_div_in_br",
     "<br><div>hello</div></br>");
}

// bug 2465145 - Sequential defaulted attribute tags lost
TEST_F(HtmlParseTest, SequentialDefaultedTagsLost) {
  // This test cannot work with libxml, but since we use our own
  // parser we can make it work.  See
  // https://bugzilla.gnome.org/show_bug.cgi?id=611655
  ValidateNoChanges("sequential_defaulted_attribute_tags_lost",
      "<select>\n"
      "  <option value=\"&amp;cat=244\">Other option</option>\n"
      "  <option value selected style=\"color: #ccc;\">Default option"
      "</option>\n"
      "</select>");

  // Illegal attribute "http://www.yahoo.com" mangled by parser into
  // "http:", although if the parser changes how it mangles that somehow
  // it's fine to regold.
  ValidateNoChanges("yahoo",
      "<a href=\"#\" http://www.yahoo.com "
      "class=\"pa-btn-open hide-textindent\">yahoo</a>");

  // Here's another interesting thing from the bug testcase.
  // Specifying a literal "&" without a recognized sequence
  // following it gets parsed correctly by libxml2, and then
  // re-encoded by our writer as &amp;.  That's fine; let's
  // make sure that doesn't change.
  ValidateNoChanges("amp_cat",
      "<option value=\"&cat=244\">other</option>");
}

// bug 2465201 : some html constructs do not need ';' termination.
// Fixed by providing own lexer.
TEST_F(HtmlParseTest, UnterminatedTokens) {
  // the termination semicolons should be added in the output.
  ValidateNoChanges("unterminated_tokens",
      "<p>Look at the non breaking space: \"&nbsp\"</p>");
}

// bug 2467040 : keep ampersands and quotes encoded
TEST_F(HtmlParseTest, EncodeAmpersandsAndQuotes) {
  ValidateNoChanges("ampersands_in_text",
      "<p>This should be a string '&amp;amp;' not a single ampersand.</p>");
  ValidateNoChanges("ampersands_in_values",
      "<img alt=\"This should be a string '&amp;amp;' "
      "not a single ampersand.\"/>");
  ValidateNoChanges("quotes",
      "<p>Clicking <a href=\"javascript: alert(&quot;Alert works!&quot;);\">"
      "here</a> should pop up an alert box.</p>");
}

// bug 2508334 : encoding unicode in general
TEST_F(HtmlParseTest, EncodeUnicode) {
  ValidateNoChanges("unicode_in_text",
      "<p>Non-breaking space: '&nbsp;'</p>\n"
      "<p>Alpha: '&alpha;'</p>\n"
      "<p>Unicode #54321: '&#54321;'</p>\n");
}

TEST_F(HtmlParseTest, ImplicitExplicitClose) {
  // The lexer/printer preserves the input syntax, making it easier
  // to diff inputs & outputs.
  //
  // TODO(jmarantz): But we can have a rewrite pass that eliminates
  // the superfluous "/>".
  ValidateNoChanges("one_brief_one_implicit_input",
      "<input type=\"text\" name=\"username\">"
      "<input type=\"password\" name=\"password\"/>");
}

TEST_F(HtmlParseTest, OpenBracketAfterQuote) {
  // '<' after '"' in attr value
  const char input[] =
      "<input type=\"text\" name=\"username\""
      "<input type=\"password\" name=\"password\"/>";
  const char expected[] =
      "<input type=\"text\" name=\"username\">"  // note added '>'
      "<input type=\"password\" name=\"password\"/>";
  ValidateExpected("open_bracket_after_quote", input, expected);
}

TEST_F(HtmlParseTest, OpenBracketUnquoted) {
  // '<' after after unquoted attr value
  const char input[] =
      "<input type=\"text\" name=username"
      "<input type=\"password\" name=\"password\"/>";
  const char expected[] =
      "<input type=\"text\" name=username>"  // note added '>'
      "<input type=\"password\" name=\"password\"/>";
  ValidateExpected("open_bracket_unquoted", input, expected);
}

TEST_F(HtmlParseTest, OpenBracketAfterEquals) {
  // '<' after after unquoted attr value
  const char input[] =
      "<input type=\"text\" name="
      "<input type=\"password\" name=\"password\"/>";
  const char expected[] =
      "<input type=\"text\" name=>"  // note added '>'
      "<input type=\"password\" name=\"password\"/>";
  ValidateExpected("open_brack_after_equals", input, expected);
}

TEST_F(HtmlParseTest, OpenBracketAfterName) {
  // '<' after after unquoted attr value
  const char input[] =
      "<input type=\"text\" name"
      "<input type=\"password\" name=\"password\"/>";
  const char expected[] =
      "<input type=\"text\" name>"  // note added '>'
      "<input type=\"password\" name=\"password\"/>";
  ValidateExpected("open_brack_after_name", input, expected);
}

TEST_F(HtmlParseTest, OpenBracketAfterSpace) {
  // '<' after after unquoted attr value
  const char input[] =
      "<input type=\"text\" "
      "<input type=\"password\" name=\"password\"/>";
  const char expected[] =
      "<input type=\"text\">"  // note added '>'
      "<input type=\"password\" name=\"password\"/>";
  ValidateExpected("open_brack_after_name", input, expected);
}

// bug 2508140 : <noscript> in <head>
TEST_F(HtmlParseTestNoBody, NoscriptInHead) {
  // Some real websites (ex: google.com) have <noscript> in the <head> even
  // though this is technically illegal acording to the HTML4 spec.
  // We should support the case in use.
  ValidateNoChanges("noscript_in_head",
      "<head><noscript><title>You don't have JS enabled :(</title>"
      "</noscript></head>");
}


// Bool that is auto-initialized to false
class Bool {
 public:
  Bool() : value_(false) {}
  Bool(bool value) : value_(value) {}  // Copy constructor // NOLINT
  const bool Test() const { return value_; }

 private:
  bool value_;
};

// Class simply keeps track of which handlers have been called.
class HandlerCalledFilter : public HtmlFilter {
 public:
  HandlerCalledFilter() { }

  virtual void StartDocument() { called_start_document_ = true; }
  virtual void EndDocument() { called_end_document_ = true;}
  virtual void StartElement(HtmlElement* element) {
    called_start_element_ = true;
  }
  virtual void EndElement(HtmlElement* element) {
    called_end_element_ = true;
  }
  virtual void Cdata(HtmlCdataNode* cdata) { called_cdata_ = true; }
  virtual void Comment(HtmlCommentNode* comment) { called_comment_ = true; }
  virtual void IEDirective(HtmlIEDirectiveNode* directive) {
    called_ie_directive_ = true;
  }
  virtual void Characters(HtmlCharactersNode* characters) {
    called_characters_ = true;
  }
  virtual void Directive(HtmlDirectiveNode* directive) {
    called_directive_ = true;
  }
  virtual void Flush() { called_flush_ = true; }
  virtual const char* Name() const { return "HandlerCalled"; }

  Bool called_start_document_;
  Bool called_end_document_;
  Bool called_start_element_;
  Bool called_end_element_;
  Bool called_cdata_;
  Bool called_comment_;
  Bool called_ie_directive_;
  Bool called_characters_;
  Bool called_directive_;
  Bool called_flush_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HandlerCalledFilter);
};

class HandlerCalledTest : public HtmlParseTest {
 protected:
  HandlerCalledTest() {
    html_parse_.AddFilter(&handler_called_filter_);
  }

  HandlerCalledFilter handler_called_filter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HandlerCalledTest);
};

// Check that StartDocument and EndDocument were called for filters.
TEST_F(HandlerCalledTest, StartEndDocumentCalled) {
  Parse("start_end_document_called", "");
  EXPECT_TRUE(handler_called_filter_.called_start_document_.Test());
  EXPECT_TRUE(handler_called_filter_.called_end_document_.Test());
}

TEST_F(HandlerCalledTest, StartEndElementCalled) {
  Parse("start_end_element_called", "<p>...</p>");
  EXPECT_TRUE(handler_called_filter_.called_start_element_.Test());
  EXPECT_TRUE(handler_called_filter_.called_end_element_.Test());
}

TEST_F(HandlerCalledTest, CdataCalled) {
  Parse("cdata_called", "<![CDATA[...]]>");
  // Looks like a directive, but isn't.
  EXPECT_FALSE(handler_called_filter_.called_directive_.Test());
  EXPECT_TRUE(handler_called_filter_.called_cdata_.Test());
}

TEST_F(HandlerCalledTest, CommentCalled) {
  Parse("comment_called", "<!--...-->");
  EXPECT_TRUE(handler_called_filter_.called_comment_.Test());
}

TEST_F(HandlerCalledTest, IEDirectiveCalled) {
  Parse("ie_directive_called", "<!--[if IE]>...<![endif]-->");
  // Looks like a comment, but isn't.
  EXPECT_FALSE(handler_called_filter_.called_comment_.Test());
  EXPECT_TRUE(handler_called_filter_.called_ie_directive_.Test());
}

// Unit tests for event-list manipulation.  In these tests, we do not parse
// HTML input text, but instead create two 'Characters' nodes and use the
// event-list manipulation methods and make sure they render as expected.
class EventListManipulationTest : public HtmlParseTest {
 protected:
  EventListManipulationTest() { }

  virtual void SetUp() {
    HtmlParseTest::SetUp();
    static const char kUrl[] = "http://html.parse.test/event_list_test.html";
    html_parse_.StartParse(kUrl);
    node1_ = html_parse_.NewCharactersNode(NULL, "1");
    HtmlTestingPeer::AddEvent(&html_parse_,
                              new HtmlCharactersEvent(node1_, -1));
    node2_ = html_parse_.NewCharactersNode(NULL, "2");
    node3_ = html_parse_.NewCharactersNode(NULL, "3");
    // Note: the last 2 are not added in SetUp.
  }

  virtual void TearDown() {
    html_parse_.FinishParse();
    HtmlParseTest::TearDown();
  }

  void CheckExpected(const std::string& expected) {
    SetupWriter();
    html_parse()->ApplyFilter(html_writer_filter_.get());
    EXPECT_EQ(expected, output_buffer_);
  }

  HtmlCharactersNode* node1_;
  HtmlCharactersNode* node2_;
  HtmlCharactersNode* node3_;
 private:
  DISALLOW_COPY_AND_ASSIGN(EventListManipulationTest);
};

TEST_F(EventListManipulationTest, TestReplace) {
  EXPECT_TRUE(html_parse_.ReplaceNode(node1_, node2_));
  CheckExpected("2");
}

TEST_F(EventListManipulationTest, TestInsertElementBeforeElement) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  html_parse_.InsertElementBeforeElement(node1_, node2_);
  CheckExpected("21");
  html_parse_.InsertElementBeforeElement(node1_, node3_);
  CheckExpected("231");
}

TEST_F(EventListManipulationTest, TestInsertElementAfterElement) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  html_parse_.InsertElementAfterElement(node1_, node2_);
  CheckExpected("12");
  html_parse_.InsertElementAfterElement(node1_, node3_);
  CheckExpected("132");
}

TEST_F(EventListManipulationTest, TestInsertElementBeforeCurrent) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  html_parse_.InsertElementBeforeCurrent(node2_);
  // Current is left at queue_.end() after the AddEvent.
  CheckExpected("12");

  HtmlTestingPeer::SetCurrent(&html_parse_, node1_);
  html_parse_.InsertElementBeforeCurrent(node3_);
  CheckExpected("312");
}

TEST_F(EventListManipulationTest, TestInsertElementAfterCurrent) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlTestingPeer::SetCurrent(&html_parse_, node1_);
  html_parse_.InsertElementAfterCurrent(node2_);
  // Note that if we call CheckExpected here it will mutate current_.
  html_parse_.InsertElementAfterCurrent(node3_);
  CheckExpected("123");
}

TEST_F(EventListManipulationTest, TestDeleteOnly) {
  html_parse_.DeleteElement(node1_);
  CheckExpected("");
}

TEST_F(EventListManipulationTest, TestDeleteFirst) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node3_, -1));
  html_parse_.DeleteElement(node1_);
  CheckExpected("23");
  html_parse_.DeleteElement(node2_);
  CheckExpected("3");
  html_parse_.DeleteElement(node3_);
  CheckExpected("");
}

TEST_F(EventListManipulationTest, TestDeleteLast) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node3_, -1));
  html_parse_.DeleteElement(node3_);
  CheckExpected("12");
  html_parse_.DeleteElement(node2_);
  CheckExpected("1");
  html_parse_.DeleteElement(node1_);
  CheckExpected("");
}

TEST_F(EventListManipulationTest, TestDeleteMiddle) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node3_, -1));
  html_parse_.DeleteElement(node2_);
  CheckExpected("13");
}

// Note that an unconditionaly sanity check runs after every
// filter, verifying that all the parent-pointers are correct.
// CheckExpected applies the HtmlWriterFilter, so it runs the
// parent-pointer check.
TEST_F(EventListManipulationTest, TestAddParentToSequence) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node3_, -1));
  HtmlElement* div = html_parse_.NewElement(NULL, html_parse_.Intern("div"));
  EXPECT_TRUE(html_parse_.AddParentToSequence(node1_, node3_, div));
  CheckExpected("<div>123</div>");

  // Now interpose a span between the div and the Characeters nodes.
  HtmlElement* span = html_parse_.NewElement(div, html_parse_.Intern("span"));
  EXPECT_TRUE(html_parse_.AddParentToSequence(node1_, node2_, span));
  CheckExpected("<div><span>12</span>3</div>");

  // Next, add an HTML block above the div.  Note that we pass 'div' in as
  // both 'first' and 'last'.
  HtmlElement* html = html_parse_.NewElement(NULL, html_parse_.Intern("html"));
  EXPECT_TRUE(html_parse_.AddParentToSequence(div, div, html));
  CheckExpected("<html><div><span>12</span>3</div></html>");
}

TEST_F(EventListManipulationTest, TestPrependChild) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlElement* div = html_parse_.NewElement(NULL, html_parse_.Intern("div"));
  html_parse_.InsertElementBeforeCurrent(div);
  CheckExpected("1<div></div>");

  html_parse_.PrependChild(div, node2_);
  CheckExpected("1<div>2</div>");
  html_parse_.PrependChild(div, node3_);
  CheckExpected("1<div>32</div>");

  // TODO(sligocki): Test with elements that don't explicitly end like image.
}

TEST_F(EventListManipulationTest, TestAppendChild) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlElement* div = html_parse_.NewElement(NULL, html_parse_.Intern("div"));
  html_parse_.InsertElementBeforeCurrent(div);
  CheckExpected("1<div></div>");

  html_parse_.AppendChild(div, node2_);
  CheckExpected("1<div>2</div>");
  html_parse_.AppendChild(div, node3_);
  CheckExpected("1<div>23</div>");

  // TODO(sligocki): Test with elements that don't explicitly end like image.
}

TEST_F(EventListManipulationTest, TestAddParentToSequenceDifferentParents) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlElement* div = html_parse_.NewElement(NULL, html_parse_.Intern("div"));
  EXPECT_TRUE(html_parse_.AddParentToSequence(node1_, node2_, div));
  CheckExpected("<div>12</div>");
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node3_, -1));
  CheckExpected("<div>12</div>3");
  EXPECT_FALSE(html_parse_.AddParentToSequence(node2_, node3_, div));
}

TEST_F(EventListManipulationTest, TestDeleteGroup) {
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlElement* div = html_parse_.NewElement(NULL, html_parse_.Intern("div"));
  EXPECT_TRUE(html_parse_.AddParentToSequence(node1_, node2_, div));
  CheckExpected("<div>12</div>");
  html_parse_.DeleteElement(div);
  CheckExpected("");
}

TEST_F(EventListManipulationTest, TestMoveElementIntoParent1) {
  HtmlElement* head = html_parse_.NewElement(NULL, html_parse_.Intern("head"));
  EXPECT_TRUE(html_parse_.AddParentToSequence(node1_, node1_, head));
  CheckExpected("<head>1</head>");
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlElement* div = html_parse_.NewElement(NULL, html_parse_.Intern("div"));
  EXPECT_TRUE(html_parse_.AddParentToSequence(node2_, node2_, div));
  CheckExpected("<head>1</head><div>2</div>");
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node3_, -1));
  CheckExpected("<head>1</head><div>2</div>3");
  HtmlTestingPeer::SetCurrent(&html_parse_, div);
  EXPECT_TRUE(html_parse_.MoveCurrentInto(head));
  CheckExpected("<head>1<div>2</div></head>3");
}

TEST_F(EventListManipulationTest, TestMoveElementIntoParent2) {
  HtmlTestingPeer::set_coalesce_characters(&html_parse_, false);
  HtmlElement* head = html_parse_.NewElement(NULL, html_parse_.Intern("head"));
  EXPECT_TRUE(html_parse_.AddParentToSequence(node1_, node1_, head));
  CheckExpected("<head>1</head>");
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node3_, -1));
  CheckExpected("<head>1</head>23");
  HtmlElement* div = html_parse_.NewElement(NULL, html_parse_.Intern("div"));
  EXPECT_TRUE(html_parse_.AddParentToSequence(node3_, node3_, div));
  CheckExpected("<head>1</head>2<div>3</div>");
  HtmlTestingPeer::SetCurrent(&html_parse_, div);
  EXPECT_TRUE(html_parse_.MoveCurrentInto(head));
  CheckExpected("<head>1<div>3</div></head>2");
  EXPECT_TRUE(html_parse_.DeleteSavingChildren(div));
  CheckExpected("<head>13</head>2");
  EXPECT_TRUE(html_parse_.DeleteSavingChildren(head));
  CheckExpected("132");
}

TEST_F(EventListManipulationTest, TestCoalesceOnAdd) {
  CheckExpected("1");
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  CheckExpected("12");

  // this will coalesce node1 and node2 togethers.  So there is only
  // one node1_="12", and node2_ is gone.  Deleting node1_ will now
  // leave us empty
  html_parse_.DeleteElement(node1_);
  CheckExpected("");
}

TEST_F(EventListManipulationTest, TestCoalesceOnDelete) {
  CheckExpected("1");
  HtmlElement* div = html_parse_.NewElement(NULL, html_parse_.Intern("div"));
  html_parse_.AddElement(div, -1);
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node2_, -1));
  HtmlTestingPeer testing_peer;
  testing_peer.SetNodeParent(node2_, div);
  html_parse_.CloseElement(div, HtmlElement::EXPLICIT_CLOSE, -1);
  HtmlTestingPeer::AddEvent(&html_parse_, new HtmlCharactersEvent(node3_, -1));
  CheckExpected("1<div>2</div>3");

  // Removing the div, leaving the children intact...
  EXPECT_TRUE(html_parse_.DeleteSavingChildren(div));
  CheckExpected("123");

  // At this point, node1, node2, and node3 are automatically coalesced.
  // This means when we remove node1, all the content will disappear.
  html_parse_.DeleteElement(node1_);
  CheckExpected("");
}

// Unit tests for attribute manipulation.
// Goal is to make sure we don't (eg) read deallocated storage
// while manipulating attribute values.
class AttributeManipulationTest : public HtmlParseTest {
 protected:
  AttributeManipulationTest() { }

  virtual void SetUp() {
    HtmlParseTest::SetUp();
    static const char kUrl[] =
        "http://html.parse.test/attribute_manipulation_test.html";
    html_parse_.StartParse(kUrl);
    node_ = html_parse_.NewElement(NULL, MakeAtom("a"));
    html_parse_.AddElement(node_, 0);
    node_->AddAttribute(MakeAtom("href"), "http://www.google.com/", "\"");
    node_->AddAttribute(MakeAtom("id"), "37", "");
    node_->AddAttribute(MakeAtom("class"), "search!", "'");
    html_parse_.CloseElement(node_, HtmlElement::BRIEF_CLOSE, 0);
  }

  virtual void TearDown() {
    html_parse_.FinishParse();
    HtmlParseTest::TearDown();
  }

  Atom MakeAtom(const char *name) {
    return html_parse_.Intern(name);
  }

  void CheckExpected(const std::string& expected) {
    SetupWriter();
    html_parse_.ApplyFilter(html_writer_filter_.get());
    EXPECT_EQ(expected, output_buffer_);
  }

  HtmlElement* node_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AttributeManipulationTest);
};

TEST_F(AttributeManipulationTest, PropertiesAndDeserialize) {
  StringPiece google("http://www.google.com/");
  StringPiece number37("37");
  StringPiece search("search!");
  EXPECT_EQ(3, node_->attribute_size());
  EXPECT_EQ(google, node_->AttributeValue(MakeAtom("href")));
  EXPECT_EQ(number37, node_->AttributeValue(MakeAtom("id")));
  EXPECT_EQ(search, node_->AttributeValue(MakeAtom("class")));
  EXPECT_TRUE(NULL == node_->AttributeValue(MakeAtom("absent")));
  int val = -35;
  EXPECT_FALSE(node_->IntAttributeValue(MakeAtom("absent"), &val));
  EXPECT_EQ(-35, val);
  EXPECT_FALSE(node_->IntAttributeValue(MakeAtom("href"), &val));
  EXPECT_EQ(0, val);
  EXPECT_TRUE(node_->IntAttributeValue(MakeAtom("id"), &val));
  EXPECT_EQ(37, val);
  EXPECT_TRUE(NULL == node_->FindAttribute(MakeAtom("absent")));
  EXPECT_EQ(google, node_->FindAttribute(MakeAtom("href"))->value());
  EXPECT_EQ(number37, node_->FindAttribute(MakeAtom("id"))->value());
  EXPECT_EQ(search, node_->FindAttribute(MakeAtom("class"))->value());
  EXPECT_EQ(google, node_->FindAttribute(MakeAtom("href"))->escaped_value());
  EXPECT_EQ(number37, node_->FindAttribute(MakeAtom("id"))->escaped_value());
  EXPECT_EQ(search, node_->FindAttribute(MakeAtom("class"))->escaped_value());
  CheckExpected("<a href=\"http://www.google.com/\" id=37 class='search!'/>");
}

TEST_F(AttributeManipulationTest, AddAttribute) {
  node_->AddAttribute(MakeAtom("lang"), "ENG-US", "\"");
  CheckExpected("<a href=\"http://www.google.com/\" id=37 class='search!'"
                " lang=\"ENG-US\"/>");
}

TEST_F(AttributeManipulationTest, DeleteAttribute) {
  node_->DeleteAttribute(1);
  CheckExpected("<a href=\"http://www.google.com/\" class='search!'/>");
}

TEST_F(AttributeManipulationTest, ModifyAttribute) {
  HtmlElement::Attribute* href =
      node_->FindAttribute(MakeAtom("href"));
  EXPECT_TRUE(href != NULL);
  href->SetValue("google");
  href->set_quote("'");
  href->set_name(MakeAtom("src"));
  CheckExpected("<a src='google' id=37 class='search!'/>");
}

TEST_F(AttributeManipulationTest, ModifyKeepAttribute) {
  HtmlElement::Attribute* href =
      node_->FindAttribute(MakeAtom("href"));
  EXPECT_TRUE(href != NULL);
  // This apparently do-nothing call to SetValue exposed an allocation bug.
  href->SetValue(href->value());
  href->set_quote(href->quote());
  href->set_name(href->name());
  CheckExpected("<a href=\"http://www.google.com/\" id=37 class='search!'/>");
}

}  // namespace net_instaweb
