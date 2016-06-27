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

#include <map>
#include <utility>

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
  // typedef's for manipulating the cookie multimap.
  typedef std::pair<StringPiece, StringPiece> ValueAndAttributes;
  typedef std::multimap<StringPiece, ValueAndAttributes> CookieMultimap;
  typedef std::multimap<StringPiece, ValueAndAttributes>::const_iterator
      CookieMultimapConstIter;

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
  void SetValue(int i, StringPiece value);

  // Lookup attributes with provided name. Attribute values are stored in
  // values. Returns true iff there were any attributes with provided name.
  // Attributes that normally appear as a comma-separated header list
  // (Cache-Control, Accept, etc.) will yield multiple entries in *values.
  // Multiple occurrences of a header (Cookie, etc.) will also yield multiple
  // entries in *values.  In most cases (but not Cookies) the semantics are
  // equivalent either way.  See:
  //   http://tools.ietf.org/html/draft-ietf-httpbis-p1-messaging-26#section-3.2.2
  //
  // Note that Lookup, though declared const, is NOT thread-safe.  This
  // is because it lazily generates a map.
  // TODO(jmarantz): this is a problem waiting to happen, but I believe it
  // will not be a problem in the immediate future.  We can refactor our way
  // around this problem by moving the Map to an explicit separate class that
  // can be instantiated to assist with Lookups and Remove.  But that should
  // be done in a separate CL from the one I'm typing into now.
  bool Lookup(const StringPiece& name, ConstStringStarVector* values) const;

  // Synthesize a string that represents what all the values a header would
  // serialize to.  Returns "" if the header isn't present.
  //
  // Same constness warnings as for ResponseHeaders::Lookup() apply.
  GoogleString LookupJoined(StringPiece name) const;

  // Looks up a single attribute value.  Returns NULL if the attribute is not
  // found, or if more than one attribute is found (either multiple
  // comma-separated entries, or multiple copies of the header).
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

  // Adds a new header, even if a header with the 'name' exists already.  Note
  // that this does *not* add a new entry to a comma-separated list for headers
  // that are ordinarily represented that way, but that the semantics will be
  // the same.
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
  //
  // All instances of 'value' will be removed from 'name'.
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
  template<class StringType>
  static bool RemoveFromHeaders(const StringType* names,
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
  void CopyToProto(Proto* proto) const;

  // Check the given vector of name[=value] strings for an entry with the given
  // name, returning true iff found and setting optional_retval iff it is not
  // NULL and there is a value assigned (there is an '=' in the string).
  // Note: only the value of the first occurence of name is returned.
  // Note: the return value is trimmed of leading and trailing whitespace.
  // Note: the return value might be assigned to even if we return false.
  static bool FindValueForName(const StringPieceVector& name_equals_value_vec,
                               StringPiece name_to_find,
                               StringPiece* optional_retval);

  // Parse a name[=value] string and extract the name and value (if the given
  // argument isn't NULL) with leading and trailing whitespace removed.
  // Returns true if a value was assigned (specifically, an '=' was found),
  // else false.
  static bool ExtractNameAndValue(StringPiece input, StringPiece* name,
                                  StringPiece* optional_retval);

 protected:
  // You need to know what you're doing to use these, so for subclasses only.
  void SetProto(Proto* proto);  // Takes ownership of the argument.
  void CopyProto(const Proto& proto);

  void PopulateMap() const;  // const is a lie, mutates map_.

  // Populates the cookies map and returns a const pointer to it. 'name' is
  // the name of the header to lookup: either "Cookie" for request headers or
  // "Set-Cookie" for response headers. The header is assumed to contain semi-
  // colon separated name=value pairs. For "Set-Cookie" headers, the first pair
  // is the name and value of the cookie and subsequent pairs are attributes of
  // that cookie - the value will be the 'first' part of the pair in the map,
  // all the attributes will be the 'second' part (as a single string of semi-
  // colon separated name=value pairs). For "Cookie" headers, each pair is an
  // independent cookie and is put into the map separately.
  // Note that const is a lie: cookies_ is mutated.
  const CookieMultimap* PopulateCookieMap(StringPiece header_name) const;

  // Called whenever a mutation occurrs.  Subclasses may override to update
  // any local copies of data.
  virtual void UpdateHook();

  // Subclasses need to manipulate the proto_ member as its type and use is
  // specific to the subclass.
  const Proto* proto() const { return proto_.get(); }
  Proto* mutable_proto() { return proto_.get(); }

 private:
  // If name is a comma-separated field (above), then split value at commas,
  // and add name, val for each of the comma-separated values
  // (removing whitespace and commas).
  // Otherwise, add the name, value pair to the map_.
  // const is a lie
  // NOTE: the map will contain the comma-split values, but the protobuf
  // will contain the original pairs including comma-separated values.
  void AddToMap(const StringPiece& name, const StringPiece& value) const;

  // We have two representations for the name/value pairs.  Proto contains a
  // simple string-pair vector, but lacks a fast associative lookup.  So we
  // will build structures for associative lookup lazily, and keep them
  // up-to-date if they are present.
  mutable scoped_ptr<StringMultiMapInsensitive> map_;
  scoped_ptr<Proto> proto_;

  // Furthermore, we also have a map of cookie names to <value, attributes>.
  // It is lazilyloaded by PopulateCookieMap as/when required. The keys and
  // values all point into the map_ data element. We cater for the same cookie
  // being set multiple times though we don't necessarily handle that correctly.
  mutable scoped_ptr<CookieMultimap> cookies_;

  DISALLOW_COPY_AND_ASSIGN(Headers);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTTP_HEADERS_H_
