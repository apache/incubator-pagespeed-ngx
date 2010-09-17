// Copyright 2010 and onwards Google Inc.
// Author: mdsteele@google.com (Matthew D. Steele)

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/rewriter/public/html_attribute_quote_removal.h"

namespace net_instaweb {

class HtmlAttributeQuoteRemovalTest : public HtmlParseTestBase {
 protected:
  HtmlAttributeQuoteRemovalTest()
      : html_attribute_quote_removal_(&html_parse_) {
    html_parse_.AddFilter(&html_attribute_quote_removal_);
  }

  virtual bool AddBody() const { return true; }

 private:
  HtmlAttributeQuoteRemoval html_attribute_quote_removal_;

  DISALLOW_COPY_AND_ASSIGN(HtmlAttributeQuoteRemovalTest);
};

TEST_F(HtmlAttributeQuoteRemovalTest, NoQuotesNoChange) {
  ValidateNoChanges("no_quotes_no_change",
                    "<div class=foo id=bar>foobar</div>");
}

TEST_F(HtmlAttributeQuoteRemovalTest, DoNotRemoveNeededQuotes) {
  ValidateNoChanges("do_not_remove_needed_quotes",
                    "<a href=\"http://www.example.com/\">foobar</a>");
}

TEST_F(HtmlAttributeQuoteRemovalTest, DoNotDeleteEmptyAttrs) {
  ValidateNoChanges("do_not_delete_empty_attrs",
                    "<div id=''></div>");
}

TEST_F(HtmlAttributeQuoteRemovalTest, RemoveUnneededQuotes) {
  ValidateExpected("remove_unneeded_quotes",
                   "<div class=\"foo\" id='bar'>foobar</div>",
                   "<div class=foo id=bar>foobar</div>");
}

TEST_F(HtmlAttributeQuoteRemovalTest, NoValueNoChange) {
  ValidateNoChanges("no_value_no_change",
                    "<input checked type=checkbox>");
}

}  // namespace net_instaweb
