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

// Author: slamm@google.com (Stephen Lamm)

// The Google Analytics writer filter by scanning all the <script> elements.
// The rewriter follows these steps:
//     1. Match fixed strings that represent a synchronous load
//        o Can be either document.write or <script src=...>
//     2. Match calls to _gat._getTracker
//        o Only done if step #1 succeeds.
//     3. Match any methods that the rewriter cannot handle such as the
//        Google Analytics methods that return values.
//        o Only done if step #2 succeeds.
//        o If any unhandled methods are found, the rewriter resets to the
//          first step.
//     4. At the end of the document, perform the rewrite if steps #1 and #2
//        succeeded and the matched script elements are editable (i.e. in
//        the current buffer).
//

#include "net/instaweb/rewriter/public/google_analytics_filter.h"

#include <cctype>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/rewriter/google_analytics_snippet.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

const char kGaJsUrlSuffix[] = "google-analytics.com/ga.js";
const char kGaJsDocumentWriteStart[] = "document.write(";
const char kGaJsDocumentWriteEnd[] = "%3C/script%3E\"));";
const char kGaJsGetTracker[] = "_gat._getTracker(";
const char kGaJsCreateTracker[] = "_gat._createTracker(";


const char GoogleAnalyticsFilter::kPageLoadCount[] =
    "google_analytics_page_load_count";
const char GoogleAnalyticsFilter::kRewrittenCount[] =
    "google_analytics_rewritten_count";


ScriptEditor::ScriptEditor(HtmlElement* script_element,
                           HtmlCharactersNode *characters_node,
                           GoogleString::size_type pos,
                           GoogleString::size_type len,
                           Type editor_type)
    : script_element_(script_element),
      script_characters_node_(characters_node),
      pos_(pos),
      len_(len),
      editor_type_(editor_type) {}

void ScriptEditor::NewContents(const StringPiece& replacement,
                               GoogleString* contents) const {
  if (pos_ == GoogleString::npos) {
    replacement.CopyToString(contents);
  } else {
    StringPiece old_contents = script_characters_node_->contents();
    contents->clear();
    contents->append(old_contents.data(), pos_);
    contents->append(replacement.data(), replacement.size());
    StringPiece suffix = old_contents.substr(pos_ + len_,
                                             old_contents.size() - pos_ - len_);
    contents->append(suffix.data(), suffix.size());
  }
}


GoogleAnalyticsFilter::GoogleAnalyticsFilter(
    HtmlParse* html_parse, Statistics* stats)
    : glue_methods_(new MethodVector),
      unhandled_methods_(new MethodVector),
      html_parse_(html_parse),
      script_element_(NULL),
      script_characters_node_(NULL),
      page_load_count_((stats == NULL) ? NULL :
                       stats->GetVariable(kPageLoadCount)),
      rewritten_count_((stats == NULL) ? NULL :
                       stats->GetVariable(kRewrittenCount))
{
  // The following are the methods that need to be forwarded to the asyn
  // interface. This list was created by parsing ga.js and finding the method
  // names in the documentation. Methods that return values were added to the
  // list of unhandled methods.
  glue_methods_->push_back("_trackPageview");
  glue_methods_->push_back("_trackEvent");
  glue_methods_->push_back("_trackTrans");
  glue_methods_->push_back("_addIgnoredOrganic");
  glue_methods_->push_back("_addIgnoredRef");
  glue_methods_->push_back("_addItem");
  glue_methods_->push_back("_addOrganic");
  glue_methods_->push_back("_addTrans");
  glue_methods_->push_back("_clearIgnoredOrganic");
  glue_methods_->push_back("_clearIgnoredRef");
  glue_methods_->push_back("_clearOrganic");
  glue_methods_->push_back("_clearXKey");
  glue_methods_->push_back("_clearXValue");
  glue_methods_->push_back("_cookiePathCopy");
  glue_methods_->push_back("_deleteCustomVar");
  glue_methods_->push_back("_link");
  glue_methods_->push_back("_linkByPost");
  glue_methods_->push_back("_sendXEvent");
  glue_methods_->push_back("_setAllowAnchor");
  glue_methods_->push_back("_setAllowHash");
  glue_methods_->push_back("_setAllowLinker");
  glue_methods_->push_back("_setAutoTrackOutbound");
  glue_methods_->push_back("_setCampCIdKey");
  glue_methods_->push_back("_setCampContentKey");
  glue_methods_->push_back("_setCampIdKey");
  glue_methods_->push_back("_setCampMediumKey");
  glue_methods_->push_back("_setCampNOKey");
  glue_methods_->push_back("_setCampNameKey");
  glue_methods_->push_back("_setCampSourceKey");
  glue_methods_->push_back("_setCampTermKey");
  glue_methods_->push_back("_setCampaignCookieTimeout");
  glue_methods_->push_back("_setCampaignTrack");
  glue_methods_->push_back("_setClientInfo");
  glue_methods_->push_back("_setCookiePath");
  glue_methods_->push_back("_setCookiePersistence");
  glue_methods_->push_back("_setCookieTimeout");
  glue_methods_->push_back("_setCustomVar");
  glue_methods_->push_back("_setDetectFlash");
  glue_methods_->push_back("_setDetectTitle");
  glue_methods_->push_back("_setDomainName");
  glue_methods_->push_back("_setHrefExamineLimit");
  glue_methods_->push_back("_setLocalGifPath");
  glue_methods_->push_back("_setLocalRemoteServerMode");
  glue_methods_->push_back("_setLocalServerMode");
  glue_methods_->push_back("_setMaxCustomVariables");
  glue_methods_->push_back("_setNamespace");
  glue_methods_->push_back("_setReferrerOverride");
  glue_methods_->push_back("_setRemoteServerMode");
  glue_methods_->push_back("_setSampleRate");
  glue_methods_->push_back("_setSessionCookieTimeout");
  glue_methods_->push_back("_setSessionTimeout");
  glue_methods_->push_back("_setTrackOutboundSubdomains");
  glue_methods_->push_back("_setTrans");
  glue_methods_->push_back("_setTransactionDelim");
  glue_methods_->push_back("_setVar");
  glue_methods_->push_back("_setVisitorCookieTimeout");
  glue_methods_->push_back("_setXKey");
  glue_methods_->push_back("_setXValue");

  unhandled_methods_->push_back("_anonymizeIp");
  unhandled_methods_->push_back("_createEventTracker");  // getter method
  unhandled_methods_->push_back("_createXObj");          // getter method
  unhandled_methods_->push_back("_require");
  unhandled_methods_->push_back("_visitCode");           // getter method
  unhandled_methods_->push_back("_get");
  unhandled_methods_->push_back("_getAccount");
  unhandled_methods_->push_back("_getClientInfo");
  unhandled_methods_->push_back("_getDetectFlash");
  unhandled_methods_->push_back("_getDetectTitle");
  unhandled_methods_->push_back("_getLinkerUrl");
  unhandled_methods_->push_back("_getLocalGifPath");
  unhandled_methods_->push_back("_getName");
  unhandled_methods_->push_back("_getServiceMode");
  unhandled_methods_->push_back("_getTrackerByName");
  unhandled_methods_->push_back("_getVersion");
  unhandled_methods_->push_back("_getVisitorCustomVar");
  unhandled_methods_->push_back("_getXKey");
  unhandled_methods_->push_back("_getXValue");
  unhandled_methods_->push_back("_setAccount");  // async only
}


GoogleAnalyticsFilter::GoogleAnalyticsFilter(
    HtmlParse* html_parse, Statistics* stats,
    MethodVector* glue_methods, MethodVector* unhandled_methods)
    : glue_methods_(glue_methods),
      unhandled_methods_(unhandled_methods),
      html_parse_(html_parse),
      script_element_(NULL),
      script_characters_node_(NULL),
      page_load_count_((stats == NULL) ? NULL :
                       stats->GetVariable(kPageLoadCount)),
      rewritten_count_((stats == NULL) ? NULL :
                       stats->GetVariable(kRewrittenCount))
    { }

GoogleAnalyticsFilter::~GoogleAnalyticsFilter() {}

void GoogleAnalyticsFilter::Initialize(Statistics* statistics) {
  if (statistics != NULL) {
    statistics->AddVariable(kPageLoadCount);
    statistics->AddVariable(kRewrittenCount);
  }
}

void GoogleAnalyticsFilter::StartDocument() {
  ResetFilter();
  page_load_count_->Add(1);
}

void GoogleAnalyticsFilter::EndDocument() {
  if (is_load_found_) {
    if (is_init_found_) {
      if (RewriteAsAsync()) {
        rewritten_count_->Add(1);
        html_parse_->InfoHere("Google Analytics rewritten: SUCCESS!");
      } else {
        html_parse_->InfoHere("Google Analytics not rewritten: rewrite failed");
      }
    } else {
      html_parse_->InfoHere(
          "Google Analytics not rewritten: only found ga.js load");
    }
  }
  ResetFilter();
}

void GoogleAnalyticsFilter::StartElement(HtmlElement* element) {
  // No tags allowed inside script element.
  if (script_element_ != NULL) {
    html_parse_->ErrorHere("Reset: Tag '%s' found inside script.",
                           element->name_str());
    ResetFilter();
  }
  if (element->keyword() == HtmlName::kScript) {
    script_element_ = element;
  }
}

void GoogleAnalyticsFilter::EndElement(HtmlElement* element) {
  if (script_element_ != NULL) {
    if (element != script_element_) {
      html_parse_->ErrorHere("Reset: Unexpected tag '%s' inside a script.",
                             element->name_str());
      ResetFilter();
    } else {
      FindRewritableScripts();
      script_element_ = NULL;
      script_characters_node_ = NULL;
    }
  }
}

void GoogleAnalyticsFilter::Flush() {
  if (script_element_ != NULL) {
    html_parse_->InfoHere("Reset: flush in a script.");
    ResetFilter();
  }
}

void GoogleAnalyticsFilter::Characters(HtmlCharactersNode* characters_node) {
  if (script_element_ != NULL) {
    if (script_characters_node_ == NULL) {
      script_characters_node_ = characters_node;
    } else {
      html_parse_->ErrorHere("Reset: multiple character nodes in script.");
      ResetFilter();
    }
  }
}

void GoogleAnalyticsFilter::Comment(HtmlCommentNode* comment) {
  if (script_element_ != NULL) {
    html_parse_->InfoHere("Reset: comment found inside script.");
    ResetFilter();
  }
}

void GoogleAnalyticsFilter::Cdata(HtmlCdataNode* cdata) {
  if (script_element_ != NULL) {
    html_parse_->InfoHere("Reset: CDATA found inside script.");
    ResetFilter();
  }
}

void GoogleAnalyticsFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  if (script_element_ != NULL) {
    html_parse_->ErrorHere("Reset: IE Directive found inside script.");
    ResetFilter();
  }
}

void GoogleAnalyticsFilter::ResetFilter() {
  script_element_ = NULL;
  script_characters_node_ = NULL;
  is_init_found_ = false;
  is_load_found_ = false;
  STLDeleteContainerPointers(script_editors_.begin(),
                             script_editors_.end());
  script_editors_.clear();
}

bool GoogleAnalyticsFilter::MatchSyncLoad(StringPiece contents,
                                          GoogleString::size_type &pos,
                                          GoogleString::size_type &len) const {
  GoogleString::size_type url_pos = contents.find(kGaJsUrlSuffix);
  if (url_pos != GoogleString::npos) {
    // In the common case, document.write is 56 characters before the url.
    // Allow a little extra wiggle room (e.g. for different formating), but
    // not so much that an unrelated document.write is found.
    const GoogleString::size_type max_distance = 80;
    GoogleString::size_type write_start_pos =
        url_pos < max_distance ? 0 : url_pos - max_distance;
    StringPiece write_start(contents.data() + write_start_pos,
                            url_pos - write_start_pos);
    GoogleString::size_type write_pos = write_start.find(
        kGaJsDocumentWriteStart);
    if (write_pos == GoogleString::npos) {
      html_parse_->InfoHere("Found ga.js without a matching document.write");
    } else {
      write_pos += write_start_pos;
      GoogleString::size_type write_end_pos = contents.find(
          kGaJsDocumentWriteEnd,
          url_pos + StringPiece(kGaJsUrlSuffix).size());
      if (write_end_pos != GoogleString::npos) {
        write_end_pos += StringPiece(kGaJsDocumentWriteEnd).size();
        pos = write_pos;
        len = write_end_pos - write_pos;
        html_parse_->InfoHere("Found ga.js load: document.write");
        return true;
      }
    }
  }
  return false;
}

bool GoogleAnalyticsFilter::MatchSyncInit(StringPiece contents,
                                          GoogleString::size_type start_pos,
                                          GoogleString::size_type &pos,
                                          GoogleString::size_type &len) const {
  StringPiece tracker_method(kGaJsGetTracker);
  GoogleString::size_type tracker_method_pos = contents.find(
      tracker_method, start_pos);
  if (tracker_method_pos == GoogleString::npos) {
    tracker_method = StringPiece(kGaJsCreateTracker);
    tracker_method_pos = contents.find(tracker_method, start_pos);
  }
  if (tracker_method_pos != GoogleString::npos) {
    html_parse_->InfoHere("Found ga.js init: %s", tracker_method.data());
    pos = tracker_method_pos;
    len = tracker_method.size();
    return true;
  }
  return false;
}

bool GoogleAnalyticsFilter::MatchUnhandledCalls(
    StringPiece contents, GoogleString::size_type start_pos) const {
  // TODO(slamm): Use a more efficient multiple pattern algorithm
  while (1) {
    GoogleString::size_type candidate_pos = contents.find("._");
    if (candidate_pos == GoogleString::npos) {
      break;
    }
    contents = contents.substr(candidate_pos + 1);
    for (int i = 0, ni = unhandled_methods_->size(); i < ni; ++i) {
      const StringPiece& method = unhandled_methods_->at(i);
      if (contents.starts_with(method)) {
        for (int j = method.size(), nj = contents.size(); j < nj; ++j) {
          char c = contents[j];
          if (c == '(') {
            html_parse_->InfoHere("Matched unhandled call: %s", method.data());
            return true;
          } else if (!isspace(c)) {
            break;
          }
        }
      }
    }
  }
  return false;
}

void GoogleAnalyticsFilter::FindRewritableScripts() {
  if (html_parse_->IsRewritable(script_element_)) {
    StringPiece src = script_element_->AttributeValue(HtmlName::kSrc);
    if (src != NULL && !src.empty()) {
      if (src.ends_with(kGaJsUrlSuffix)) {
        html_parse_->InfoHere("Found ga.js load: script src");
        is_load_found_ = true;
        script_editors_.push_back(new ScriptEditor(
            script_element_, script_characters_node_,
            GoogleString::npos, GoogleString::npos,
            ScriptEditor::kGaJsScriptSrcLoad));
      }
    } else if (script_characters_node_ != NULL) {
      StringPiece contents = script_characters_node_->contents();
      if (!contents.empty()) {
        GoogleString::size_type start_pos = 0;
        GoogleString::size_type pos, len;
        if (MatchSyncLoad(contents, pos, len)) {
          is_load_found_ = true;
          script_editors_.push_back(new ScriptEditor(
              script_element_, script_characters_node_, pos, len,
              ScriptEditor::kGaJsDocWriteLoad));
          start_pos = pos + len;
        }
        if (is_load_found_ && MatchSyncInit(contents, start_pos, pos, len)) {
          is_init_found_ = true;
          script_editors_.push_back(new ScriptEditor(
              script_element_, script_characters_node_, pos, len,
              ScriptEditor::kGaJsInit));
          start_pos = pos + len;
        }
        if (is_init_found_ && MatchUnhandledCalls(contents, start_pos)) {
          html_parse_->InfoHere("Reset: unhandled call.");
          ResetFilter();
          return;
        }
      }
    }
  }
}

void GoogleAnalyticsFilter::GetSyncToAsyncScript(GoogleString *buffer) const {
  buffer->clear();
  buffer->append(kGaSnippetPrefix);
  int last_index = glue_methods_->size() - 1;
  for (int i = 0; i <= last_index; i++) {
    buffer->append("        '");
    buffer->append(glue_methods_->at(i).as_string());
    if (i == last_index) {
      buffer->append("'\n");
    } else {
      buffer->append("',\n");
    }
  }
  buffer->append(kGaSnippetSuffix);
}

bool GoogleAnalyticsFilter::RewriteAsAsync() {
  if (!is_init_found_ || !is_load_found_) {
    return false;
  }
  ScriptEditor* first_editor = script_editors_[0];
  HtmlElement* first_script = first_editor->GetScriptElement();
  if (!html_parse_->IsRewritable(first_script)) {
    html_parse_->InfoHere("First script is not rewritable.");
    return false;
  }
  ScriptEditor::Type first_type = first_editor->GetType();
  CHECK(first_type == ScriptEditor::kGaJsScriptSrcLoad ||
        first_type == ScriptEditor::kGaJsDocWriteLoad);

  GoogleString replacement_script;
  for (int i = script_editors_.size() - 1; i > 0; --i) {
    ScriptEditor* editor = script_editors_[i];
    HtmlElement* script = editor->GetScriptElement();
    if (editor->GetType() == ScriptEditor::kGaJsScriptSrcLoad) {
      html_parse_->DeleteElement(script);
      html_parse_->InfoHere("Deleted script src load");
    } else if (editor->GetType() == ScriptEditor::kGaJsDocWriteLoad) {
      editor->NewContents("", &replacement_script);
      html_parse_->ReplaceNode(
          editor->GetScriptCharactersNode(),
          html_parse_->NewCharactersNode(script, replacement_script));
      html_parse_->InfoHere("Deleted document.write load");
    } else if (editor->GetType() == ScriptEditor::kGaJsInit) {
      editor->NewContents(kGaSnippetGetTracker, &replacement_script);
      html_parse_->ReplaceNode(
          editor->GetScriptCharactersNode(),
          html_parse_->NewCharactersNode(script, replacement_script));
      html_parse_->InfoHere("Replaced init");
    }
  }

  GoogleString glue_script;
  GetSyncToAsyncScript(&glue_script);
  if (first_type == ScriptEditor::kGaJsScriptSrcLoad) {
    html_parse_->PrependChild(
        first_script,
        html_parse_->NewCharactersNode(first_script, glue_script));
    first_script->DeleteAttribute(HtmlName::kSrc);
    html_parse_->InfoHere("Replaced script src load");
  } else {
    first_editor->NewContents(glue_script, &replacement_script);
    html_parse_->ReplaceNode(
        first_editor->GetScriptCharactersNode(),
        html_parse_->NewCharactersNode(first_script, replacement_script));
    html_parse_->InfoHere("Replaced document.write load");
  }
  return true;
}

}  // namespace net_instaweb
