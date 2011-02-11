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

#ifndef NET_INSTAWEB_HTMLPARSE_HTML_TESTING_PEER_H_
#define NET_INSTAWEB_HTMLPARSE_HTML_TESTING_PEER_H_

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parse.h"

namespace net_instaweb {

class HtmlTestingPeer {
 public:
  HtmlTestingPeer() { }

  static void SetNodeParent(HtmlNode* node, HtmlElement* parent) {
    node->set_parent(parent);
  }
  static void AddEvent(HtmlParse* parser, HtmlEvent* event) {
    parser->AddEvent(event);
  }
  static void SetCurrent(HtmlParse* parser, HtmlNode* node) {
    parser->SetCurrent(node);
  }
  static void set_coalesce_characters(HtmlParse* parser, bool x) {
    parser->set_coalesce_characters(x);
  }
  static size_t symbol_table_size(HtmlParse* parser) {
    return parser->symbol_table_size();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HtmlTestingPeer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTMLPARSE_HTML_TESTING_PEER_H_
