/*
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

// Author: mdsteele@google.com (Matthew D. Steele)

#include "net/instaweb/rewriter/public/script_tag_scanner.h"

#include <vector>
#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class ScriptTagScannerTest : public HtmlParseTestBase {
 protected:
  ScriptTagScannerTest() : collector_(&html_parse_) {
    html_parse_.AddFilter(&collector_);
  }

  virtual bool AddBody() const { return true; }

  // Helper class to collect script information (language,
  // and attributes)
  class ScriptCollector : public EmptyHtmlFilter {
   public:
    ScriptCollector(HtmlParse* html_parse)
        : script_tag_scanner_(html_parse) {
    }

    virtual void StartElement(HtmlElement* element) {
      HtmlElement::Attribute* src;
      ScriptInfo info;
      info.classification =
          script_tag_scanner_.ParseScriptElement(element, &src);
      if (info.classification != ScriptTagScanner::kNonScript) {
        if (src) {
          info.url = src->value();
        }
        info.flags = script_tag_scanner_.ExecutionMode(element);
        scripts_.push_back(info);
      }
    }

    int Size() const { return static_cast<int>(scripts_.size()); }

    const GoogleString& UrlAt(int pos) const {
      return scripts_[pos].url;
    }

    ScriptTagScanner::ScriptClassification ClassificationAt(int pos) {
      return scripts_[pos].classification;
    }

    int FlagsAt(int pos) {
      return scripts_[pos].flags;
    }

    virtual const char* Name() const { return "ScriptCollector"; }

   private:
    struct ScriptInfo {
      GoogleString url;
      ScriptTagScanner::ScriptClassification classification;
      int flags;
    };

    std::vector<ScriptInfo> scripts_;
    ScriptTagScanner script_tag_scanner_;

    DISALLOW_COPY_AND_ASSIGN(ScriptCollector);
  };

  struct TestSpec {
    const char* attributes;
    int expected_flags;
  };

  // Checks to make sure each of attributes inside <script> produces
  // the appropriate flags. The array is expected to be 0-terminated
  void TestFlags(const TestSpec* test_spec) {
    GoogleString html;
    int test;
    for (test = 0; test_spec[test].attributes; ++test) {
      html += "<script " + GoogleString(test_spec[test].attributes) +
              "></script>";
    }

    ValidateNoChanges("from_test_spec", html);

    ASSERT_EQ(test, collector_.Size());
    for (test = 0; test_spec[test].attributes; ++test) {
      LOG(INFO) << test_spec[test].attributes;
      EXPECT_EQ(test_spec[test].expected_flags, collector_.FlagsAt(test));
    }
  }

  GoogleString ScriptWithType(const GoogleString& type) {
    return "<script type=\"" + type + "\"></script>";
  }

  GoogleString ScriptWithLang(const GoogleString& type) {
    return "<script language=\"" + type + "\"></script>";
  }

  ScriptCollector collector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScriptTagScannerTest);
};

// Note: kNonScript is covered by the length counts,
// as it will not go into the collector

TEST_F(ScriptTagScannerTest, NotFoundScriptTag) {
  ValidateNoChanges("noscript", "<noscript>");
  ASSERT_EQ(0, collector_.Size());
}

TEST_F(ScriptTagScannerTest, FindNoScriptTag) {
  ValidateNoChanges("simple_script", "<script src=\"myscript.js\"></script>");
  ASSERT_EQ(1, collector_.Size());
  EXPECT_EQ(GoogleString("myscript.js"), collector_.UrlAt(0));
  EXPECT_EQ(ScriptTagScanner::kJavaScript, collector_.ClassificationAt(0));
}

TEST_F(ScriptTagScannerTest, TypeNoVal) {
  // type with no value - handle as JS
  ValidateNoChanges("simple_script", "<script type></script>");
  ASSERT_EQ(1, collector_.Size());
  EXPECT_EQ(GoogleString(), collector_.UrlAt(0));
  EXPECT_EQ(ScriptTagScanner::kJavaScript, collector_.ClassificationAt(0));
}

TEST_F(ScriptTagScannerTest, TypeEmpty) {
  // type is empty - handle as JS
  ValidateNoChanges("simple_script", "<script type=""></script>");
  ASSERT_EQ(1, collector_.Size());
  EXPECT_EQ(GoogleString(), collector_.UrlAt(0));
  EXPECT_EQ(ScriptTagScanner::kJavaScript, collector_.ClassificationAt(0));
}

TEST_F(ScriptTagScannerTest, TypeNoValHaveLang) {
  // type is no-value, but language is there --- it matters
  ValidateNoChanges("simple_script", "<script type language=tcl></script>");
  ASSERT_EQ(1, collector_.Size());
  EXPECT_EQ(GoogleString(), collector_.UrlAt(0));
  EXPECT_EQ(ScriptTagScanner::kUnknownScript, collector_.ClassificationAt(0));
}

TEST_F(ScriptTagScannerTest, TypeLangSubordinate) {
  // make sure type beats language
  ValidateNoChanges("simple_script",
                    "<script type=\"text/ecmascript\" language=tcl></script>");
  ASSERT_EQ(1, collector_.Size());
  EXPECT_EQ(GoogleString(), collector_.UrlAt(0));
  EXPECT_EQ(ScriptTagScanner::kJavaScript, collector_.ClassificationAt(0));
}

TEST_F(ScriptTagScannerTest, LangNoVal) {
  // lang no value - handle as JS
  ValidateNoChanges("simple_script", "<script language></script>");
  ASSERT_EQ(1, collector_.Size());
  EXPECT_EQ(GoogleString(), collector_.UrlAt(0));
  EXPECT_EQ(ScriptTagScanner::kJavaScript, collector_.ClassificationAt(0));
}

TEST_F(ScriptTagScannerTest, LangEmpty) {
  // lang is empty - handle as JS
  ValidateNoChanges("simple_script", "<script language=""></script>");
  ASSERT_EQ(1, collector_.Size());
  EXPECT_EQ(GoogleString(), collector_.UrlAt(0));
  EXPECT_EQ(ScriptTagScanner::kJavaScript, collector_.ClassificationAt(0));
}

TEST_F(ScriptTagScannerTest, TypeScripts) {
  // various type values. Nothing fancy done with them. List of types is from
  // HTML5 + a few ones that are not
  ValidateNoChanges("script types",
      ScriptWithType("application/ecmascript") +   // 0
      ScriptWithType("application/javascript") +
      ScriptWithType("application/x-ecmascript") +
      ScriptWithType("application/x-javascript") +
      ScriptWithType("text/ecmascript") +  // 4
      ScriptWithType("text/javascript") +
      ScriptWithType("text/javascript1.0") +
      ScriptWithType("text/javascript1.1") +
      ScriptWithType("text/javascript1.2") +
      ScriptWithType("text/javascript1.3") + // 9
      ScriptWithType("text/javascript1.4") +
      ScriptWithType("text/javascript1.5") +
      ScriptWithType("text/jscript") +
      ScriptWithType("text/livescript") +
      ScriptWithType("text/x-ecmascript") + // 14
      ScriptWithType("text/x-javascript") + // 15 -- last valid one
      ScriptWithType("text/tcl") +
      ScriptWithType("text/ecmascript4") +
      ScriptWithType("text/javascript2.0") +
      ScriptWithType("                  ")); // 19 -- last invalid one

  ASSERT_EQ(20, collector_.Size());
  for (int i = 0; i <= 15; ++i) {
    EXPECT_EQ(GoogleString(), collector_.UrlAt(i));
    EXPECT_EQ(ScriptTagScanner::kJavaScript, collector_.ClassificationAt(i));
  }

  for (int i = 16; i <= 19; ++i) {
    EXPECT_EQ(GoogleString(), collector_.UrlAt(i));
    EXPECT_EQ(ScriptTagScanner::kUnknownScript, collector_.ClassificationAt(i));
  }
}

TEST_F(ScriptTagScannerTest, TypeScriptsNormalize) {
  // For type, we need to support removal of leading/trailing whitespace
  // and case folding
  ValidateNoChanges("script types",
      ScriptWithType("  application/ecmascRipt") +   // 0
      ScriptWithType("      applicAtion/javascript  ") +
      ScriptWithType("application/x-ecmaScript  ") +
      ScriptWithType("   applicAtion/x-javascript") +
      ScriptWithType("text/Ecmascript") +  // 4
      ScriptWithType("     text/jaVasCript    ") +
      ScriptWithType(" TEXt/javascript1.0\t") +
      ScriptWithType("  text/javascript1.1") +
      ScriptWithType(" teXt/javascripT1.2") +
      ScriptWithType("\ttExt/javascRipt1.3 ") + // 9
      ScriptWithType("  text/javascRipT1.4  ") +
      ScriptWithType("  Text/javAscript1.5 ") +
      ScriptWithType("   Text/jscrIpt") +
      ScriptWithType("   text/lIvescript") +
      ScriptWithType("teXt/x-ecmasCript ") + // 14
      ScriptWithType("tExt/x-jaVascript ") + // 15 -- last valid one
      ScriptWithType("Text/Tcl ") +
      ScriptWithType(" text/Ecmascript4") +
      ScriptWithType("tExt/javascript2.0")+
      ScriptWithType("text/javasc ript")); // 19 -- last invalid one

  ASSERT_EQ(20, collector_.Size());
  for (int i = 0; i <= 15; ++i) {
    EXPECT_EQ(GoogleString(), collector_.UrlAt(i));
    EXPECT_EQ(ScriptTagScanner::kJavaScript, collector_.ClassificationAt(i));
  }

  for (int i = 16; i <= 19; ++i) {
    EXPECT_EQ(GoogleString(), collector_.UrlAt(i));
    EXPECT_EQ(ScriptTagScanner::kUnknownScript, collector_.ClassificationAt(i));
  }
}

TEST_F(ScriptTagScannerTest, LangScripts) {
  // for language attribute, we are supposed to test text/lang
  // against the valid mimetypes list
  ValidateNoChanges("script langs",
      ScriptWithLang("ecmascript") +
      ScriptWithLang("javascript") +
      ScriptWithLang("javascript1.0") +
      ScriptWithLang("javascript1.1") +
      ScriptWithLang("javascript1.2") + // 4
      ScriptWithLang("javascript1.3") +
      ScriptWithLang("javascript1.4") +
      ScriptWithLang("javascript1.5") +
      ScriptWithLang("jscript") +
      ScriptWithLang("livescript") +   // 9
      ScriptWithLang("x-ecmascript") +
      ScriptWithLang("x-javascript") + // 11 -- last valid one
      ScriptWithLang("tcl") +
      ScriptWithLang("ecmascript4") +
      ScriptWithLang("javascript2.0")); // 14 -- last invalid one

  ASSERT_EQ(15, collector_.Size());
  for (int i = 0; i <= 11; ++i) {
    EXPECT_EQ(GoogleString(), collector_.UrlAt(i));
    EXPECT_EQ(ScriptTagScanner::kJavaScript, collector_.ClassificationAt(i));
  }

  for (int i = 12; i <= 14; ++i) {
    EXPECT_EQ(GoogleString(), collector_.UrlAt(i));
    EXPECT_EQ(ScriptTagScanner::kUnknownScript, collector_.ClassificationAt(i));
  }
}

TEST_F(ScriptTagScannerTest, LangScriptsNormalizeCase) {
  // Case normalization is to be done for language="" as well.
  ValidateNoChanges("script langs",
      ScriptWithLang("ecmasCript") +
      ScriptWithLang("javAscript") +
      ScriptWithLang("javascript1.0") +
      ScriptWithLang("javascRipt1.1") +
      ScriptWithLang("javascripT1.2") + // 4
      ScriptWithLang("javaScrIpt1.3") +
      ScriptWithLang("jaVasCript1.4") +
      ScriptWithLang("javaScriPt1.5") +
      ScriptWithLang("jscRiPt") +
      ScriptWithLang("livEscript") +   // 9
      ScriptWithLang("x-ecmaScript") +
      ScriptWithLang("x-jaVascript") + // 11 -- last valid one
      ScriptWithLang("tCl") +
      ScriptWithLang("ecmasCript4") +
      ScriptWithLang("jaVascript2.0")); // 14 -- last invalid one

  ASSERT_EQ(15, collector_.Size());
  for (int i = 0; i <= 11; ++i) {
    EXPECT_EQ(GoogleString(), collector_.UrlAt(i));
    EXPECT_EQ(ScriptTagScanner::kJavaScript, collector_.ClassificationAt(i));
  }

  for (int i = 12; i <= 14; ++i) {
    EXPECT_EQ(GoogleString(), collector_.UrlAt(i));
    EXPECT_EQ(ScriptTagScanner::kUnknownScript, collector_.ClassificationAt(i));
  }
}

TEST_F(ScriptTagScannerTest, LangScriptsNormalizeWhitespace) {
  // Whitespace, however, is not removed for language, unlike with type,
  // so all of these are to fail
  ValidateNoChanges("script langs",
      ScriptWithLang(" ecmascript") +
      ScriptWithLang("javascript\t") +
      ScriptWithLang("  javascript1.0  ") +
      ScriptWithLang(" javascript1.1") +
      ScriptWithLang("javascript1.2 ") + // 4
      ScriptWithLang("  javascript1.3") +
      ScriptWithLang("javascript1.4 ") +
      ScriptWithLang("  javascript1.5") +
      ScriptWithLang("jscript ") +
      ScriptWithLang("livescript  ") +   // 9
      ScriptWithLang("  x-ecmascript") +
      ScriptWithLang("x-javascript\t") +
      ScriptWithLang("  tcl  ") +
      ScriptWithLang("ecmascript4  ") +
      ScriptWithLang("  javascript2.0")); // 14 -- last invalid one

  ASSERT_EQ(15, collector_.Size());
  for (int i = 0; i <= 14; ++i) {
    EXPECT_EQ(GoogleString(), collector_.UrlAt(i));
    EXPECT_EQ(ScriptTagScanner::kUnknownScript, collector_.ClassificationAt(i));
  }
}

TEST_F(ScriptTagScannerTest, ForEvent) {
  TestSpec for_event_tests[] = {
    { "for event", ScriptTagScanner::kExecuteForEvent },
    { "for=\"\" event=\"\"", ScriptTagScanner::kExecuteForEvent },
    { "for", ScriptTagScanner::kExecuteSync },
    { "event", ScriptTagScanner::kExecuteSync },
    { "for=\"a\" event=\"b\"", ScriptTagScanner::kExecuteForEvent },
    { "for=\"window\" event=\"b\"", ScriptTagScanner::kExecuteForEvent },
    { "for=\"window\" event=\"b\" async",
        ScriptTagScanner::kExecuteForEvent | ScriptTagScanner::kExecuteAsync },
    { "for=\"window\" event=\"onload\"", ScriptTagScanner::kExecuteSync },
    { "for=\"window\" event=onload async", ScriptTagScanner::kExecuteAsync },
    { "for=\"window\" event=\"onload()\"", ScriptTagScanner::kExecuteSync },
    { "for=\"wiNdow \" event=\" onLoad  \"", ScriptTagScanner::kExecuteSync },
    { "for=\" windOw\" event=\"OnloAd() \"", ScriptTagScanner::kExecuteSync },
    { 0, ScriptTagScanner::kExecuteSync }
  };
  TestFlags(for_event_tests);
}

TEST_F(ScriptTagScannerTest, AsyncDefer) {
  TestSpec async_defer_tests[] = {
    { "language=tcl async", ScriptTagScanner::kExecuteAsync },
    { "async=\"irrelevant\"", ScriptTagScanner::kExecuteAsync },
    { "defer", ScriptTagScanner::kExecuteDefer },
    { "defer async",
        ScriptTagScanner::kExecuteDefer | ScriptTagScanner::kExecuteAsync },
    { "language=tcl async src=a", ScriptTagScanner::kExecuteAsync },
    { "async=\"irrelevant\" src=a", ScriptTagScanner::kExecuteAsync },
    { "defer src=a", ScriptTagScanner::kExecuteDefer },
    { "defer async src=a",
        ScriptTagScanner::kExecuteDefer | ScriptTagScanner::kExecuteAsync },
    { 0, ScriptTagScanner::kExecuteSync }
  };
  TestFlags(async_defer_tests);
}

}  // namespace net_instaweb
