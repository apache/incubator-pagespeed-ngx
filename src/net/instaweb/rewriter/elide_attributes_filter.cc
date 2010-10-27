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

// An attribute can be simplified if it can have only one value, like
// <option selected=selected> can be simplified to <option selected>.
// The list is derived from <http://www.w3.org/TR/html4/loose.dtd>.

struct TagAttr {
  const char* tag_name;
  const char* attr_name;
};

const TagAttr kOneValueList[] = {
  {"area", "nohref"},
  {"img", "ismap"},
  {"object", "declare"},
  {"hr", "noshade"},
  {"dl", "compact"},
  {"ol", "compact"},
  {"ul", "compact"},
  {"dir", "compact"},
  {"menu", "compact"},
  {"input", "checked"},
  {"input", "disabled"},
  {"input", "readonly"},
  {"input", "ismap"},
  {"select", "multiple"},
  {"select", "disabled"},
  {"optgroup", "disabled"},
  {"option", "selected"},
  {"option", "disabled"},
  {"textarea", "disabled"},
  {"textarea", "readonly"},
  {"button", "disabled"},
  {"th", "nowrap"},
  {"td", "nowrap"},
  {"frame", "noresize"},
  {"script", "defer"},
};

// An attribute can be removed from a tag if its name and value is in
// kDefaultList.  If attr_value is NULL, it means matching just attribute name
// is enough.  The list is derived from <http://www.w3.org/TR/html4/loose.dtd>.
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
};

const TagAttrValue kDefaultList[] = {
  {"script", "language", "javascript"},
  {"script", "type", NULL},
  {"style", "type", NULL},
  {"br", "clear", "none"},
  {"a", "shape", "rect"},
  {"area", "shape", "rect"},
  {"param", "valuetype", "data"},
  {"form", "method", "get"},
  {"form", "enctype", "application/x-www-form-urlencoded"},
  {"input", "type", "text"},
  {"button", "type", "submit"},
  {"colgroup", "span", "1"},
  {"col", "span", "1"},
  {"th", "rowspan", "1"},
  {"th", "colspan", "1"},
  {"td", "rowspan", "1"},
  {"td", "colspan", "1"},
  {"frame", "frameborder", "1"},
  {"frame", "scrolling", "auto"},
  {"iframe", "frameborder", "1"},
  {"iframe", "scrolling", "auto"},
};

}  // namespace

namespace net_instaweb {

ElideAttributesFilter::ElideAttributesFilter(HtmlParse* html_parse)
    : xhtml_mode_(false) {
  // Populate one_value_attrs_map_
  for (size_t i = 0; i < arraysize(kOneValueList); ++i) {
    const TagAttr& entry = kOneValueList[i];
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

void ElideAttributesFilter::StartDocument() {
  xhtml_mode_ = false;
}

void ElideAttributesFilter::Directive(HtmlDirectiveNode* directive) {
  // If this is an XHTML doctype directive, then put us into XHTML mode.
  std::string lowercase = directive->contents();
  LowerString(&lowercase);
  // TODO(mdsteele): This is probably not very robust; we should find a more
  // reliable way to test for XHTML doctypes.
  if (HasPrefixString(lowercase, "doctype") &&
      lowercase.find("xhtml") != std::string::npos) {
    xhtml_mode_ = true;
  }
}

void ElideAttributesFilter::StartElement(HtmlElement* element) {
  if (!xhtml_mode_) {
    // Check for one-value attributes.
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
      if (iter2 != default_values.end()) {
        const char* default_value = iter2->second;
        if (default_value == NULL ||
            (attribute.value() != NULL &&
             strcasecmp(attribute.value(), iter2->second) == 0)) {
          element->DeleteAttribute(i);
          --i;
        }
      }
    }
  }
}

}  // namespace net_instaweb
