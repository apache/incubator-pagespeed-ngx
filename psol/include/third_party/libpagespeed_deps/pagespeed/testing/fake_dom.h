// Copyright 2010 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PAGESPEED_TESTING_FAKE_DOM_H_
#define PAGESPEED_TESTING_FAKE_DOM_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "pagespeed/core/dom.h"
#include "pagespeed/core/string_util.h"

namespace pagespeed_testing {

class FakeDomDocument;

class FakeDomElement : public pagespeed::DomElement {
 public:
  static FakeDomElement* New(FakeDomElement* parent,
                             const std::string& tag_name);
  static FakeDomElement* NewStyle(FakeDomElement* parent);
  static FakeDomElement* NewLinkStylesheet(FakeDomElement* parent,
                                           const std::string& url);
  static FakeDomElement* NewImg(FakeDomElement* parent,
                                const std::string& url);
  static FakeDomElement* NewScript(FakeDomElement* parent,
                                   const std::string& url);
  static FakeDomElement* NewIframe(FakeDomElement* parent);
  static FakeDomElement* NewRoot(FakeDomDocument* parent,
                                 const std::string& tag_name);

  virtual ~FakeDomElement();

  virtual pagespeed::DomDocument* GetContentDocument() const;
  virtual std::string GetTagName() const;
  virtual bool GetAttributeByName(const std::string& name,
                                  std::string* attr_value) const;
  virtual Status GetX(int* out_x) const;
  virtual Status GetY(int* out_y) const;
  virtual Status GetActualWidth(int* out_width) const;
  virtual Status GetActualHeight(int* out_height) const;
  virtual Status HasHeightSpecified(bool *out) const;
  virtual Status HasWidthSpecified(bool *out) const;
  virtual Status GetNumChildren(size_t* number) const;
  virtual Status GetChild(const DomElement** child, size_t index) const;

  // Adds an attribute to the element.
  void AddAttribute(const std::string& key, const std::string& value);
  void RemoveAttribute(const std::string& key);
  void SetCoordinates(int x, int y);
  void SetActualWidthAndHeight(int width, int height);

  // Helpers for tree traversal.
  const FakeDomElement* GetFirstChild() const;
  const FakeDomElement* GetParentElement() const;
  const FakeDomElement* GetNextSibling() const;

  // Create a shallow copy of this document. A cloned instance must
  // not outlive the instance that created it.
  FakeDomElement* Clone() const;

 private:
  friend class FakeDomDocument;

  typedef std::vector<FakeDomElement*> ChildVector;
  typedef pagespeed::string_util::CaseInsensitiveStringStringMap
      StringStringMap;

  FakeDomElement(const FakeDomElement* parent, const std::string& tag_name);

  std::string tag_name_;
  const FakeDomElement* parent_;
  ChildVector children_;
  StringStringMap attributes_;
  // The document, if this is a frame/iframe element.
  const FakeDomDocument* document_;
  int x_;
  int y_;
  int actual_width_;
  int actual_height_;
  bool is_clone_;
};

class FakeDomDocument : public pagespeed::DomDocument {
 public:
  // Create a new root document.
  static FakeDomDocument* NewRoot(const std::string& document_url);

  // Create a new sub document, under the specified iframe element.
  static FakeDomDocument* New(FakeDomElement* iframe,
                              const std::string& document_url);
  virtual ~FakeDomDocument();

  virtual std::string GetDocumentUrl() const;
  virtual std::string GetBaseUrl() const;
  virtual void Traverse(pagespeed::DomElementVisitor* visitor) const;
  virtual Status GetWidth(int* out_width) const;
  virtual Status GetHeight(int* out_height) const;

  void SetBaseUrl(const std::string& base_url) { base_url_ = base_url; }

  // Create a shallow copy of this document. A cloned instance must
  // not outlive the instance that created it.
  FakeDomDocument* Clone() const;

  // Get the root element for this document.
  const FakeDomElement* GetDocumentElement() const;

 private:
  friend class FakeDomElement;
  explicit FakeDomDocument(const std::string& document_url);

  std::string url_;
  std::string base_url_;
  int width_;
  int height_;
  const FakeDomElement* document_element_;
  bool is_clone_;
};

}  // namespace pagespeed_testing

#endif  // PAGESPEED_TESTING_FAKE_DOM_H_
