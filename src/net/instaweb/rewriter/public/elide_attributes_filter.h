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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_ELIDE_ATTRIBUTES_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_ELIDE_ATTRIBUTES_FILTER_H_

#include <map>

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/util/public/atom.h"

namespace net_instaweb {

// Remove attributes and attribute values that can be safely elided.
class ElideAttributesFilter : public EmptyHtmlFilter {
 public:
  explicit ElideAttributesFilter(HtmlParse* html_parse);

  virtual void StartElement(HtmlElement* element);
  virtual const char* Name() const { return "ElideAttributes"; }

 private:
  struct AttrValue {
    const char* attr_value;
    bool requires_version_5;  // Default value only exists in (X)HTML 5.
  };

  typedef std::map<Atom, AtomSet, AtomCompare> AtomSetMap;
  typedef std::map<Atom, AttrValue, AtomCompare> ValueMap;
  typedef std::map<Atom, ValueMap, AtomCompare> ValueMapMap;

  HtmlParse* html_parse_;
  AtomSetMap one_value_attrs_map_;  // tag/attrs with only one possible value
  ValueMapMap default_value_map_;  // tag/attrs with default values

  DISALLOW_COPY_AND_ASSIGN(ElideAttributesFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_ELIDE_ATTRIBUTES_FILTER_H_
