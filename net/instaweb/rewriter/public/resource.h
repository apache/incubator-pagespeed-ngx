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
// Resources are created by a RewriteDriver.  Input resources are
// read from URLs or the file system.  Output resources are constructed
// programatically, usually by transforming one or more existing
// resources.  Both input and output resources inherit from this class
// so they can be used interchangeably in successive rewrite passes.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_H_

#include <vector>

#include "base/logging.h"
#include "net/instaweb/http/public/http_cache_failure.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/request_context.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

class CachedResult;
class GoogleUrl;
class InputInfo;
class MessageHandler;
class Resource;
class RewriteDriver;
class ServerContext;

typedef RefCountedPtr<Resource> ResourcePtr;
typedef std::vector<ResourcePtr> ResourceVector;

class Resource : public RefCounted<Resource> {
 public:
  class AsyncCallback;

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


  Resource(const RewriteDriver* driver, const ContentType* type);

  // Common methods across all deriviations
  ServerContext* server_context() const { return server_context_; }

  // Checks if the contents are loaded and valid and also if the resource is
  // up-to-date and cacheable by a proxy like us.
  virtual bool IsValidAndCacheable() const;

  // Whether the domain on which the resource is present is explicitly
  // authorized or not. Unauthorized resources can be created for the purpose
  // of inlining content into the HTML.
  bool is_authorized_domain() { return is_authorized_domain_; }
  void set_is_authorized_domain(bool is_authorized) {
    is_authorized_domain_ = is_authorized;
  }

  // Answers question: Are we allowed to rewrite the contents now?
  // Checks if valid and cacheable and if it has a no-transform header.
  // rewrite_uncacheable is used to answer question whether the resource can be
  // optimized even if it is not cacheable.
  // If a resource cannot be rewritten, the reason is appended to *reason.
  bool IsSafeToRewrite(bool rewrite_uncacheable, GoogleString* reason) const;
  bool IsSafeToRewrite(bool rewrite_uncacheable) const {
    // TODO(jmaessen): Convert all remaining call sites to use a reason.
    GoogleString reason_ignored;
    return IsSafeToRewrite(rewrite_uncacheable, &reason_ignored);
  }

  // TODO(sligocki): Do we need these or can we just use IsValidAndCacheable
  // everywhere?
  bool loaded() const { return response_headers_.status_code() != 0; }
  bool HttpStatusOk() const {
    return (response_headers_.status_code() == HttpStatus::kOK);
  }

  // Loads contents of resource asynchronously, calling callback when
  // done.  If the resource contents are already loaded into the object,
  // the callback will be called directly, rather than asynchronously.  The
  // resource will be passed to the callback, with its contents and headers
  // filled in.
  //
  // This is implemented in terms of LoadAndCallback, taking care of the case
  // where the resource is already loaded.
  void LoadAsync(NotCacheablePolicy not_cacheable_policy,
                 const RequestContextPtr& request_context,
                 AsyncCallback* callback);

  // If the resource is about to expire from the cache, re-fetches the
  // resource in background to try to prevent it from expiring.
  //
  // Base implementation does nothing, since most subclasses of this do not
  // use caching.
  virtual void RefreshIfImminentlyExpiring();

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

  // Returns the uncompressed contents stored in value_. Although this is marked
  // as const, it mutates the internal state of this object and is not thread
  // safe.
  StringPiece ExtractUncompressedContents() const;

  // Returns the size of the the ExtractUncompressedContents(). Like
  // ExtractUncompressedContents(), this method can mutate the internal state of
  // the object and is not thread safe.
  size_t UncompressedContentsSize() const {
    StringPiece val = ExtractUncompressedContents();
    return val.length();
  }

  StringPiece raw_contents() const {
    StringPiece val;
    bool got_contents = value_.ExtractContents(&val);
    CHECK(got_contents) << "Resource contents read before loading: "
                        << UrlForDebug();
    return val;
  }

  ResponseHeaders* response_headers() { return &response_headers_; }
  const ResponseHeaders* response_headers() const { return &response_headers_; }
  const ContentType* type() const { return type_; }
  virtual void SetType(const ContentType* type);
  bool IsContentsEmpty() const {
    return raw_contents().empty();
  }

  // Note: this is empty if the header is not specified.
  StringPiece charset() const { return charset_; }
  void set_charset(StringPiece c) { c.CopyToString(&charset_); }

  // Gets the absolute URL of the resource.
  virtual GoogleString url() const = 0;
  // Most resources should have URLs, but inline resources will not and should
  // override this function.
  virtual bool has_url() const { return true; }
  // Override if resource does not have a URL.
  virtual GoogleString UrlForDebug() const { return url(); }

  // Gets the cache key for resource. This may be different from URL
  // if the resource is e.g. UA-dependent.
  virtual GoogleString cache_key() const {
    return url();
  }

  // Computes the content-type (and charset) based on response_headers and
  // extension, and sets it via SetType.
  void DetermineContentType();

  // We define a new Callback type here because we need to
  // pass in the Resource to the Done callback so it can
  // collect the fetched data.
  class AsyncCallback {
   public:
    explicit AsyncCallback(const ResourcePtr& resource) : resource_(resource) {}

    virtual ~AsyncCallback();
    virtual void Done(bool lock_failure, bool resource_ok) = 0;

    const ResourcePtr& resource() { return resource_; }

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

    // This is called with resource_ok = true only if the hash of the fetched
    // response is the same as the hash in input_info()->input_content_hash().
    virtual void Done(bool lock_failure, bool resource_ok) {
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

  FetchResponseStatus fetch_response_status() {
    return fetch_response_status_;
  }

  void set_fetch_response_status(FetchResponseStatus x) {
    fetch_response_status_ = x;
  }

  // Returns whether this type of resource should use the HTTP Cache.  This
  // method is based on properties of the class, not the resource itself, and
  // helps short-circuit pointless cache lookups for file-based and data URLs.
  virtual bool UseHttpCache() const = 0;

 protected:
  virtual ~Resource();
  REFCOUNT_FRIEND_DECLARATION(Resource);
  friend class ServerContext;
  friend class ReadAsyncHttpCacheCallback;  // uses LoadAndCallback
  friend class RewriteDriver;  // for ReadIfCachedWithStatus
  friend class UrlReadAsyncFetchCallback;

  // Load the resource asynchronously, storing ResponseHeaders and
  // contents in object.  Calls 'callback' when finished.  The
  // ResourcePtr used to construct 'callback' must be the same as the
  // resource used to invoke this method.
  //
  // Setting not_cacheable_policy to kLoadEvenIfNotCacheable will permit it
  // to consider loading to be successful on Cache-Control:private and
  // Cache-Control:no-cache resources.  It should not affect /whether/ the
  // callback gets involved, only whether it gets true or false.
  virtual void LoadAndCallback(NotCacheablePolicy not_cacheable_policy,
                               const RequestContextPtr& request_context,
                               AsyncCallback* callback) = 0;

  void set_enable_cache_purge(bool x) { enable_cache_purge_ = x; }
  ResponseHeaders::VaryOption respect_vary() const { return respect_vary_; }
  void set_respect_vary(ResponseHeaders::VaryOption x) { respect_vary_ = x; }
  void set_proactive_resource_freshening(bool x) {
    proactive_resource_freshening_ = x;
  }

  void set_disable_rewrite_on_no_transform(bool x) {
    disable_rewrite_on_no_transform_ = x;
  }
  ServerContext* server_context_;

  const ContentType* type_;
  GoogleString charset_;
  HTTPValue value_;  // contains contents and meta-data
  ResponseHeaders response_headers_;

  // A stale value that can be used in case we aren't able to fetch a fresh
  // version of the resource. Note that this should only be used if it is not
  // empty.
  HTTPValue fallback_value_;

 private:
  // Minimalist constructor for DummyResource with server_context_ == NULL
  // used in association_transformer_test.cc
  Resource();
  friend class DummyResource;

  // The status of the fetched response.
  FetchResponseStatus fetch_response_status_;

  // Indicates whether we are trying to load the resource for a background
  // rewrite or to serve a user request.
  // Note that by default, we assume that every fetch is triggered in the
  // background and is not user-facing unless we explicitly set
  // is_background_fetch_ to false.
  bool is_background_fetch_;
  bool enable_cache_purge_;
  bool proactive_resource_freshening_;
  bool disable_rewrite_on_no_transform_;
  bool is_authorized_domain_;
  ResponseHeaders::VaryOption respect_vary_;
  mutable GoogleString extracted_contents_;
  mutable bool extracted_;

  DISALLOW_COPY_AND_ASSIGN(Resource);
};

// Sometimes some portions of URL space need to be handled differently
// by dedicated resource subclasses. ResourceProvider callbacks are used
// to teach RewriteDriver about these, so it knows not to build regular
// UrlInputResource objects.
typedef Callback2<const GoogleUrl&, bool*> ResourceUrlClaimant;

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_RESOURCE_H_
