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

#include "net/instaweb/rewriter/public/elide_attributes_filter.h"

#include <cstddef>
#include <map>
#include <utility>

#include "net/instaweb/htmlparse/public/doctype.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

// An attribute can be simplified if it is a "boolean attribute".
// See http://www.w3.org/TR/html5/common-microsyntaxes.html#boolean-attribute
// For example, <option selected="selected"> can become <option selected>.

struct TagAttr {
  HtmlName::Keyword tag_name;
  HtmlName::Keyword attr_name;
};

const TagAttr kBooleanAttrs[] = {
  // http://www.w3.org/TR/html4/struct/objects.html#h-13.6.1
  {HtmlName::kArea, HtmlName::kNohref},
  // http://www.w3.org/TR/html5/video.html#media-elements
  {HtmlName::kAudio, HtmlName::kAutoplay},
  {HtmlName::kAudio, HtmlName::kControls},
  {HtmlName::kAudio, HtmlName::kLoop},
  {HtmlName::kAudio, HtmlName::kMuted},
  // http://www.w3.org/TR/html5/the-button-element.html#the-button-element
  {HtmlName::kButton, HtmlName::kAutofocus},
  {HtmlName::kButton, HtmlName::kDisabled},
  // http://www.w3.org/TR/html5/interactive-elements.html#the-command
  {HtmlName::kCommand, HtmlName::kChecked},
  {HtmlName::kCommand, HtmlName::kDisabled},
  // http://www.w3.org/TR/html5/interactive-elements.html#the-details-element
  {HtmlName::kDetails, HtmlName::kOpen},
  // http://www.w3.org/TR/html5/association-of-controls-and-forms.html#
  // attributes-for-form-submission
  {HtmlName::kForm, HtmlName::kNovalidate},
  // http://www.w3.org/TR/html4/present/frames.html#h-16.2.2
  {HtmlName::kFrame, HtmlName::kNoresize},
  // http://www.w3.org/TR/html5/the-button-element.html#the-keygen-element
  {HtmlName::kKeygen, HtmlName::kAutofocus},
  {HtmlName::kKeygen, HtmlName::kDisabled},
  // http://www.w3.org/TR/html5/the-iframe-element.html#the-iframe-element
  {HtmlName::kIframe, HtmlName::kSeamless},
  // http://www.w3.org/TR/html5/embedded-content-1.html#the-img-element
  {HtmlName::kImg, HtmlName::kIsmap},
  // http://www.w3.org/TR/html5/the-input-element.html#the-input-element
  {HtmlName::kInput, HtmlName::kAutofocus},
  {HtmlName::kInput, HtmlName::kChecked},
  {HtmlName::kInput, HtmlName::kDefaultchecked},
  {HtmlName::kInput, HtmlName::kDisabled},
  {HtmlName::kInput, HtmlName::kFormnovalidate},
  {HtmlName::kInput, HtmlName::kIndeterminate},
  {HtmlName::kInput, HtmlName::kMultiple},
  {HtmlName::kInput, HtmlName::kReadonly},
  {HtmlName::kInput, HtmlName::kRequired},
  // http://www.w3.org/TR/html4/struct/objects.html#h-13.3
  {HtmlName::kObject, HtmlName::kDeclare},
  // http://www.w3.org/TR/html5/grouping-content.html#the-ol-element
  {HtmlName::kOl, HtmlName::kReversed},
  // http://www.w3.org/TR/html5/the-button-element.html#the-optgroup-element
  {HtmlName::kOptgroup, HtmlName::kDisabled},
  // http://www.w3.org/TR/html5/the-button-element.html#the-option-element
  {HtmlName::kOption, HtmlName::kDefaultselected},
  {HtmlName::kOption, HtmlName::kDisabled},
  {HtmlName::kOption, HtmlName::kSelected},
  // http://www.w3.org/TR/html5/scripting-1.html#script
  {HtmlName::kScript, HtmlName::kAsync},
  {HtmlName::kScript, HtmlName::kDefer},
  // http://www.w3.org/TR/html5/the-button-element.html#the-select-element
  {HtmlName::kSelect, HtmlName::kAutofocus},
  {HtmlName::kSelect, HtmlName::kDisabled},
  {HtmlName::kSelect, HtmlName::kMultiple},
  {HtmlName::kSelect, HtmlName::kRequired},
  // http://www.w3.org/TR/html5/semantics.html#the-style-element
  {HtmlName::kStyle, HtmlName::kScoped},
  // http://www.w3.org/TR/html5/the-button-element.html#the-textarea-element
  {HtmlName::kTextarea, HtmlName::kAutofocus},
  {HtmlName::kTextarea, HtmlName::kDisabled},
  {HtmlName::kTextarea, HtmlName::kReadonly},
  {HtmlName::kTextarea, HtmlName::kRequired},
  // http://www.w3.org/TR/html5/video.html#media-elements
  {HtmlName::kVideo, HtmlName::kAutoplay},
  {HtmlName::kVideo, HtmlName::kControls},
  {HtmlName::kVideo, HtmlName::kLoop},
  {HtmlName::kVideo, HtmlName::kMuted},
};

// An attribute can be removed from a tag if its name and value is in
// kDefaultList.
//
// Note: It is important that this list not include attributes that can be
// inherited.  Otherwise something like this could fail:
//   <div attr="non_default_value">
//     <div attr="default_value">   <!-- must not be elided -->
//     </div>
//   </div>

struct TagAttrValue {
  HtmlName::Keyword tag_name;
  HtmlName::Keyword attr_name;
  const char* attr_value;
  bool requires_version_5;  // Default value only exists in (X)HTML 5.
};

// References for HTML 4 and HTML 5 are included below, with extra notes for
// entries that apply differently to HTML 4 and HTML 5 (i.e. those with 4th
// argument true).  If you are so inclined, you are encouraged to carefully
// verify the references and make changes to any errors in this data.
const TagAttrValue kDefaultList[] = {
  // 4: http://www.w3.org/TR/html4/struct/links.html#h-12.2
  // 5: Note that the <a> tag's shape attribute is deprecated in HTML5.
  //    http://www.w3.org/TR/html5/obsolete.html#non-conforming-features
  {HtmlName::kA, HtmlName::kShape, "rect", false},
  // 4: http://www.w3.org/TR/html4/struct/objects.html#h-13.6.1
  // 5: http://www.w3.org/TR/html5/the-map-element.html#the-area-element
  {HtmlName::kArea, HtmlName::kShape, "rect", false},
  // 4: http://www.w3.org/TR/html4/interact/forms.html#h-17.5
  // 5: http://www.w3.org/TR/html5/the-button-element.html#the-button-element
  // IE does not support this default.
  // {HtmlName::kButton, HtmlName::kType, "submit", false},
  // 4: The <command> tag does not exist in HTML 4.
  // 5: http://www.w3.org/TR/html5/interactive-elements.html#the-command
  {HtmlName::kCommand, HtmlName::kType, "command", true},
  // 4: The <form> tag's autocomplete attribute does not exist in HTML 4.
  // 5: http://www.w3.org/TR/html5/forms.html#the-form-element
  {HtmlName::kForm, HtmlName::kAutocomplete, "on", true},
  // 4: http://www.w3.org/TR/html4/interact/forms.html#h-17.3
  // 5: http://www.w3.org/TR/html5/association-of-controls-and-forms.html#
  //    attributes-for-form-submission
  {HtmlName::kForm, HtmlName::kEnctype, "application/x-www-form-urlencoded",
   false},
  {HtmlName::kForm, HtmlName::kMethod, "get", false},
  // 4: http://www.w3.org/TR/html4/present/frames.html#h-16.2.2
  // 5: Note that the <frame> tag is deprecated in HTML5.
  //    http://www.w3.org/TR/html5/obsolete.html#non-conforming-features
  {HtmlName::kFrame, HtmlName::kFrameborder, "1", false},
  {HtmlName::kFrame, HtmlName::kScrolling, "auto", false},
  // 4: http://www.w3.org/TR/html4/present/frames.html#h-16.5
  // 5: Note that these attributes are deprecated in HTML5.
  //    http://www.w3.org/TR/html5/obsolete.html#non-conforming-features
  {HtmlName::kIframe, HtmlName::kFrameborder, "1", false},
  {HtmlName::kIframe, HtmlName::kScrolling, "auto", false},
  // 4: http://www.w3.org/TR/html4/interact/forms.html#h-17.4
  // 5: http://www.w3.org/TR/html5/the-input-element.html#the-input-element
  {HtmlName::kInput, HtmlName::kType, "text", false},
  // 4: The <keygen> tag does not exist in HTML 4.
  // 5: http://www.w3.org/TR/html5/the-button-element.html#the-keygen-element
  {HtmlName::kKeygen, HtmlName::kKeytype, "rsa", true},
  // 4: The <menu> tag seems to mean something different in HTML 4.
  // 5: http://www.w3.org/TR/html5/interactive-elements.html#menus
  {HtmlName::kMenu, HtmlName::kType, "list", true},
  // 4: http://www.w3.org/TR/html4/struct/objects.html#h-13.3.2
  // 5: Note that the <param> tag's valuetype attribute is deprecated in HTML5.
  //    http://www.w3.org/TR/html5/obsolete.html#non-conforming-features
  {HtmlName::kParam, HtmlName::kValuetype, "data", false},
  // 4: These attributes have no default values in HTML 4.
  //    http://www.w3.org/TR/html4/interact/scripts.html#h-18.2.1
  // 5: http://www.w3.org/TR/html5/scripting-1.html
  {HtmlName::kScript, HtmlName::kLanguage, "javascript", true},
  {HtmlName::kScript, HtmlName::kType, "text/javascript", true},
  // 4: The <source> tag does not exist in HTML 4.
  // 5: http://www.w3.org/TR/html5/video.html#the-source-element
  {HtmlName::kSource, HtmlName::kMedia, "all", true},
  // 4: This attribute has no default value in HTML 4.
  //    http://www.w3.org/TR/html4/present/styles.html#h-14.2.3
  // 5: http://www.w3.org/TR/html5/semantics.html#the-style-element
  {HtmlName::kStyle, HtmlName::kType, "text/css", true},
  // 4: This attributes has a _different_ default value in HTML 4!
  //    http://www.w3.org/TR/html4/present/styles.html#h-14.2.3
  // 5: http://www.w3.org/TR/html5/semantics.html#the-style-element
  {HtmlName::kStyle, HtmlName::kMedia, "all", true},
  // 4: The <textarea> tag's wrap attribute does not exist in HTML 4.
  // 5: http://www.w3.org/TR/html5/the-button-element.html#the-textarea-element
  {HtmlName::kTextarea, HtmlName::kWrap, "soft", true},
  // 4: http://www.w3.org/TR/html4/struct/tables.html
  // 5: http://www.w3.org/TR/html5/tabular-data.html#table-model
  {HtmlName::kCol, HtmlName::kSpan, "1", false},
  {HtmlName::kColgroup, HtmlName::kSpan, "1", false},
  {HtmlName::kTd, HtmlName::kColspan, "1", false},
  {HtmlName::kTd, HtmlName::kRowspan, "1", false},
  {HtmlName::kTh, HtmlName::kColspan, "1", false},
  {HtmlName::kTh, HtmlName::kRowspan, "1", false},
};

}  // namespace

ElideAttributesFilter::ElideAttributesFilter(HtmlParse* html_parse)
    : html_parse_(html_parse) {
  // Populate one_value_attrs_map_
  for (size_t i = 0; i < arraysize(kBooleanAttrs); ++i) {
    const TagAttr& entry = kBooleanAttrs[i];
    KeywordSet& keywords = one_value_attrs_map_[entry.tag_name];
    keywords.insert(entry.attr_name);
  }
  // Populate default_value_map_
  for (size_t i = 0; i < arraysize(kDefaultList); ++i) {
    const TagAttrValue& entry = kDefaultList[i];
    AttrValue& value = default_value_map_[entry.tag_name][entry.attr_name];
    value.attr_value = entry.attr_value;
    value.requires_version_5 = entry.requires_version_5;
  }
}

void ElideAttributesFilter::StartElement(HtmlElement* element) {
  const DocType& doctype = html_parse_->doctype();

  if (!doctype.IsXhtml()) {
    // Check for boolean attributes.
    KeywordSetMap::const_iterator iter =
        one_value_attrs_map_.find(element->keyword());
    if (iter != one_value_attrs_map_.end()) {
      const KeywordSet& oneValueAttrs = iter->second;
      for (int i = 0, end = element->attribute_size(); i < end; ++i) {
        HtmlElement::Attribute& attribute = element->attribute(i);
        if (attribute.value() != NULL &&
            oneValueAttrs.count(attribute.keyword()) > 0) {
          attribute.SetValue(NULL);
        }
      }
    }
  }

  // Check for attributes with default values.
  ValueMapMap::const_iterator iter1 = default_value_map_.find(
      element->keyword());
  if (iter1 != default_value_map_.end()) {
    const ValueMap& default_values = iter1->second;
    for (int i = 0; i < element->attribute_size(); ++i) {
      HtmlElement::Attribute& attribute = element->attribute(i);
      if (attribute.value() != NULL) {
        ValueMap::const_iterator iter2 = default_values.find(
            attribute.keyword());
        if (iter2 != default_values.end()) {
          const AttrValue& value = iter2->second;
          if ((!value.requires_version_5 || doctype.IsVersion5()) &&
              StringCaseEqual(attribute.value(), value.attr_value)) {
            element->DeleteAttribute(i);
            --i;
          }
        }
      }
    }
  }
}

}  // namespace net_instaweb
