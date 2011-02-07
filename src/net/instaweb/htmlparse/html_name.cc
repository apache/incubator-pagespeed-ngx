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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/htmlparse/public/html_name.h"

#include <algorithm>

namespace net_instaweb {

namespace {

struct CompareNameKeywordPair {
  bool operator()(const HtmlName::NameKeywordPair& a,
                  const HtmlName::NameKeywordPair& b) {
    return strcmp(a.name, b.name) < 0;
  }
};

// TODO(jmarantz): if this is too slow, consider gperf or dense_hash_map.
const HtmlName::NameKeywordPair kSortedPairs[] = {
  { HtmlName::kXml, "?xml" },   // First because '?' < 'a'
  { HtmlName::kA, "a" },
  { HtmlName::kAlt, "alt" },
  { HtmlName::kArea, "area" },
  { HtmlName::kAsync, "async" },
  { HtmlName::kAudio, "audio" },
  { HtmlName::kAutocomplete, "autocomplete" },
  { HtmlName::kAutofocus, "autofocus" },
  { HtmlName::kAutoplay, "autoplay" },
  { HtmlName::kBase, "base" },
  { HtmlName::kBody, "body" },
  { HtmlName::kBr, "br" },
  { HtmlName::kButton, "button" },
  { HtmlName::kChecked, "checked" },
  { HtmlName::kClass, "class" },
  { HtmlName::kCol, "col" },
  { HtmlName::kColgroup, "colgroup" },
  { HtmlName::kColspan, "colspan" },
  { HtmlName::kCommand, "command" },
  { HtmlName::kContent, "content" },
  { HtmlName::kControls, "controls" },
  { HtmlName::kDd, "dd" },
  { HtmlName::kDeclare, "declare" },
  { HtmlName::kDefaultchecked, "defaultchecked" },
  { HtmlName::kDefaultselected, "defaultselected" },
  { HtmlName::kDefer, "defer" },
  { HtmlName::kDetails, "details" },
  { HtmlName::kDisabled, "disabled" },
  { HtmlName::kDisplay, "display" },
  { HtmlName::kDiv, "div" },
  { HtmlName::kDt, "dt" },
  { HtmlName::kEnctype, "enctype" },
  { HtmlName::kEvent, "event" },
  { HtmlName::kFor, "for" },
  { HtmlName::kForm, "form" },
  { HtmlName::kFormnovalidate, "formnovalidate" },
  { HtmlName::kFrame, "frame" },
  { HtmlName::kFrameborder, "frameborder" },
  { HtmlName::kHead, "head" },
  { HtmlName::kHeight, "height" },
  { HtmlName::kHr, "hr" },
  { HtmlName::kHref, "href" },
  { HtmlName::kHtml, "html" },
  { HtmlName::kHttpEquiv, "http-equiv" },
  { HtmlName::kId, "id" },
  { HtmlName::kIframe, "iframe" },
  { HtmlName::kImg, "img" },
  { HtmlName::kIndeterminate, "indeterminate" },
  { HtmlName::kInput, "input" },
  { HtmlName::kIsmap, "ismap" },
  { HtmlName::kKeygen, "keygen" },
  { HtmlName::kKeytype, "keytype" },
  { HtmlName::kLanguage, "language" },
  { HtmlName::kLi, "li" },
  { HtmlName::kLink, "link" },
  { HtmlName::kLoop, "loop" },
  { HtmlName::kMedia, "media" },
  { HtmlName::kMenu, "menu" },
  { HtmlName::kMeta, "meta" },
  { HtmlName::kMethod, "method" },
  { HtmlName::kMultiple, "multiple" },
  { HtmlName::kMuted, "muted" },
  { HtmlName::kNohref, "nohref" },
  { HtmlName::kNoresize, "noresize" },
  { HtmlName::kNoscript, "noscript" },
  { HtmlName::kNovalidate, "novalidate" },
  { HtmlName::kObject, "object" },
  { HtmlName::kOl, "ol" },
  { HtmlName::kOnclick, "onclick" },
  { HtmlName::kOpen, "open" },
  { HtmlName::kOptgroup, "optgroup" },
  { HtmlName::kOption, "option" },
  { HtmlName::kOther, "other" },
  { HtmlName::kP, "p" },
  { HtmlName::kParam, "param" },
  { HtmlName::kPre, "pre" },
  { HtmlName::kReadonly, "readonly" },
  { HtmlName::kRel, "rel" },
  { HtmlName::kRequired, "required" },
  { HtmlName::kReversed, "reversed" },
  { HtmlName::kRowspan, "rowspan" },
  { HtmlName::kRp, "rp" },
  { HtmlName::kRt, "rt" },
  { HtmlName::kScoped, "scoped" },
  { HtmlName::kScript, "script" },
  { HtmlName::kScrolling, "scrolling" },
  { HtmlName::kSeamless, "seamless" },
  { HtmlName::kSelect, "select" },
  { HtmlName::kSelected, "selected" },
  { HtmlName::kShape, "shape" },
  { HtmlName::kSource, "source" },
  { HtmlName::kSpan, "span" },
  { HtmlName::kSrc, "src" },
  { HtmlName::kStyle, "style" },
  { HtmlName::kTag, "tag" },
  { HtmlName::kTbody, "tbody" },
  { HtmlName::kTd, "td" },
  { HtmlName::kTest, "test" },
  { HtmlName::kTextarea, "textarea" },
  { HtmlName::kTfoot, "tfoot" },
  { HtmlName::kTh, "th" },
  { HtmlName::kThead, "thead" },
  { HtmlName::kTr, "tr" },
  { HtmlName::kType, "type" },
  { HtmlName::kValuetype, "valuetype" },
  { HtmlName::kVideo, "video" },
  { HtmlName::kWbr, "wbr" },
  { HtmlName::kWidth, "width" },
  { HtmlName::kWrap, "wrap" },
  { HtmlName::kNotAKeyword, NULL},
};

// When compiling for debug, do a startup check to ensure that the names
// are in strcmp order.
#ifndef NDEBUG
class HtmlNameChecker {
 public:
  HtmlNameChecker() {
    int num_names = static_cast<int>(HtmlName::kNotAKeyword);
    CHECK_EQ(num_names, static_cast<int>(arraysize(kSortedPairs)) - 1);
    CompareNameKeywordPair checker;
    for (int i = 1; i < num_names; ++i) {
      CHECK(checker(kSortedPairs[i - 1], kSortedPairs[i]));
    }
  }
};
HtmlNameChecker startup_name_checker;
#endif

}  // namespace

const HtmlName::NameKeywordPair* HtmlName::sorted_pairs() {
  return kSortedPairs;
}

int HtmlName::num_sorted_pairs() {
  return arraysize(kSortedPairs) - 1;  // don't include "not a keyword"
}

HtmlName::Keyword HtmlName::Lookup(const StringPiece& name) {
  const NameKeywordPair* first = &kSortedPairs[0];
  const NameKeywordPair* last = &kSortedPairs[kNotAKeyword];
  NameKeywordPair trial;
  std::string name_str(name.data(), name.size());
  LowerString(&name_str);
  trial.name = name_str.c_str();
  const NameKeywordPair* found = std::lower_bound(
      first, last, trial, CompareNameKeywordPair());

  // binary_search just returns a bool, which is not useful.
  // lower_bound does not guarantee an exact match so we must test.
  if ((found->name != NULL) && (strcmp(found->name, trial.name) != 0)) {
    found = last;
  }
  return found->keyword;
}

}  // namespace net_instaweb
