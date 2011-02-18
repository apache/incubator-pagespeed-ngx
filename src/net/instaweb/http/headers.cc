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

#include "net/instaweb/http/public/headers.h"

#include "base/logging.h"
#include "net/instaweb/http/http.pb.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/string_multi_map.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

template<class Proto> Headers<Proto>::Headers() {
  proto_.reset(new Proto);
  Clear();
}

template<class Proto> Headers<Proto>::~Headers() {
  Clear();
}

template<class Proto> void Headers<Proto>::Clear() {
  proto_->clear_major_version();
  proto_->clear_minor_version();
  map_.reset(NULL);
}

template<class Proto> int Headers<Proto>::major_version() const {
  return proto_->major_version();
}

template<class Proto> bool Headers<Proto>::has_major_version() const {
  return proto_->has_major_version();
}

template<class Proto> int Headers<Proto>::minor_version() const {
  return proto_->minor_version();
}

template<class Proto> void Headers<Proto>::set_major_version(
    int major_version) {
  proto_->set_major_version(major_version);
}

template<class Proto> void Headers<Proto>::set_minor_version(
    int minor_version) {
  proto_->set_minor_version(minor_version);
}

template<class Proto> int Headers<Proto>::NumAttributes() const {
  return proto_->header_size();
}

template<class Proto> const char* Headers<Proto>::Name(int i) const {
  return proto_->header(i).name().c_str();
}

template<class Proto> const char* Headers<Proto>::Value(int i) const {
  return proto_->header(i).value().c_str();
}

template<class Proto> void Headers<Proto>::PopulateMap() const {
  if (map_.get() == NULL) {
    map_.reset(new StringMultiMapInsensitive);
    for (int i = 0, n = NumAttributes(); i < n; ++i) {
      map_->Add(Name(i), Value(i));
    }
  }
}

template<class Proto> int Headers<Proto>::NumAttributeNames() const {
  PopulateMap();
  return map_->num_names();
}

template<class Proto> bool Headers<Proto>::Lookup(
    const StringPiece& name, CharStarVector* values) const {
  PopulateMap();
  return map_->Lookup(name, values);
}

template<class Proto> void Headers<Proto>::Add(
    const StringPiece& name, const StringPiece& value) {
  // TODO(jmarantz): Parse comma-separated values.  bmcquade sez:
  // you probably want to normalize these by splitting on commas and
  // adding a separate k,v pair for each comma-separated value. then
  // it becomes very easy to do things like search for individual
  // Content-Type tokens. Otherwise the client has to assume that
  // every single value could be comma-separated and they have to
  // parse it as such.  the list of header names that are not safe to
  // comma-split is at
  // http://src.chromium.org/viewvc/chrome/trunk/src/net/http/http_util.cc
  // (search for IsNonCoalescingHeader)

  NameValue* name_value = proto_->add_header();
  name_value->set_name(name.data(), name.size());
  name_value->set_value(value.data(), value.size());
  if (map_.get() != NULL) {
    map_->Add(name, value);
  }
}

template<class Proto> bool Headers<Proto>::RemoveAll(const StringPiece& name) {
  // Protobufs lack a convenient remove method for array elements, so
  // we copy the data into the map and do the remove there, then
  // reconstruct the protobuf.
  PopulateMap();
  CharStarVector values;
  bool removed = map_->Lookup(name, &values);
  if (removed) {
    proto_->clear_header();
    map_->RemoveAll(name);
    for (int i = 0, n = map_->num_values(); i < n; ++i) {
      NameValue* name_value = proto_->add_header();
      name_value->set_name(map_->name(i));
      name_value->set_value(map_->value(i));
    }
  }
  return removed;
}

template<class Proto> void Headers<Proto>::Replace(
    const StringPiece& name, const StringPiece& value) {
  // TODO(jmarantz): This could be arguably be implemented more efficiently.
  RemoveAll(name);
  Add(name, value);
}

template<class Proto> bool Headers<Proto>::WriteAsBinary(
    Writer* writer, MessageHandler* handler) {
  std::string buf;
  {
    StringOutputStream sstream(&buf);
    proto_->SerializeToZeroCopyStream(&sstream);
  }
  return writer->Write(buf, handler);
}

template<class Proto> bool Headers<Proto>::ReadFromBinary(
    const StringPiece& buf, MessageHandler* message_handler) {
  Clear();
  ArrayInputStream input(buf.data(), buf.size());
  return proto_->ParseFromZeroCopyStream(&input);
}

template<class Proto> bool Headers<Proto>::WriteAsHttp(
    Writer* writer, MessageHandler* handler) const {
  bool ret = true;
  for (int i = 0, n = NumAttributes(); ret && (i < n); ++i) {
    ret &= writer->Write(Name(i), handler);
    ret &= writer->Write(": ", handler);
    ret &= writer->Write(Value(i), handler);
    ret &= writer->Write("\r\n", handler);
  }
  ret &= writer->Write("\r\n", handler);
  return ret;
}

// Explicit template class instantiation.
// See http://www.cplusplus.com/forum/articles/14272/
template class Headers<HttpResponseHeaders>;
template class Headers<HttpRequestHeaders>;

}  // namespace net_instaweb
