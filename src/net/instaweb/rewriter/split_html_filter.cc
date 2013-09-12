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

// Author: rahulbansal@google.com (Rahul Bansal)

#include "net/instaweb/rewriter/public/split_html_filter.h"

#include <memory>
#include <utility>
#include <vector>

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/critical_line_info.pb.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/split_html_beacon_filter.h"
#include "net/instaweb/rewriter/public/split_html_config.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/escaping.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/json_writer.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"
#include "pagespeed/kernel/base/fast_wildcard_group.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"

namespace net_instaweb {

// TODO(rahulbansal): We are sending an extra close body and close html tag.
// Fix that.
const char SplitHtmlFilter::kSplitSuffixJsFormatString[] =
    "<script type=\"text/javascript\" src=\"%s\"></script>"
    "<script type=\"text/javascript\">"
      "%s"
      "pagespeed.lastScriptIndexBeforePanelStub=%d;"
      "pagespeed.panelLoaderInit();"
      "pagespeed.panelLoader.bufferNonCriticalData(%s, %s);"
    "</script>\n</body></html>\n";

const char SplitHtmlFilter::kSplitTwoChunkSuffixJsFormatString[] =
    "<script type=\"text/javascript\">"
    "if(document.body.scrollTop==0) {"
    "  scrollTo(0, 1);"
    "}"
    "function loadXMLDoc(should_load) {"
    "\n  if (!should_load) {"
    "\n    pagespeed['split_non_critical'] = {};"
    "\n    return;"
    "\n  }"
    "\n  var url=window.location.toString();"
    "\n  url=url.replace('x_split=atf', 'x_split=btf');"
    "\n  var xmlhttp;"
    "\n  if (window.XMLHttpRequest) {"
    "\n     xmlhttp=new XMLHttpRequest();"
    "\n  } else {"
    "\n     xmlhttp=new ActiveXObject(\"Microsoft.XMLHTTP\");"
    "\n  }"
    "\n  xmlhttp.onreadystatechange = function() {"
    "\n  if (xmlhttp.readyState==4 && xmlhttp.status==200) {"
    "\n    var t = JSON.parse(xmlhttp.responseText);"
    "\n    if (pagespeed.panelLoader) {"
    "\n      pagespeed.panelLoader.bufferNonCriticalData(t, false);"
    "\n    } else { "
    "\n      pagespeed['split_non_critical'] = t; }"
    "\n    }"
    "\n  }"
    "\n  xmlhttp.open(\"GET\",url,true);"
    "\n  xmlhttp.setRequestHeader('%s', '%s');"
    "\n  xmlhttp.send();"
    "\n}"
    "function loadBTF() {"
    "  if(window.psa_btf_loaded) return;"
    "  window.psa_btf_loaded=true;"
    "  loadXMLDoc(\"%s\");"
    "  %s"
    "  var blink_js = document.createElement('script');"
    "  blink_js.src=\"%s\";"
    "  blink_js.setAttribute('onload', \""
    "    pagespeed.lastScriptIndexBeforePanelStub=%d;"
    "    pagespeed.panelLoaderInit();"
    "    if (pagespeed['split_non_critical']) {"
    "      pagespeed.panelLoader.bufferNonCriticalData("
    "          pagespeed['split_non_critical'], false);"
    "    }\");"
    "  document.body.appendChild(blink_js);"
    "}"
    "window.setTimeout(loadBTF, 300);"
    "if (window.addEventListener) {"
    "  window.addEventListener('load', function() {"
    "      window.setTimeout(loadBTF,0);}, false);"
    "} else {"
    "  window.attachEvent('onload', function() {"
    "      window.setTimeout(loadBTF,0);});"
    "}"
    "</script>\n"
    "</body></html>\n";

const char SplitHtmlFilter::kLoadHiResImages[] =
    "function psa_replace_high_res_for_tag(str) {"
     "var images=document.getElementsByTagName(str);"
     "for (var i=0;i<images.length;++i) {"
      "var high_res_src=images[i].getAttribute('pagespeed_high_res_src');"
      "var src=images[i].getAttribute('src');"
      "if (high_res_src && src != high_res_src && src.indexOf('data:') != -1){"
        "images[i].src=high_res_src;"
      "}"
     "}"
    "};"
    "psa_replace_high_res_for_tag('img');"
    "psa_replace_high_res_for_tag('input');";

const char SplitHtmlFilter::kMetaReferer[] =
    "<meta name=\"referrer\" content=\"never\">";

// At StartElement, if element is panel instance push a new json to capture
// contents of instance to the json stack.
// All the emitBytes are captured into the top json until a new panel
// instance is found or the current panel instance ends.
SplitHtmlFilter::SplitHtmlFilter(RewriteDriver* rewrite_driver)
    : SuppressPreheadFilter(rewrite_driver),
      rewrite_driver_(rewrite_driver),
      config_(NULL),
      options_(rewrite_driver->options()),
      disable_filter_(true),
      last_script_index_before_panel_stub_(-1),
      panel_seen_(false),
      static_asset_manager_(NULL),
      script_tag_scanner_(rewrite_driver) {
}

SplitHtmlFilter::~SplitHtmlFilter() {
}

bool SplitHtmlFilter::IsAllowedCrossDomainRequest(StringPiece cross_origin) {
  FastWildcardGroup wildcards;
  if (!cross_origin.empty()) {
    StringPieceVector allowed_cross_origins;
    SplitStringPieceToVector(options_->access_control_allow_origins(), ", ",
                             &allowed_cross_origins, true);
    for (int i = 0, n = allowed_cross_origins.size(); i < n; ++i) {
      wildcards.Allow(allowed_cross_origins[i]);
    }
  }
  return wildcards.Match(cross_origin, false);
}

void SplitHtmlFilter::DetermineEnabled() {
  config_ = rewrite_driver_->split_html_config();
  disable_filter_ = !rewrite_driver_->request_properties()->SupportsSplitHtml(
      rewrite_driver_->options()->enable_aggressive_rewriters_for_mobile()) ||
      SplitHtmlBeaconFilter::ShouldApply(rewrite_driver_) ||
      // Disable this filter if a two chunked response is requested and we have
      // no critical line info.
      (config_->critical_line_info() == NULL &&
       options_->serve_split_html_in_two_chunks());
  if (!disable_filter_ &&
      rewrite_driver_->request_context()->split_request_type() ==
      RequestContext::SPLIT_ABOVE_THE_FOLD) {
    rewrite_driver_->set_defer_instrumentation_script(true);
  }
  // Always enable this filter since it is a writer filter.
  set_is_enabled(true);
}

void SplitHtmlFilter::StartDocument() {
  element_json_stack_.clear();
  panel_seen_ = false;
  last_script_index_before_panel_stub_ = -1;

  state_.reset(new SplitHtmlState(config_));

  flush_head_enabled_ = options_->Enabled(RewriteOptions::kFlushSubresources);
  static_asset_manager_ =
      rewrite_driver_->server_context()->static_asset_manager();
  if (disable_filter_) {
    InvokeBaseHtmlFilterStartDocument();
    return;
  }

  original_writer_ = rewrite_driver_->writer();
  // TODO(nikhilmadan): RewriteOptions::serve_split_html_in_two_chunks is
  // currently incompatible with cache html. Fix this.
  serve_response_in_two_chunks_ = options_->serve_split_html_in_two_chunks()
      && !disable_filter_ &&
      rewrite_driver_->request_context()->split_request_type() !=
      RequestContext::SPLIT_FULL;
  if (serve_response_in_two_chunks_) {
    ResponseHeaders* response_headers =
        rewrite_driver_->mutable_response_headers();
    if (rewrite_driver_->request_context()->split_request_type() ==
        RequestContext::SPLIT_BELOW_THE_FOLD) {
      flush_head_enabled_ = false;
      original_writer_ = &null_writer_;
      set_writer(&null_writer_);
    } else if (options_->max_html_cache_time_ms() > 0) {
      // If max html cache time is > 0, set the cache time for the ATF chunk
      // accordingly. Also, mark the html as private, and strip the pragma and
      // age headers.
      response_headers->ComputeCaching();
      response_headers->SetDateAndCaching(
          response_headers->date_ms(), options_->max_html_cache_time_ms(),
          ", private");
      response_headers->RemoveAll(HttpAttributes::kAge);
      response_headers->RemoveAll(HttpAttributes::kPragma);
      response_headers->ComputeCaching();
    }
    if (rewrite_driver_->request_context()->split_request_type() !=
        RequestContext::SPLIT_BELOW_THE_FOLD &&
        options_->serve_xhr_access_control_headers()) {
      const RequestHeaders* request_headers =
          rewrite_driver_->request_headers();
      if (request_headers != NULL) {
        // Origin header should be seen if it is a cross-origin request.
        StringPiece cross_origin =
            request_headers->Lookup1(HttpAttributes::kOrigin);
        if (IsAllowedCrossDomainRequest(cross_origin)) {
          response_headers->Add(HttpAttributes::kAccessControlAllowOrigin,
                                cross_origin);
          response_headers->Add(HttpAttributes::kAccessControlAllowCredentials,
                                "true");
        }
      }
    }
  }
  json_writer_.reset(new JsonWriter(original_writer_,
                                    &element_json_stack_));
  url_ = rewrite_driver_->google_url().Spec();
  script_written_ = false;
  inside_pagespeed_no_defer_script_ = false;

  // Push the base panel.
  StartPanelInstance(static_cast<HtmlElement*>(NULL));
  // StartPanelInstance sets the json writer. For the base panel, we don't want
  // the writer to be set.
  set_writer(original_writer_);

  InvokeBaseHtmlFilterStartDocument();
}

void SplitHtmlFilter::EndDocument() {
  InvokeBaseHtmlFilterEndDocument();

  if (disable_filter_) {
    return;
  }

  // Remove critical html since it should already have been sent out by now.
  element_json_stack_[0].second->removeMember(BlinkUtil::kInstanceHtml);

  Json::Value json = Json::arrayValue;
  json.append(*(element_json_stack_[0].second));

  ServeNonCriticalPanelContents(json[0]);
  delete element_json_stack_[0].second;
  element_json_stack_.pop_back();
}

void SplitHtmlFilter::WriteString(const StringPiece& str) {
  rewrite_driver_->writer()->Write(str, rewrite_driver_->message_handler());
}

void SplitHtmlFilter::ServeNonCriticalPanelContents(const Json::Value& json) {
  if (!serve_response_in_two_chunks_ ||
      rewrite_driver_->request_context()->split_request_type() ==
      RequestContext::SPLIT_BELOW_THE_FOLD) {
    GoogleString non_critical_json = fast_writer_.write(json);
    BlinkUtil::StripTrailingNewline(&non_critical_json);
    BlinkUtil::EscapeString(&non_critical_json);
    if (!serve_response_in_two_chunks_) {
      WriteString(StringPrintf(
          kSplitSuffixJsFormatString,
          GetBlinkJsUrl(options_, static_asset_manager_).c_str(),
          kLoadHiResImages,
          last_script_index_before_panel_stub_,
          non_critical_json.c_str(),
          rewrite_driver_->flushing_cached_html() ? "true" : "false"));
    } else {
      WriteString(non_critical_json);
    }
    if (!json.empty()) {
      rewrite_driver_->log_record()->SetRewriterLoggingStatus(
          RewriteOptions::FilterId(RewriteOptions::kSplitHtml),
          RewriterApplication::APPLIED_OK);
      ScopedMutex lock(rewrite_driver_->log_record()->mutex());
      rewrite_driver_->log_record()->logging_info()->mutable_split_html_info()
          ->set_json_size(non_critical_json.size());
    }
  } else {
    scoped_ptr<GoogleUrl> gurl(
        rewrite_driver_->google_url().CopyAndAddQueryParam(
            HttpAttributes::kXSplit, HttpAttributes::kXSplitBelowTheFold));
    GoogleString escaped_url;
    EscapeToJsStringLiteral(gurl->PathAndLeaf(), false, &escaped_url);
    WriteString(StringPrintf(
        kSplitTwoChunkSuffixJsFormatString,
        HttpAttributes::kXPsaSplitConfig,
        GenerateCriticalLineConfigString().c_str(),
        json.empty() ? "" : "1",
        kLoadHiResImages,
        GetBlinkJsUrl(options_, static_asset_manager_).c_str(),
        last_script_index_before_panel_stub_));
  }
  HtmlWriterFilter::Flush();
}

GoogleString SplitHtmlFilter::GenerateCriticalLineConfigString() {
  GoogleString out;
  for (int i = 0; i < config_->critical_line_info()->panels_size(); ++i) {
    const Panel& panel = config_->critical_line_info()->panels(i);
    StrAppend(&out, panel.start_xpath());
    if (panel.has_end_marker_xpath()) {
      StrAppend(&out, ":", panel.end_marker_xpath());
    }
    StrAppend(&out, ",");
  }
  return out;
}

void SplitHtmlFilter::EndPanelInstance() {
  json_writer_->UpdateDictionary();

  ElementJsonPair element_json_pair = element_json_stack_.back();
  scoped_ptr<Json::Value> dictionary(element_json_pair.second);
  element_json_stack_.pop_back();
  Json::Value* parent_dictionary = element_json_stack_.back().second;
  AppendJsonData(&((*parent_dictionary)[state_->current_panel_id()]),
                 *dictionary);
  state_->set_current_panel_parent_element(NULL);
  state_->set_current_panel_id("");
  set_writer(original_writer_);
}

void SplitHtmlFilter::StartPanelInstance(HtmlElement* element) {
  if (element_json_stack_.size() != 0) {
    json_writer_->UpdateDictionary();
  }

  Json::Value* new_json = new Json::Value(Json::objectValue);
  // Push new Json
  element_json_stack_.push_back(std::make_pair(element, new_json));
  if (element != NULL) {
    panel_seen_ = true;
    state_->set_current_panel_parent_element(element->parent());
    state_->set_current_panel_id(GetPanelIdForInstance(element));
  }
  if (!serve_response_in_two_chunks_ ||
      rewrite_driver_->request_context()->split_request_type() !=
      RequestContext::SPLIT_BELOW_THE_FOLD) {
    original_writer_ = rewrite_driver_->writer();
  }
  set_writer(json_writer_.get());
}

void SplitHtmlFilter::InsertPanelStub(HtmlElement* element,
                                      const GoogleString& panel_id) {
  HtmlCommentNode* comment = rewrite_driver_->NewCommentNode(
      element->parent(),
      StrCat(RewriteOptions::kPanelCommentPrefix, " begin ", panel_id));
  rewrite_driver_->InsertNodeBeforeCurrent(comment);
  Comment(comment);
  // Append end stub to json.
  comment = rewrite_driver_->NewCommentNode(
      element->parent(),
      StrCat(RewriteOptions::kPanelCommentPrefix, " end ", panel_id));
  rewrite_driver_->InsertNodeBeforeCurrent(comment);
  Comment(comment);
}

void SplitHtmlFilter::InsertSplitInitScripts(HtmlElement* element) {
  // TODO(rahulbansal): Enable AddHead filter and this code can be made simpler.
  bool include_head = (element->keyword() != HtmlName::kHead);
  GoogleString defer_js_with_blink = "";
  if (include_head) {
    StrAppend(&defer_js_with_blink, "<head>");
    if (options_->hide_referer_using_meta()) {
      StrAppend(&defer_js_with_blink, kMetaReferer);
    }
  }

  if (options_->serve_ghost_click_buster_with_split_html()) {
    StrAppend(&defer_js_with_blink, "<script type=\"text/javascript\">");
    StringPiece ghost_click_buster_js =
        static_asset_manager_->GetAsset(StaticAssetManager::kGhostClickBusterJs,
                                        options_);
    StrAppend(&defer_js_with_blink, ghost_click_buster_js);
    StrAppend(&defer_js_with_blink, "</script>");
  }
  if (include_head) {
    StrAppend(&defer_js_with_blink, "</head>");
  }

  HtmlCharactersNode* blink_script_node = rewrite_driver_->NewCharactersNode(
      element, defer_js_with_blink);
  Characters(blink_script_node);
  script_written_ = true;
}

void SplitHtmlFilter::StartElement(HtmlElement* element) {
  if (disable_filter_) {
    InvokeBaseHtmlFilterStartElement(element);
    return;
  }

  if (!panel_seen_ &&
      element->keyword() == HtmlName::kScript) {
    // Store the script index before panel stub for ATF script execution.
    HtmlElement::Attribute* script_index_attr =
        element->FindAttribute(HtmlName::kOrigIndex);
    if (script_index_attr != NULL) {
      StringPiece script_index_str = script_index_attr->DecodedValueOrNull();
      int script_index = -1;
      if (!script_index_str.empty() &&
          StringToInt(script_index_str, &script_index)) {
        last_script_index_before_panel_stub_ = script_index;
      }
    }
  }
  if (element->FindAttribute(HtmlName::kPagespeedNoDefer) &&
      element_json_stack_.size() > 1 ) {
    HtmlElement::Attribute* src = NULL;
    if (script_tag_scanner_.ParseScriptElement(element, &src) ==
        ScriptTagScanner::kJavaScript) {
      set_writer(original_writer_);
      inside_pagespeed_no_defer_script_ = true;
      InvokeBaseHtmlFilterStartElement(element);
      return;
    }
  }

  state_->UpdateNumChildrenStack(element);

  if (element->keyword() == HtmlName::kBody && !script_written_) {
    InsertSplitInitScripts(element);
  }

  if (state_->IsEndMarkerForCurrentPanel(element)) {
    EndPanelInstance();
  }

  if (state_->current_panel_id().empty()) {
    GoogleString panel_id = state_->MatchPanelIdForElement(element);
    // if panel_id is empty, then element didn't match with any start xpath of
    // panel specs
    if (!panel_id.empty()) {
      InsertPanelStub(element, panel_id);
      MarkElementWithPanelId(element, panel_id);
      StartPanelInstance(element);
    }
  } else if (state_->IsElementSiblingOfCurrentPanel(element)) {
    MarkElementWithPanelId(element, state_->current_panel_id());
  }
  if (element_json_stack_.size() > 1) {
    // Suppress these bytes since they belong to a panel.
    HtmlWriterFilter::StartElement(element);
  } else {
    if (element->keyword() == HtmlName::kImg ||
        element->keyword() == HtmlName::kInput) {
      HtmlElement::Attribute* pagespeed_high_res_src_attr =
          element->FindAttribute(HtmlName::kPagespeedHighResSrc);
      HtmlElement::Attribute* onload =
          element->FindAttribute(HtmlName::kOnload);
      if (pagespeed_high_res_src_attr != NULL &&
          pagespeed_high_res_src_attr->DecodedValueOrNull() != NULL &&
          onload != NULL && onload->DecodedValueOrNull() != NULL) {
        element->DeleteAttribute(HtmlName::kOnload);
      }
    }
    InvokeBaseHtmlFilterStartElement(element);
    if (element->keyword() == HtmlName::kHead) {
      // Add meta referer.
      if (options_->hide_referer_using_meta()) {
        HtmlCharactersNode* meta_node =
            rewrite_driver_->NewCharactersNode(element, kMetaReferer);
        Characters(meta_node);
      }
    }
  }
}

void SplitHtmlFilter::EndElement(HtmlElement* element) {
  if (disable_filter_) {
    InvokeBaseHtmlFilterEndElement(element);
    return;
  }

  if (inside_pagespeed_no_defer_script_) {
    InvokeBaseHtmlFilterEndElement(element);
    set_writer(json_writer_.get());
    inside_pagespeed_no_defer_script_ = false;
    return;
  }

  if (!state_->num_children_stack()->empty()) {
    state_->num_children_stack()->pop_back();
  }
  if (state_->IsElementParentOfCurrentPanel(element) ||
      (element->parent() == NULL &&
       element_json_stack_.back().first == element)) {
    EndPanelInstance();
  }

  if (element->keyword() == HtmlName::kHead && !script_written_) {
    InsertSplitInitScripts(element);
  }

  if (element_json_stack_.size() > 1) {
    // Suppress these bytes since they belong to a panel.
    HtmlWriterFilter::EndElement(element);
  } else {
    InvokeBaseHtmlFilterEndElement(element);
  }
}

void SplitHtmlFilter::AppendJsonData(Json::Value* dictionary,
                                 const Json::Value& dict) {
  if (!dictionary->isArray()) {
    *dictionary = Json::arrayValue;
  }
  dictionary->append(dict);
}

void SplitHtmlFilter::MarkElementWithPanelId(HtmlElement* element,
                                         const GoogleString& panel_id) {
  element->AddAttribute(rewrite_driver_->MakeName(BlinkUtil::kPanelId),
                        panel_id, HtmlElement::DOUBLE_QUOTE);
}

GoogleString SplitHtmlFilter::GetPanelIdForInstance(HtmlElement* element) {
  GoogleString panel_id_value;
  StringPiece panel_id_attr_name = BlinkUtil::kPanelId;
  const HtmlElement::AttributeList& attrs = element->attributes();
  for (HtmlElement::AttributeConstIterator i(attrs.begin());
         i != attrs.end(); ++i) {
      const HtmlElement::Attribute& attribute = *i;
    if ((panel_id_attr_name == attribute.name_str()) &&
        (attribute.DecodedValueOrNull() != NULL)) {
      panel_id_value = attribute.DecodedValueOrNull();
      break;
    }
  }
  return panel_id_value;
}

const GoogleString& SplitHtmlFilter::GetBlinkJsUrl(
      const RewriteOptions* options,
      StaticAssetManager* static_asset_manager) {
  return static_asset_manager->GetAssetUrl(StaticAssetManager::kBlinkJs,
                                           options);
}

// TODO(rahulbansal): Refactor this pattern.
void SplitHtmlFilter::InvokeBaseHtmlFilterStartDocument() {
  if (flush_head_enabled_) {
    SuppressPreheadFilter::StartDocument();
  } else {
    HtmlWriterFilter::StartDocument();
  }
}

void SplitHtmlFilter::InvokeBaseHtmlFilterStartElement(HtmlElement* element) {
  if (flush_head_enabled_) {
    SuppressPreheadFilter::StartElement(element);
  } else {
    HtmlWriterFilter::StartElement(element);
  }
}

void SplitHtmlFilter::InvokeBaseHtmlFilterEndElement(HtmlElement* element) {
  if (flush_head_enabled_) {
    SuppressPreheadFilter::EndElement(element);
  } else {
    HtmlWriterFilter::EndElement(element);
  }
}

void SplitHtmlFilter::InvokeBaseHtmlFilterEndDocument() {
  if (flush_head_enabled_) {
    SuppressPreheadFilter::EndDocument();
  } else {
    HtmlWriterFilter::EndDocument();
  }
}

}  // namespace net_instaweb
