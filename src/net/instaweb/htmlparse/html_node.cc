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

#include "net/instaweb/htmlparse/public/html_node.h"

#include "net/instaweb/htmlparse/html_event.h"
#include "net/instaweb/htmlparse/public/html_parser_types.h"

namespace net_instaweb {

HtmlNode::~HtmlNode() {}

void HtmlLiveNode::MarkAsDead(const HtmlEventListIterator& end) {
  live_ = false;
  InvalidateIterators(end);
}

HtmlLiveNode::~HtmlLiveNode() {}
HtmlLeafNode::~HtmlLeafNode() {}

void HtmlLeafNode::InvalidateIterators(const HtmlEventListIterator& end) {
  set_iter(end);
}

HtmlCdataNode::~HtmlCdataNode() {}

void HtmlCdataNode::SynthesizeEvents(const HtmlEventListIterator& iter,
                                     HtmlEventList* queue) {
  // We use -1 as a bogus line number, since the event is synthetic.
  HtmlCdataEvent* event = new HtmlCdataEvent(this, -1);
  set_iter(queue->insert(iter, event));
}

HtmlCharactersNode::~HtmlCharactersNode() {}

void HtmlCharactersNode::SynthesizeEvents(const HtmlEventListIterator& iter,
                                          HtmlEventList* queue) {
  // We use -1 as a bogus line number, since the event is synthetic.
  HtmlCharactersEvent* event = new HtmlCharactersEvent(this, -1);
  set_iter(queue->insert(iter, event));
}

HtmlCommentNode::~HtmlCommentNode() {}

void HtmlCommentNode::SynthesizeEvents(const HtmlEventListIterator& iter,
                                       HtmlEventList* queue) {
  // We use -1 as a bogus line number, since the event is synthetic.
  HtmlCommentEvent* event = new HtmlCommentEvent(this, -1);
  set_iter(queue->insert(iter, event));
}

HtmlIEDirectiveNode::~HtmlIEDirectiveNode() {}

void HtmlIEDirectiveNode::SynthesizeEvents(const HtmlEventListIterator& iter,
                                         HtmlEventList* queue) {
  // We use -1 as a bogus line number, since the event is synthetic.
  HtmlIEDirectiveEvent* event = new HtmlIEDirectiveEvent(this, -1);
  set_iter(queue->insert(iter, event));
}

HtmlDirectiveNode::~HtmlDirectiveNode() {}

void HtmlDirectiveNode::SynthesizeEvents(const HtmlEventListIterator& iter,
                                         HtmlEventList* queue) {
  // We use -1 as a bogus line number, since the event is synthetic.
  HtmlDirectiveEvent* event = new HtmlDirectiveEvent(this, -1);
  set_iter(queue->insert(iter, event));
}

}  // namespace net_instaweb
