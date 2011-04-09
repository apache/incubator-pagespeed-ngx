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

#ifndef NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_NODE_H_
#define NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_NODE_H_

#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/html_parser_types.h"
#include "net/instaweb/util/public/arena.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// Base class for HtmlElement and HtmlLeafNode
class HtmlNode {
 public:
  virtual ~HtmlNode();
  friend class HtmlParse;

  HtmlElement* parent() const { return parent_; }
  bool live() const { return live_; }

  // Marks a node as dead.  The queue's end iterator should be passed in,
  // to remove references to stale iterators, and to force IsRewritable to
  // return false.
  void MarkAsDead(const HtmlEventListIterator& end);

  void* operator new(size_t size, Arena<HtmlNode>* arena) {
    return arena->Allocate(size);
  }

  void operator delete(void* ptr, Arena<HtmlNode>* arena) {
    CHECK(false) << "HtmlNode must not be deleted directly.";
  }

 protected:
  // TODO(jmarantz): jmaessen suggests instantiating the html nodes
  // without parents and computing them from context at the time they
  // are instantiated from the lexer.  This is a little more difficult
  // when synthesizing new nodes, however.  We assert sanity, however,
  // when calling HtmlParse::ApplyFilter.
  explicit HtmlNode(HtmlElement* parent) : parent_(parent), live_(true) {}

  // Create new event object(s) representing this node, and insert them into
  // the queue just before the given iterator; also, update this node object as
  // necessary so that begin() and end() will return iterators pointing to
  // the new event(s).  The line number for each event should probably be -1.
  virtual void SynthesizeEvents(const HtmlEventListIterator& iter,
                                HtmlEventList* queue) = 0;
  virtual void InvalidateIterators(const HtmlEventListIterator& end) = 0;

  // Return an iterator pointing to the first event associated with this node.
  virtual HtmlEventListIterator begin() const = 0;
  // Return an iterator pointing to the last event associated with this node.
  virtual HtmlEventListIterator end() const = 0;

  // Version that affects visibility of the destructor.
  void operator delete(void* ptr) {
    CHECK(false) << "HtmlNode must not be deleted directly.";
  }

 private:
  friend class HtmlTestingPeer;

  // Note: setting the parent doesn't change the DOM -- it just updates
  // the pointer.  This is intended to be called only from the DOM manipulation
  // methods in HtmlParse.
  void set_parent(HtmlElement* parent) { parent_ = parent; }

  HtmlElement* parent_;
  bool live_;
  DISALLOW_COPY_AND_ASSIGN(HtmlNode);
};

// Base class for leaf nodes (like HtmlCharactersNode and HtmlCommentNode)
class HtmlLeafNode : public HtmlNode {
 public:
  virtual ~HtmlLeafNode();
  friend class HtmlParse;

 protected:
  HtmlLeafNode(HtmlElement* parent, const HtmlEventListIterator& iter)
      : HtmlNode(parent),
        iter_(iter) {}
  virtual HtmlEventListIterator begin() const { return iter_; }
  virtual HtmlEventListIterator end() const { return iter_; }
  void set_iter(const HtmlEventListIterator& iter) { iter_ = iter; }
  virtual void InvalidateIterators(const HtmlEventListIterator& end);

 private:
  HtmlEventListIterator iter_;
  DISALLOW_COPY_AND_ASSIGN(HtmlLeafNode);
};

// Leaf node representing a CDATA section
class HtmlCdataNode : public HtmlLeafNode {
 public:
  virtual ~HtmlCdataNode();
  const GoogleString& contents() { return contents_; }
  friend class HtmlParse;

 protected:
  virtual void SynthesizeEvents(const HtmlEventListIterator& iter,
                                HtmlEventList* queue);

 private:
  HtmlCdataNode(HtmlElement* parent,
                const StringPiece& contents,
                const HtmlEventListIterator& iter)
      : HtmlLeafNode(parent, iter),
        contents_(contents.data(), contents.size()) {}
  const GoogleString contents_;
  DISALLOW_COPY_AND_ASSIGN(HtmlCdataNode);
};

// Leaf node representing raw characters in HTML
class HtmlCharactersNode : public HtmlLeafNode {
 public:
  virtual ~HtmlCharactersNode();
  const GoogleString& contents() { return contents_; }
  GoogleString* mutable_contents() { return &contents_; }
  void Append(const StringPiece& str) {
    contents_.append(str.data(), str.size());
  }
  friend class HtmlParse;

 protected:
  virtual void SynthesizeEvents(const HtmlEventListIterator& iter,
                                HtmlEventList* queue);

 private:
  HtmlCharactersNode(HtmlElement* parent,
                     const StringPiece& contents,
                     const HtmlEventListIterator& iter)
      : HtmlLeafNode(parent, iter),
        contents_(contents.data(), contents.size()) {}
  GoogleString contents_;
  DISALLOW_COPY_AND_ASSIGN(HtmlCharactersNode);
};

// Leaf node representing an HTML comment
class HtmlCommentNode : public HtmlLeafNode {
 public:
  virtual ~HtmlCommentNode();
  const GoogleString& contents() { return contents_; }
  friend class HtmlParse;

 protected:
  virtual void SynthesizeEvents(const HtmlEventListIterator& iter,
                                HtmlEventList* queue);

 private:
  HtmlCommentNode(HtmlElement* parent,
                  const StringPiece& contents,
                  const HtmlEventListIterator& iter)
      : HtmlLeafNode(parent, iter),
        contents_(contents.data(), contents.size()) {}
  const GoogleString contents_;
  DISALLOW_COPY_AND_ASSIGN(HtmlCommentNode);
};

// Leaf node representing an HTML IE directive
class HtmlIEDirectiveNode : public HtmlLeafNode {
 public:
  virtual ~HtmlIEDirectiveNode();
  const GoogleString& contents() { return contents_; }
  friend class HtmlParse;

 protected:
  virtual void SynthesizeEvents(const HtmlEventListIterator& iter,
                                HtmlEventList* queue);

 private:
  HtmlIEDirectiveNode(HtmlElement* parent,
                      const StringPiece& contents,
                      const HtmlEventListIterator& iter)
      : HtmlLeafNode(parent, iter),
        contents_(contents.data(), contents.size()) {}
  const GoogleString contents_;
  DISALLOW_COPY_AND_ASSIGN(HtmlIEDirectiveNode);
};

// Leaf node representing an HTML directive
class HtmlDirectiveNode : public HtmlLeafNode {
 public:
  virtual ~HtmlDirectiveNode();
  const GoogleString& contents() { return contents_; }
  friend class HtmlParse;

 protected:
  virtual void SynthesizeEvents(const HtmlEventListIterator& iter,
                                HtmlEventList* queue);

 private:
  HtmlDirectiveNode(HtmlElement* parent,
                    const StringPiece& contents,
                    const HtmlEventListIterator& iter)
      : HtmlLeafNode(parent, iter),
        contents_(contents.data(), contents.size()) {}
  const GoogleString contents_;
  DISALLOW_COPY_AND_ASSIGN(HtmlDirectiveNode);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTMLPARSE_PUBLIC_HTML_NODE_H_
