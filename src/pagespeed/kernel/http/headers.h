// Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef PAGESPEED_KERNEL_HTTP_HEADERS_H_
#define PAGESPEED_KERNEL_HTTP_HEADERS_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/proto_util.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class MessageHandler;
class NameValue;
class StringMultiMapInsensitive;
class Writer;

// Read/write API for HTTP headers (shared base class)
template<class Proto> class Headers {
 public:
  Headers();
  virtual ~Headers();

  virtual void Clear();

  int major_version() const;
  bool has_major_version() const;
  int minor_version() const;
  void set_major_version(int major_version);
  void set_minor_version(int major_version);

  // Raw access for random access to attribute name/value pairs.
  int NumAttributes() const;
  const GoogleString& Name(int i) const;
  const GoogleString& Value(int i) const;

  // Lookup attributes with provided name. Attribute values are stored in
  // values. Returns true iff there were any attributes with provided name.
  //
  // Note that Lookup, though declared const, is NOT thread-safe.  This
  // is because it lazily generates a map.
  // TODO(jmarantz): this is a problem waiting to happen, but I believe it
  // will not be a problem in the immediate future.  We can refactor our way
  // around this problem by moving the Map to an explicit separate class that
  // can be instantiated to assist with Lookups and Remove.  But that should
  // be done in a separate CL from the one I'm typing into now.
  bool Lookup(const StringPiece& name, ConstStringStarVector* values) const;

  // Looks up a single attribute value.  Returns NULL if the attribute is
  // not found, or if more than one attribute is found.
  const char* Lookup1(const StringPiece& name) const;

  // Does there exist a header with given name.
  bool Has(const StringPiece& name) const;

  // Is value one of the values in Lookup(name)?
  bool HasValue(const StringPiece& name, const StringPiece& value) const;

  // NumAttributeNames is also const but not thread-safe.
  int NumAttributeNames() const;

  // Remove all instances of cookie_name in all the cookie headers.
  // Empty cookie headers will be removed.
  // It might be better for performance if this function is called after
  // checking that the cookie is present.
  // CAVEAT: Double quoted values are not necessarily treated as one token.
  // Please refer to the test cases in headers_cookie_util_test.cc for more
  // details.
  void RemoveCookie(const StringPiece& cookie_name);

  // Adds a new header, even if a header with the 'name' exists already.
  void Add(const StringPiece& name, const StringPiece& value);

  // Remove headers by name and value. Return true if anything was removed.
  // Note: If the original headers were:
  // attr: val1
  // attr: val2
  // attr: val3
  // and you Remove(attr, val2), your new headers will be:
  // attr: val1, val3 (if attr is a comma-separated field)
  // and -
  // attr: val1
  // attr: val3 (otherwise).
  bool Remove(const StringPiece& name, const StringPiece& value);

  // Removes all headers by name.  Return true if anything was removed.
  bool RemoveAll(const StringPiece& name);

  // Removes all headers whose name is in |names|, which must be in
  // case-insensitive sorted order.
  bool RemoveAllFromSortedArray(const StringPiece* names,
                                int names_size);

  // Removes all headers whose name is in |names|, which must be in
  // string-insensitive sorted order.  Returns true if anything was
  // removed.
  static bool RemoveFromHeaders(const StringPiece* names,
                                int names_size,
                                protobuf::RepeatedPtrField<NameValue>* headers);

  // Removes all headers whose name starts with prefix.  Returns true if
  // anything was removed.
  bool RemoveAllWithPrefix(const StringPiece& prefix);

  // Removes any headers from this that are not in 'keep', considering both
  // names and values.  Returns true if anything was removed.
  bool RemoveIfNotIn(const Headers& keep);

  // Similar to RemoveAll followed by Add.  Note that the attribute
  // order may be changed as a side effect of this operation.
  virtual void Replace(const StringPiece& name, const StringPiece& value);

  // Merge headers. Replaces all headers specified both here and in
  // other with the version in other. Useful for updating headers
  // when recieving 304 Not Modified responses.
  // Note: This is order-scrambling.
  virtual void UpdateFrom(const Headers<Proto>& other);

  // Serialize HTTP header to a binary stream.
  virtual bool WriteAsBinary(Writer* writer, MessageHandler* message_handler);

  // Read HTTP header from a binary string.
  virtual bool ReadFromBinary(const StringPiece& buf, MessageHandler* handler);

  // Serialize HTTP headers in HTTP format so it can be re-parsed
  virtual bool WriteAsHttp(Writer* writer, MessageHandler* handler) const;

  // Copy protobuf representation to "proto".
  void CopyToProto(Proto* proto);

 protected:
  void PopulateMap() const;  // const is a lie, mutates map_.

  // Called whenever a mutation occurrs.  Subclasses may override to update
  // any local copies of data.
  virtual void UpdateHook();

  // We have two represenations for the name/value pairs.  The
  // HttpResponseHeader protobuf contains a simple string-pair vector, but
  // lacks a fast associative lookup.  So we will build structures for
  // associative lookup lazily, and keep them up-to-date if they are
  // present.
  mutable scoped_ptr<StringMultiMapInsensitive> map_;
  scoped_ptr<Proto> proto_;

 private:
  // If name is a comma-separated field (above), then split value at commas,
  // and add name, val for each of the comma-separated values
  // (removing whitespace and commas).
  // Otherwise, add the name, value pair to the map_.
  // const is a lie
  // NOTE: the map will contain the comma-split values, but the protobuf
  // will contain the original pairs including comma-separated values.
  void AddToMap(const StringPiece& name, const StringPiece& value) const;

  DISALLOW_COPY_AND_ASSIGN(Headers);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTTP_HEADERS_H_
