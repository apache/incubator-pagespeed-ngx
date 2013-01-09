/**
 * Copyright 2009 Google Inc.
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

// DOM access API

#ifndef PAGESPEED_CORE_DOM_H_
#define PAGESPEED_CORE_DOM_H_

#include <algorithm>
#include <string>

#include "base/basictypes.h"

namespace pagespeed {

class DomDocument;
class DomElement;
class DomElementVisitor;

// Represents a rectangle with x, y, width, height coordinates.
class DomRect {
 public:
  DomRect(int x, int y, int width, int height)
      : x_(x),
        y_(y),
        width_(std::max(0, width)),
        height_(std::max(0, height)) {}
  int x() const { return x_; }
  int y() const { return y_; }
  int width() const { return width_; }
  int height() const { return height_; }

  bool IsEmpty() const { return width_ <= 0 || height_ <= 0; }

  // Get the intersection of this rect and the other rect.
  DomRect Intersection(const DomRect& other) const;

 private:
  int x_;
  int y_;
  int width_;
  int height_;
};

/**
 * Document interface.
 */
class DomDocument {
 public:
  // All of DomDocument's optional methods return a Status value.
  // Status indicates whether or not the runtime was able to perform
  // the requested operation.
  enum Status {
    SUCCESS,
    FAILURE,
  };

  DomDocument();
  virtual ~DomDocument();

  // Return the URL that points to this document.
  virtual std::string GetDocumentUrl() const = 0;

  // Return the URL that is used as the base for relative URLs appearing in
  // this document.  Usually this is the same as the document URL, but it can
  // be changed with a <base> tag.
  virtual std::string GetBaseUrl() const = 0;

  // Visit the elements within this document in pre-order (that is, always
  // visit a parent before visiting its children).
  virtual void Traverse(DomElementVisitor* visitor) const = 0;

  // Get the width of the document.
  virtual Status GetWidth(int* out_width) const;

  // Get the height of the document.
  virtual Status GetHeight(int* out_height) const;

  // Resolve a possibly-relative URI using this document's base URL.
  std::string ResolveUri(const std::string& uri) const;

  // Returns a copy of this DomDocument, where the ownership is transferred to
  // the caller. CAUTION: If the backing DOM is deleted, the behavior of this
  // DomDocument is undefined. Returns NULL if not implemented.
  virtual DomDocument* Clone() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(DomDocument);
};

/**
 * Element interface.
 */
class DomElement {
 public:
  // All of DomElement's optional methods return a Status value.
  // Status indicates whether or not the runtime was able to perform
  // the requested operation. For instance, if HasWidthSpecified() returns
  // Status::FAILURE, it indicates that the runtime is not capable of
  // determining whether or not the width of the DomElement was
  // specified. Status::FAILURE does not indicate that the DomElement does
  // not have a width specified. Note that when a method returns
  // Status::FAILURE, the values of the output parameters are
  // undefined. Callers must check the return value of methods
  // that return a Status.
  enum Status {
    SUCCESS,
    FAILURE,
  };

  DomElement();
  virtual ~DomElement();

  // Builds a new document instance for an IFrame's contents document.
  // It is up to the caller to dispose of this object once processing
  // is done.
  //
  // @return IFrame subdocument if the current node is an IFrame, else NULL.
  virtual DomDocument* GetContentDocument() const = 0;

  // Node type string.
  // Implementations must ensure that the contents of this string is
  // always UPPERCASE.
  virtual std::string GetTagName() const = 0;

  // @param name attribute name
  // @param attr_value output parameter to hold attribute value
  // @return true if the node has an attribute with that name. If the attribute
  // is boolean, attr_value will be empty. See HTML5 Specs:
  // (http://dev.w3.org/html5/spec/Overview.html#boolean-attributes)
  virtual bool GetAttributeByName(const std::string& name,
                                  std::string* attr_value) const = 0;

  // The methods below are optional parts of the DomElement API. If
  // the runtime is unable to compute the result for a given method, that
  // method should return Status::FAILURE. Otherwise, the method should
  // return Status::SUCCESS.

  // Get the X coordinate of the element within its parent document,
  // if available.
  // @param out_x output parameter to hold the x coordinate, in CSS pixels.
  // @return SUCCESS if the runtime is able to compute the x
  // coordinate of the node.
  virtual Status GetX(int* out_x) const;

  // Get the Y coordinate of the element within its parent document,
  // if available.
  // @param out_y output parameter to hold the y coordinate, in CSS pixels.
  // @return SUCCESS if the runtime is able to compute the y
  // coordinate of the node.
  virtual Status GetY(int* out_y) const;

  // Get the actual width of the element, in CSS pixels. This should
  // return the computed width of the element on screen (e.g. the
  // clientWidth), even if the width is not explicitly specified as an
  // attribute or via a style.
  // @param out_width output parameter to hold the width, in CSS pixels.
  // @return SUCCESS if the runtime is able to compute the width of the node.
  virtual Status GetActualWidth(int* out_width) const;

  // Get the actual height of the element, in CSS pixels. This should
  // return the computed height of the element on screen (e.g. the
  // clientHeight), even if the height is not explicitly specified as an
  // attribute or via a style.
  // @param out_height output parameter to hold the height, in CSS pixels.
  // @return SUCCESS if the runtime is able to compute the height of the node.
  virtual Status GetActualHeight(int* out_height) const;

  // Get whether or not the width of the element was explicitly
  // specified, either as an attribute, via an inline style, or via
  // CSS.
  // @param out_width_specified whether or not the element has a width
  // specified.
  // @return SUCCESS if the runtime is able to determine whether the
  // element has a specified width.
  virtual Status HasWidthSpecified(bool* out_width_specified) const;

  // Get whether or not the height of the element was explicitly
  // specified, either as an attribute, via an inline style, or via
  // CSS.
  // @param out_height_specified whether or not the element has a height
  // specified.
  // @return SUCCESS if the runtime is able to determine whether the
  // element has a specified height.
  virtual Status HasHeightSpecified(bool* out_height_specified) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(DomElement);
};

class DomElementVisitor {
 public:
  DomElementVisitor();
  virtual ~DomElementVisitor();
  virtual void Visit(const DomElement& node) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DomElementVisitor);
};

// A filtered visitor that only vists nodes that reference external
// resources. Also provides the fully qualified URL of the external
// resource.
class ExternalResourceDomElementVisitor {
 public:
  ExternalResourceDomElementVisitor();
  virtual ~ExternalResourceDomElementVisitor();
  virtual void VisitUrl(const DomElement& node, const std::string& url) = 0;

  // Called on each visit to a child DomDocument. The element will be
  // the HTML element that hosts the document (e.g. an iframe). This
  // will be called immediately after Visit() above is called for the
  // parent frame node of the document. Implementers may choose to
  // further traverse the child documents with an
  // ExternalResourceDomElementVisitor to discover resources
  // referenced in those child documents.
  virtual void VisitDocument(const DomElement& element,
                             const DomDocument& document) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalResourceDomElementVisitor);
};

// Instantiates a DomElementVisitor that wraps the given
// ExternalResourceDomElementVisitor. Ownership of the
// ExternalResourceDomElementVisitor is NOT transferred to this
// object. Ownership of the returned DomElementVisitor is transferred
// to the caller.
DomElementVisitor* MakeDomElementVisitorForDocument(
    const DomDocument* document,
    ExternalResourceDomElementVisitor* visitor);

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_DOM_H_
