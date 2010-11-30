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

// TODO(mdsteele): This list should depend on the doctype.  For example, there
// are some attributes that have default values in HTML 5 but not in HTML 4.

struct TagAttrValue {
  const char* tag_name;
  const char* attr_name;
  const char* attr_value;
};

const TagAttrValue kDefaultList[] = {
  // http://www.w3.org/TR/html4/struct/links.html#h-12.2
  {"a", "shape", "rect"},
  // http://www.w3.org/TR/html5/the-map-element.html#the-area-element
  {"area", "shape", "rect"},
  // http://www.w3.org/TR/html5/the-button-element.html#the-button-element
  {"button", "type", "submit"},
  // http://www.w3.org/TR/html5/interactive-elements.html#the-command
  {"command", "type", "command"},
  // http://www.w3.org/TR/html5/forms.html#the-form-element
  {"form", "autocomplete", "on"},
  // http://www.w3.org/TR/html5/association-of-controls-and-forms.html#
  // attributes-for-form-submission
  {"form", "enctype", "application/x-www-form-urlencoded"},
  {"form", "method", "get"},
  // http://www.w3.org/TR/html4/present/frames.html#h-16.2.2
  {"frame", "frameborder", "1"},
  {"frame", "scrolling", "auto"},
  // http://www.w3.org/TR/html4/present/frames.html#h-16.5
  {"iframe", "frameborder", "1"},
  {"iframe", "scrolling", "auto"},
  // http://www.w3.org/TR/html5/the-input-element.html#the-input-element
  {"input", "type", "text"},
  // http://www.w3.org/TR/html5/the-button-element.html#the-keygen-element
  {"keygen", "keytype", "rsa"},
  // http://www.w3.org/TR/html5/interactive-elements.html#menus
  {"menu", "type", "list"},
  // http://www.w3.org/TR/html4/struct/objects.html#h-13.3.2
  {"param", "valuetype", "data"},
  // http://www.w3.org/TR/html5/scripting-1.html
  {"script", "language", "javascript"},
  {"script", "type", "text/javascript"},
  // http://www.w3.org/TR/html5/video.html#the-source-element
  {"source", "media", "all"},
  // http://www.w3.org/TR/html5/semantics.html#the-style-element
  {"style", "type", "text/css"},
  {"style", "media", "all"},
  // http://www.w3.org/TR/html5/the-button-element.html#the-textarea-element
  {"textarea", "wrap", "soft"},
  // http://www.w3.org/TR/html5/tabular-data.html#table-model
  {"col", "span", "1"},
  {"colgroup", "span", "1"},
  {"td", "colspan", "1"},
  {"td", "rowspan", "1"},
  {"th", "colspan", "1"},
  {"th", "rowspan", "1"},
  // http://www.w3.org/TR/html5/tabular-data.html#the-th-element
  {"th", "scope", "auto"},
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
    default_value_map_[html_parse->Intern(entry.tag_name)]
                      [html_parse->Intern(entry.attr_name)] = entry.attr_value;
  }
}

void ElideAttributesFilter::StartElement(HtmlElement* element) {
  if (!html_parse_->doctype().IsXhtml()) {
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
  AtomMapMap::const_iterator iter1 = default_value_map_.find(element->tag());
  if (iter1 != default_value_map_.end()) {
    const AtomMap& default_values = iter1->second;
    for (int i = 0; i < element->attribute_size(); ++i) {
      HtmlElement::Attribute& attribute = element->attribute(i);
      AtomMap::const_iterator iter2 = default_values.find(attribute.name());
      if (iter2 != default_values.end() &&
          attribute.value() != NULL &&
          strcasecmp(attribute.value(), iter2->second) == 0) {
        element->DeleteAttribute(i);
        --i;
      }
    }
  }
}

}  // namespace net_instaweb
