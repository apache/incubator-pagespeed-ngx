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

#include <vector>

#include "base/logging.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CachedResult;
struct ContentType;
class InputInfo;
class MessageHandler;
class Resource;
class ServerContext;
class RewriteOptions;

typedef RefCountedPtr<Resource> ResourcePtr;
typedef std::vector<ResourcePtr> ResourceVector;

class Resource : public RefCounted<Resource> {
 public:
  enum HashHint {
    kOmitInputHash,
    kIncludeInputHash
  };

  // This enumerates possible follow-up behaviors when a requested resource was
  // uncacheable.
  enum NotCacheablePolicy {
    kLoadEvenIfNotCacheable,
    kReportFailureIfNotCacheable,
  };

  Resource(ServerContext* resource_manager, const ContentType* type);

  // Common methods across all deriviations
  ServerContext* resource_manager() const { return resource_manager_; }

  // Answers question: Are we allowed to rewrite the contents now?
  //
  // Checks if the contents are loaded and valid and also if the resource is
  // up-to-date and cacheable enought to be rewritten by us right now.
  virtual bool IsValidAndCacheable() const;

  // TODO(sligocki): Do we need these or can we just use IsValidAndCacheable
  // everywhere?
  bool loaded() const { return response_headers_.status_code() != 0; }
  bool HttpStatusOk() const {
    return (response_headers_.status_code() == HttpStatus::kOK);
  }

  // Computes (with non-trivial cost) a hash of contents of a loaded resource.
  // Precondition: IsValidAndCacheable().
  // Warning: this uses contents_hasher_ and not the primary hasher,
  // unlike the hashes computed by OutputResource for naming purposes on
  // writes.
  GoogleString ContentsHash() const;

  // Adds a new InputInfo object representing this resource to CachedResult,
  // assigning the index supplied.
  void AddInputInfoToPartition(HashHint suggest_include_content_hash,
                               int index, CachedResult* partition);

  // Set CachedResult's input info used for expiration validation.
  // If include_content_hash is kIncludeInputHash, and it makes sense for
  // the Resource type to check if resource changed based by content hash
  // (e.g. it would be pointless for data:), the hash of resource's
  // contents should also be set on 'input'.
  //
  // Default one sets resource type as CACHED and sets an expiration timestamp,
  // last modified, date, and, if requested, content hash.
  // If a derived class has a different criterion for validity, override
  // this method.
  virtual void FillInPartitionInputInfo(HashHint suggest_include_content_hash,
                                        InputInfo* input);

  void FillInPartitionInputInfoFromResponseHeaders(
      const ResponseHeaders& headers,
      InputInfo* input);

  // Returns 0 if resource is not cacheable.
  // TODO(sligocki): Look through callsites and make sure this is being
  // interpreted correctly.
  int64 CacheExpirationTimeMs() const;

  StringPiece contents() const {
    StringPiece val;
    bool got_contents = value_.ExtractContents(&val);
    CHECK(got_contents) << "Resource contents read before loading";
    return val;
  }
  ResponseHeaders* response_headers() { return &response_headers_; }
  const ResponseHeaders* response_headers() const { return &response_headers_; }
  const ContentType* type() const { return type_; }
  virtual void SetType(const ContentType* type);

  // Note: this is empty if the header is not specified.
  StringPiece charset() const { return charset_; }
  void set_charset(StringPiece c) { c.CopyToString(&charset_); }

  virtual bool IsCacheableTypeOfResource() const { return true; }

  // Gets the absolute URL of the resource
  virtual GoogleString url() const = 0;

  // Computes the content-type (and charset) based on response_headers and
  // extension, and sets it via SetType.
  void DetermineContentType();

  // Obtain rewrite options for this. Any resources which return true
  // for IsCacheable() but don't unconditionally return true for loaded()
  // must override this in a useful way. Used in cache invalidation.
  virtual const RewriteOptions* rewrite_options() const = 0;

  // We define a new Callback type here because we need to
  // pass in the Resource to the Done callback so it can
  // collect the fetched data.
  class AsyncCallback {
   public:
    explicit AsyncCallback(const ResourcePtr& resource) : resource_(resource) {}

    virtual ~AsyncCallback();
    virtual void Done(bool success) = 0;

    const ResourcePtr& resource() { return resource_; }

    // Override this to return true if this callback is safe to invoke from
    // thread other than the main html parse/http request serving thread.
    virtual bool EnableThreaded() const { return false; }

   private:
    ResourcePtr resource_;
    DISALLOW_COPY_AND_ASSIGN(AsyncCallback);
  };

  // An AsyncCallback for a freshen. The Done() callback in the default
  // implementation deletes itself.
  class FreshenCallback : public AsyncCallback {
   public:
    explicit FreshenCallback(const ResourcePtr& resource)
        : AsyncCallback(resource) {}

    virtual ~FreshenCallback();
    // Returns NULL by default. Sublasses should override this if they want this
    // to be updated based on the response fetched while freshening.
    virtual InputInfo* input_info() { return NULL; }

    // This is called with success = true only if the hash of the fetched
    // response is the same as the hash in input_info()->input_content_hash().
    virtual void Done(bool success) {
      delete this;
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(FreshenCallback);
  };

  // Links in the HTTP contents and header from a fetched value.
  // The contents are linked by sharing.  The HTTPValue also
  // contains a serialization of the headers, and this routine
  // parses them into response_headers_ and return whether that was
  // successful.
  bool Link(HTTPValue* source, MessageHandler* handler);

  // Freshen a soon-to-expire resource so that we minimize the number
  // of cache misses when serving live traffic.
  // Note that callback may be NULL, and all subclasses must handle this.
  virtual void Freshen(FreshenCallback* callback, MessageHandler* handler);

  // Links the stale fallback value that can be used in case a fetch fails.
  void LinkFallbackValue(HTTPValue* value);

  void set_is_background_fetch(bool x) { is_background_fetch_ = x; }
  bool is_background_fetch() const { return is_background_fetch_; }

 protected:
  virtual ~Resource();
  REFCOUNT_FRIEND_DECLARATION(Resource);
  friend class ServerContext;
  friend class RewriteDriver;  // for ReadIfCachedWithStatus
  friend class UrlReadAsyncFetchCallback;
  friend class ResourceManagerHttpCallback;

  // Load the resource asynchronously, storing ResponseHeaders and
  // contents in cache.  Returns true, if the resource is already
  // loaded or loaded synchronously. Never reports uncacheable resources.
  virtual bool Load(MessageHandler* message_handler) = 0;

  // Same as Load, but calls a callback when finished.  The ResourcePtr
  // used to construct 'callback' must be the same as the resource used
  // to invoke this method. If the resource is uncacheable, will only
  // return true if not_cacheable_policy == kLoadEvenIfNotCacheable.
  virtual void LoadAndCallback(NotCacheablePolicy not_cacheable_policy,
                               AsyncCallback* callback,
                               MessageHandler* message_handler);

  ServerContext* resource_manager_;

  const ContentType* type_;
  GoogleString charset_;
  HTTPValue value_;  // contains contents and meta-data
  ResponseHeaders response_headers_;

  // A stale value that can be used in case we aren't able to fetch a fresh
  // version of the resource. Note that this should only be used if it is not
  // empty.
  HTTPValue fallback_value_;

 private:
  // Indicates whether we are trying to load the resource for a background
  // rewrite or to serve a user request.
  // Note that by default, we assume that every fetch is triggered in the
  // background and is not user-facing unless we explicitly set
  // is_background_fetch_ to false.
  bool is_background_fetch_;
  DISALLOW_COPY_AND_ASSIGN(Resource);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_H_
