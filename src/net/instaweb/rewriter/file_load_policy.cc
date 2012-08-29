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

// Author: sligocki@google.com (Shawn Ligocki)

#include <list>

#include "net/instaweb/rewriter/public/file_load_mapping.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/file_load_rule.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/re2.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

FileLoadPolicy::~FileLoadPolicy() {
  STLDeleteElements(&file_load_mappings_);
  STLDeleteElements(&file_load_rules_);
}

bool FileLoadPolicy::ShouldLoadFromFile(const GoogleUrl& url,
                                        GoogleString* filename) const {
  if (!url.is_valid()) {
    return false;
  }

  StringPiece url_string = url.AllExceptQuery();
  if (!url_string.empty()) {
    // TODO(sligocki): Consider layering a cache over this lookup.
    // Note: Later associations take precedence over earlier ones.
    FileLoadMappings::const_reverse_iterator mappings_iter;
    for (mappings_iter = file_load_mappings_.rbegin();
         mappings_iter != file_load_mappings_.rend(); ++mappings_iter) {
      if ((*mappings_iter)->Substitute(url_string, filename)) {
        // GoogleUrl will decode most %XX escapes, but it does not convert
        // "%20" -> " " which has come up often.
        GlobalReplaceSubstring("%20", " ", filename);

        // We know know what file this url should map to, and we want know
        // whether this one is safe to load directly or whether we need to back
        // off and load through HTTP.  By default a mapping set up with
        // Associate() permits direct loading of anything it applies to, but
        // AddRule() lets people add exceptions.  See if any exceptions apply.
        FileLoadRules::const_reverse_iterator rules_iter;
        for (rules_iter = file_load_rules_.rbegin();
             rules_iter != file_load_rules_.rend(); ++rules_iter) {
          FileLoadRule::Classification classification =
              (*rules_iter)->Classify(*filename);
          if (classification == FileLoadRule::kAllowed) {
            return true;  // Whitelist entry: load directly.
          } else if (classification == FileLoadRule::kDisallowed) {
            return false;  // Blacklist entry: fall back to HTTP.
          }
        }
        return true;  // No exception applied; default allow.
      }
    }
  }
  return false;  // No mapping found, no file to load from.
}

bool FileLoadPolicy::AddRule(const GoogleString& rule_str, bool is_regexp,
                             bool allow, GoogleString* error) {
  FileLoadRule* rule = NULL;
  if (is_regexp) {
    RE2 re(rule_str);
    if (!re.ok()) {
      error->assign(re.error());
      return false;
    }
    rule = new FileLoadRuleRegexp(rule_str, allow);
  } else {
    rule = new FileLoadRuleLiteral(rule_str, allow);
  }
  file_load_rules_.push_back(rule);
  return true;
}

bool FileLoadPolicy::AssociateRegexp(const StringPiece& url_regexp,
                                     const StringPiece& filename_prefix,
                                     GoogleString* error) {
  GoogleString url_regexp_str, filename_prefix_str;

  url_regexp.CopyToString(&url_regexp_str);
  filename_prefix.CopyToString(&filename_prefix_str);

  if (!url_regexp.starts_with("^")) {
    error->assign("File mapping regular expression must match beginning "
                  "of string. (Must start with '^'.)");
    return false;
  }

  RE2 re(url_regexp_str);
  if (!re.ok()) {
    error->assign(re.error());
    return false;
  } else if (!re.CheckRewriteString(filename_prefix_str, error)) {
    return false;
  }

  file_load_mappings_.push_back(
      new FileLoadMappingRegexp(url_regexp_str, filename_prefix_str));

  return true;
}

void FileLoadPolicy::Associate(const StringPiece& url_prefix_in,
                               const StringPiece& filename_prefix_in) {
  GoogleString url_prefix, filename_prefix;

  url_prefix_in.CopyToString(&url_prefix);
  filename_prefix_in.CopyToString(&filename_prefix);

  // Make sure these are directories.  Add a terminal slashes if absent.
  EnsureEndsInSlash(&url_prefix);
  EnsureEndsInSlash(&filename_prefix);

  // TODO(sligocki): Should fail if filename_prefix doesn't start with '/'?

  file_load_mappings_.push_back(
      new FileLoadMappingLiteral(url_prefix, filename_prefix));
}

void FileLoadPolicy::Merge(const FileLoadPolicy& other) {
  FileLoadMappings::const_iterator mappings_iter;
  for (mappings_iter = other.file_load_mappings_.begin();
       mappings_iter != other.file_load_mappings_.end(); ++mappings_iter) {
    // Copy associations over.
    file_load_mappings_.push_back((*mappings_iter)->Clone());
  }

  FileLoadRules::const_iterator rules_iter;
  for (rules_iter = other.file_load_rules_.begin();
       rules_iter != other.file_load_rules_.end(); ++rules_iter) {
    // Copy rules over.
    file_load_rules_.push_back((*rules_iter)->Clone());
  }
}

}  // namespace net_instaweb
