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

// Copyright 2006, Google Inc.  All rights reserved.
// Author: mec@google.com  (Michael Chastain)
//
// Map an html tag to a dense index number.
// Hardwired for speed on builtin tags.
// Caller can add tags on top of the builtins.
// Caller can choose case-sensitive or case-insensitive.
//
// TODO(mec): merge this with webutil/html/htmltag

#ifndef WEBUTIL_HTML_HTMLTAGINDEX_H__
#define WEBUTIL_HTML_HTMLTAGINDEX_H__

#include <string>
#include "string_using.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "util/gtl/dense_hash_map.h"
#include "webutil/html/htmltagenum.h"

class HtmlTagIndex {
 public:
  HtmlTagIndex();
  ~HtmlTagIndex();

  // Add a tag and return its index.  It is okay to add a builtin
  // tag or to add the same tag more than once.
  int AddHtmlTag(const char* tag, int length);
  int AddHtmlTag(const char* tag) {
    return AddHtmlTag(tag, strlen(tag));
  }

  // Find returns a value in the half-open range [0..GetIndexMax()).
  // 0 == unknown tag.
  COMPILE_ASSERT(kHtmlTagUnknown == 0, unknown_tag_equals_zero);
  int FindHtmlTag(const char* tag, int length) const;
  int FindHtmlTag(const char* tag) const {
    return FindHtmlTag(tag, strlen(tag));
  }

  // Return the half-open upper bound on lookup return value.
  // If GetIndexMax returns 10, then find will return [0..9).
  int GetIndexMax() const {
    return index_max_;
  };

  // Set case sensitivity.  This cannot be done after any calls to AddHtmlTag.
  void SetCaseSensitive(bool case_sensitive);
  bool IsCaseSensitive() const {
    return case_sensitive_;
  }

 private:
  // Case sensitive stuff.
  bool case_sensitive_fixed_;
  bool case_sensitive_;
  uint32 case_mask_1_;
  uint32 case_mask_2_;
  uint32 case_mask_3_;
  uint32 case_mask_4_;
  uint64 case_mask_5_;
  uint64 case_mask_6_;
  uint64 case_mask_7_;
  uint64 case_mask_8_;

  int index_max_;
  typedef dense_hash_map<string, int> CustomTagMap;
  scoped_ptr<CustomTagMap> custom_tag_map_;

  DISALLOW_COPY_AND_ASSIGN(HtmlTagIndex);
};

#endif  // WEBUTIL_HTML_HTMLTAGINDEX_H__
