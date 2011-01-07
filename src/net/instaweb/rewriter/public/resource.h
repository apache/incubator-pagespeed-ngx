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

#include "base/basictypes.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/http_value.h"
#include "net/instaweb/http/public/response_headers.h"
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
  // TODO(sligocki): Change name to HttpStatusOk?
  bool ContentsValid() const {
    return (meta_data_.status_code() == HttpStatus::kOK);
  }
  int64 CacheExpirationTimeMs() const;
  StringPiece contents() const {
    StringPiece val;
    bool got_contents = value_.ExtractContents(&val);
    CHECK(got_contents) << "Resource contents read before loading";
    return val;
  }
  ResponseHeaders* metadata() { return &meta_data_; }
  const ResponseHeaders* metadata() const { return &meta_data_; }
  const ContentType* type() const { return type_; }
  virtual void SetType(const ContentType* type);
  virtual bool IsCacheable() const;

  // Gets the absolute URL of the resource
  virtual std::string url() const = 0;

  virtual void DetermineContentType();

  // We define a new Callback type here because we need to
  // pass in the Resource to the Done callback so it can
  // collect the fetched data.
  class AsyncCallback {
   public:
    virtual ~AsyncCallback();
    virtual void Done(bool success, Resource* resource) = 0;
  };

  // Links in the HTTP contents and header from a fetched value.
  // The contents are linked by sharing.  The HTTPValue also
  // contains a serialization of the headers, and this routine
  // parses them into meta_data_ and return whether that was
  // successful.
  bool Link(HTTPValue* source, MessageHandler* handler);

 protected:
  friend class ResourceManager;
  friend class UrlReadAsyncFetchCallback;

  // Load the resource asynchronously, storing ResponseHeaders and
  // contents in cache.  Returns true, if the resource is already
  // loaded or loaded synchronously.
  virtual bool Load(MessageHandler* message_handler) = 0;
  // Same as Load, but calls a callback when finished.
  virtual void LoadAndCallback(AsyncCallback* callback,
                               MessageHandler* message_handler);

  ResourceManager* resource_manager_;

  const ContentType* type_;
  HTTPValue value_;  // contains contents and meta-data
  ResponseHeaders meta_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Resource);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_H_
