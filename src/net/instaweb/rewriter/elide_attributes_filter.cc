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

#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace {

// An attribute can be simplified if it is a "boolean attribute".
// See http://www.w3.org/TR/html5/common-microsyntaxes.html#boolean-attribute
// For example, <option selected="selected"> can become <option selected>.

struct TagAttr {
  const char* tag_name;
  const char* attr_name;
};

const TagAttr kBooleanAttrs[] = {
  // http://www.w3.org/TR/html4/struct/objects.html#h-13.6.1
  {"area", "nohref"},
  // http://www.w3.org/TR/html5/video.html#media-elements
  {"audio", "autoplay"},
  {"audio", "controls"},
  {"audio", "loop"},
  {"audio", "muted"},
  // http://www.w3.org/TR/html5/the-button-element.html#the-button-element
  {"button", "autofocus"},
  {"button", "disabled"},
  // http://www.w3.org/TR/html5/interactive-elements.html#the-command
  {"command", "checked"},
  {"command", "disabled"},
  // http://www.w3.org/TR/html5/interactive-elements.html#the-details-element
  {"details", "open"},
  // http://www.w3.org/TR/html5/association-of-controls-and-forms.html#
  // attributes-for-form-submission
  {"form", "novalidate"},
  // http://www.w3.org/TR/html4/present/frames.html#h-16.2.2
  {"frame", "noresize"},
  // http://www.w3.org/TR/html5/the-button-element.html#the-keygen-element
  {"keygen", "autofocus"},
  {"keygen", "disabled"},
  // http://www.w3.org/TR/html5/the-iframe-element.html#the-iframe-element
  {"iframe", "seamless"},
  // http://www.w3.org/TR/html5/embedded-content-1.html#the-img-element
  {"img", "ismap"},
  // http://www.w3.org/TR/html5/the-input-element.html#the-input-element
  {"input", "autofocus"},
  {"input", "checked"},
  {"input", "defaultchecked"},
  {"input", "disabled"},
  {"input", "formnovalidate"},
  {"input", "indeterminate"},
  {"input", "multiple"},
  {"input", "readonly"},
  {"input", "required"},
  // http://www.w3.org/TR/html4/struct/objects.html#h-13.3
  {"object", "declare"},
  // http://www.w3.org/TR/html5/grouping-content.html#the-ol-element
  {"ol", "reversed"},
  // http://www.w3.org/TR/html5/the-button-element.html#the-optgroup-element
  {"optgroup", "disabled"},
  // http://www.w3.org/TR/html5/the-button-element.html#the-option-element
  {"option", "defaultselected"},
  {"option", "disabled"},
  {"option", "selected"},
  // http://www.w3.org/TR/html5/scripting-1.html#script
  {"script", "async"},
  {"script", "defer"},
  // http://www.w3.org/TR/html5/the-button-element.html#the-select-element
  {"select", "autofocus"},
  {"select", "disabled"},
  {"select", "multiple"},
  {"select", "required"},
  // http://www.w3.org/TR/html5/semantics.html#the-style-element
  {"style", "scoped"},
  // http://www.w3.org/TR/html5/the-button-element.html#the-textarea-element
  {"textarea", "autofocus"},
  {"textarea", "disabled"},
  {"textarea", "readonly"},
  {"textarea", "required"},
  // http://www.w3.org/TR/html5/video.html#media-elements
  {"video", "autoplay"},
  {"video", "controls"},
  {"video", "loop"},
  {"video", "muted"},
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
  const char* tag_name;
  const char* attr_name;
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
  {"a", "shape", "rect", false},
  // 4: http://www.w3.org/TR/html4/struct/objects.html#h-13.6.1
  // 5: http://www.w3.org/TR/html5/the-map-element.html#the-area-element
  {"area", "shape", "rect", false},
  // 4: http://www.w3.org/TR/html4/interact/forms.html#h-17.5
  // 5: http://www.w3.org/TR/html5/the-button-element.html#the-button-element
  // IE does not support this default.
  //{"button", "type", "submit", false},
  // 4: The <command> tag does not exist in HTML 4.
  // 5: http://www.w3.org/TR/html5/interactive-elements.html#the-command
  {"command", "type", "command", true},
  // 4: The <form> tag's autocomplete attribute does not exist in HTML 4.
  // 5: http://www.w3.org/TR/html5/forms.html#the-form-element
  {"form", "autocomplete", "on", true},
  // 4: http://www.w3.org/TR/html4/interact/forms.html#h-17.3
  // 5: http://www.w3.org/TR/html5/association-of-controls-and-forms.html#
  //    attributes-for-form-submission
  {"form", "enctype", "application/x-www-form-urlencoded", false},
  {"form", "method", "get", false},
  // 4: http://www.w3.org/TR/html4/present/frames.html#h-16.2.2
  // 5: Note that the <frame> tag is deprecated in HTML5.
  //    http://www.w3.org/TR/html5/obsolete.html#non-conforming-features
  {"frame", "frameborder", "1", false},
  {"frame", "scrolling", "auto", false},
  // 4: http://www.w3.org/TR/html4/present/frames.html#h-16.5
  // 5: Note that these attributes are deprecated in HTML5.
  //    http://www.w3.org/TR/html5/obsolete.html#non-conforming-features
  {"iframe", "frameborder", "1", false},
  {"iframe", "scrolling", "auto", false},
  // 4: http://www.w3.org/TR/html4/interact/forms.html#h-17.4
  // 5: http://www.w3.org/TR/html5/the-input-element.html#the-input-element
  {"input", "type", "text", false},
  // 4: The <keygen> tag does not exist in HTML 4.
  // 5: http://www.w3.org/TR/html5/the-button-element.html#the-keygen-element
  {"keygen", "keytype", "rsa", true},
  // 4: The <menu> tag seems to mean something different in HTML 4.
  // 5: http://www.w3.org/TR/html5/interactive-elements.html#menus
  {"menu", "type", "list", true},
  // 4: http://www.w3.org/TR/html4/struct/objects.html#h-13.3.2
  // 5: Note that the <param> tag's valuetype attribute is deprecated in HTML5.
  //    http://www.w3.org/TR/html5/obsolete.html#non-conforming-features
  {"param", "valuetype", "data", false},
  // 4: These attributes have no default values in HTML 4.
  //    http://www.w3.org/TR/html4/interact/scripts.html#h-18.2.1
  // 5: http://www.w3.org/TR/html5/scripting-1.html
  {"script", "language", "javascript", true},
  {"script", "type", "text/javascript", true},
  // 4: The <source> tag does not exist in HTML 4.
  // 5: http://www.w3.org/TR/html5/video.html#the-source-element
  {"source", "media", "all", true},
  // 4: This attribute has no default value in HTML 4.
  //    http://www.w3.org/TR/html4/present/styles.html#h-14.2.3
  // 5: http://www.w3.org/TR/html5/semantics.html#the-style-element
  {"style", "type", "text/css", true},
  // 4: This attributes has a _different_ default value in HTML 4!
  //    http://www.w3.org/TR/html4/present/styles.html#h-14.2.3
  // 5: http://www.w3.org/TR/html5/semantics.html#the-style-element
  {"style", "media", "all", true},
  // 4: The <textarea> tag's wrap attribute does not exist in HTML 4.
  // 5: http://www.w3.org/TR/html5/the-button-element.html#the-textarea-element
  {"textarea", "wrap", "soft", true},
  // 4: http://www.w3.org/TR/html4/struct/tables.html
  // 5: http://www.w3.org/TR/html5/tabular-data.html#table-model
  {"col", "span", "1", false},
  {"colgroup", "span", "1", false},
  {"td", "colspan", "1", false},
  {"td", "rowspan", "1", false},
  {"th", "colspan", "1", false},
  {"th", "rowspan", "1", false},
};

}  // namespace

namespace net_instaweb {

ElideAttributesFilter::ElideAttributesFilter(HtmlParse* html_parse)
    : html_parse_(html_parse) {
  // Populate one_value_attrs_map_
  for (size_t i = 0; i < arraysize(kBooleanAttrs); ++i) {
    const TagAttr& entry = kBooleanAttrs[i];
    one_value_attrs_map_[html_parse->Intern(entry.tag_name)].insert(
        html_parse->Intern(entry.attr_name));
  }
  // Populate default_value_map_
  for (size_t i = 0; i < arraysize(kDefaultList); ++i) {
    const TagAttrValue& entry = kDefaultList[i];
    AttrValue& value = default_value_map_[html_parse->Intern(entry.tag_name)]
                                         [html_parse->Intern(entry.attr_name)];
    value.attr_value = entry.attr_value;
    value.requires_version_5 = entry.requires_version_5;
  }
}

void ElideAttributesFilter::StartElement(HtmlElement* element) {
  const DocType& doctype = html_parse_->doctype();

  if (!doctype.IsXhtml()) {
    // Check for boolean attributes.
    AtomSetMap::const_iterator iter =
        one_value_attrs_map_.find(element->tag());
    if (iter != one_value_attrs_map_.end()) {
      const AtomSet& oneValueAttrs = iter->second;
      for (int i = 0, end = element->attribute_size(); i < end; ++i) {
        HtmlElement::Attribute& attribute = element->attribute(i);
        if (attribute.value() != NULL &&
            oneValueAttrs.count(attribute.name()) > 0) {
          attribute.SetValue(NULL);
        }
      }
    }
  }

  // Check for attributes with default values.
  ValueMapMap::const_iterator iter1 = default_value_map_.find(element->tag());
  if (iter1 != default_value_map_.end()) {
    const ValueMap& default_values = iter1->second;
    for (int i = 0; i < element->attribute_size(); ++i) {
      HtmlElement::Attribute& attribute = element->attribute(i);
      if (attribute.value() != NULL) {
        ValueMap::const_iterator iter2 = default_values.find(attribute.name());
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
