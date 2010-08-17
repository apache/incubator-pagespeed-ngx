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

// Author: sligocki@google.com (Shawn Ligocki)
//         jmarantz@google.com (Joshua Marantz)
//
// Resources are created by a ResourceManager.  Input resources are
// read from URLs or the file system.  Output resources are constructed
// programatically, usually by transforming one or more existing
// resources.  Both input and output resources inherit from this class
// so they can be used interchangably in successive rewrite passes.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_H_

#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/http_value.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

namespace net_instaweb {

class ResourceManager;

class Resource {
 public:
  static const int64 kDefaultExpireTimeMs;

  Resource(ResourceManager* manager, const ContentType* type)
      : resource_manager_(manager),
        type_(type) {
  }
  virtual ~Resource();

  // Common methods across all deriviations
  ResourceManager* resource_manager() const { return resource_manager_; }
  bool loaded() const { return meta_data_.status_code() != 0; }
  bool ContentsValid() const {
    return (meta_data_.status_code() == HttpStatus::OK);
  }
  int64 CacheExpirationTimeMs() const;
  StringPiece contents() const {
    StringPiece val;
    bool got_contents = value_.ExtractContents(&val);
    CHECK(got_contents) << "Resource contents read before loading";
    return val;
  }
  MetaData* metadata() { return &meta_data_; }
  const MetaData* metadata() const { return &meta_data_; }
  const ContentType* type() { return type_; }
  virtual void SetType(const ContentType* type);

  // Gets the absolute URL of the resource
  virtual std::string url() const = 0;

  virtual void DetermineContentType();

  // We define a new Callback type here because we need to
  // pass in the HTTPValue to the Done callback so it can
  // colllect the fetched data.
  //
  // TODO(jmarantz): This will not be necessary once we
  // change AsyncFetch to guarantee to annotate the resource
  // itself when the data becomes available.
  class AsyncCallback {
   public:
    virtual ~AsyncCallback();
    virtual void Done(bool success, HTTPValue* value) = 0;
  };

  // Links in the HTTP contents and header from a fetched value.
  // The contents are linked by sharing.  The HTTPValue also
  // contains a serialization of the headers, and this routine
  // parses them into meta_data_ and return whether that was
  // successful.
  bool Link(HTTPValue* source, MessageHandler* handler);

 protected:
  friend class ResourceManager;
  friend class UrlInputResourceCallback;

  // Read complete resource, storing MetaData and contents.
  virtual void ReadAsync(AsyncCallback* callback,
                         MessageHandler* message_handler);

  virtual bool ReadIfCached(MessageHandler* message_handler) = 0;

  ResourceManager* resource_manager_;

  const ContentType* type_;
  HTTPValue value_;  // contains contents and meta-data
  SimpleMetaData meta_data_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_H_
