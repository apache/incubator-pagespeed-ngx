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
//
// Output resources are created by a RewriteDriver. They must be able to
// write contents and return their url (so that it can be href'd on a page).

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_OUTPUT_RESOURCE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_OUTPUT_RESOURCE_H_

#include "base/logging.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class CachedResult;
class MessageHandler;
class RewriteDriver;
class RewriteOptions;
class Writer;
struct ContentType;

class OutputResource : public Resource {
 public:
  // Construct an OutputResource.  For the moment, we pass in type redundantly
  // even though full_name embeds an extension.  This reflects current code
  // structure rather than a principled stand on anything.
  // TODO(jmaessen): remove redundancy.
  //
  // The 'options' argument can be NULL.  This is done in the Fetch path because
  // that field is only used for domain sharding, and during the fetch, further
  // domain makes no sense.
  OutputResource(const RewriteDriver* driver,
                 StringPiece resolved_base,
                 StringPiece unmapped_base, /* aka source domain */
                 StringPiece original_base, /* aka cnamed domain */
                 const ResourceNamer& resource_id,
                 OutputResourceKind kind);

  virtual void LoadAndCallback(NotCacheablePolicy not_cacheable_policy,
                               const RequestContextPtr& request_context,
                               AsyncCallback* callback);
  // NOTE: url() will crash if resource has does not have a hash set yet.
  // Specifically, this will occur if the resource has not been completely
  // written yet. Before that point, the final URL cannot be known.
  //
  // Note: the OutputResource will never have a query string, even when
  // ModPagespeedAddOptionsToUrls is on.
  virtual GoogleString url() const;
  // Returns the same as url(), but with a spoofed hash in case no hash
  // was set yet. Use this for error reporting, etc. where you do not
  // know whether the output resource has a valid hash yet.
  GoogleString UrlEvenIfHashNotSet();

  // Save resource contents to disk, for testing and debugging purposes.
  // Precondition: the resource contents must be fully set.
  // The resource will be saved under the resource manager's filename_prefix()
  // using with URL escaped using its filename_encoder().
  void DumpToDisk(MessageHandler* handler);

  // Update the passed in CachedResult from the CachedResult in this
  // OutputResource.
  void UpdateCachedResultPreservingInputInfo(CachedResult* to_update) const;

  // The NameKey describes the source url and rewriter used, without hash and
  // content type information.  This is used to find previously-computed filter
  // results whose output hash and content type is unknown.  The full name of a
  // resource is of the form
  //    path/prefix.encoded_resource_name.hash.extension
  // we know prefix and name, but not the hash, and we don't always even have
  // the extension, which might have changes as the result of, for example image
  // optimization (e.g. gif->png).  But We can "remember" the hash/extension for
  // as long as the origin URL was cacheable.  So we construct this as a key:
  //    path/prefix.encoded_resource_name
  // and use that to map to the hash-code and extension.  If we know the
  // hash-code then we may also be able to look up the contents in the same
  // cache.
  virtual GoogleString name_key() const;

  // Builds a canonical URL in a form for use with the HTTP cache.
  // The DomainLawyer from options is used to find the proper domain in case
  // there is a fetch for the unsharded form, or the wrong shard.
  //
  // For example, if you have a resource styles.css
  //     ModPagespeedMapRewriteDomain master alias
  //     ModPagespeedShardDomain master shard1,shard2
  // then all HTTP cache puts/gets will use the key "http://master/style.css",
  // which can be obtained from an output resource using this method.
  GoogleString HttpCacheKey() const;

  // output-specific
  const GoogleString& resolved_base() const { return resolved_base_; }
  const GoogleString& unmapped_base() const { return unmapped_base_; }
  const GoogleString& original_base() const { return original_base_; }
  const ResourceNamer& full_name() const { return full_name_; }
  ResourceNamer* mutable_full_name() { return &full_name_; }
  StringPiece name() const { return full_name_.name(); }
  StringPiece experiment() const { return full_name_.experiment(); }
  StringPiece suffix() const;
  StringPiece filter_prefix() const { return full_name_.id(); }
  StringPiece hash() const { return full_name_.hash(); }
  StringPiece signature() const { return full_name_.signature(); }
  bool has_hash() const { return !hash().empty(); }
  void clear_hash() {
    full_name_.ClearHash();
    computed_url_.clear();
  }

  // Some output resources have mangled names derived from input resource(s),
  // such as when combining CSS files.  When we need to regenerate the output
  // resource given just its URL we need to convert the URL back to its
  // constituent input resource URLs.  Our url() method can return a modified
  // version of the input resources' host and path if our resource manager
  // has a non-standard url_namer(), so when trying to regenerate the input
  // resources' URL we need to reverse that modification.  Note that the
  // default UrlNamer class doesn't do any modification, and that the decoding
  // of the leaf names is done separetly by the UrlMultipartEncoder class.
  GoogleString decoded_base() const;

  // In a scalable installation where the sprites must be kept in a
  // database, we cannot serve HTML that references new resources
  // that have not been committed yet, and committing to a database
  // may take too long to block on the HTML rewrite.  So we will want
  // to refactor this to check to see whether the desired resource is
  // already known.  For now we'll assume we can commit to serving the
  // resource during the HTML rewriter.
  bool IsWritten() const { return writing_complete_; }

  // Sets the type of the output resource, and thus also its suffix.
  virtual void SetType(const ContentType* type);

  // Whenever output resources are created via RewriteDriver
  // (except DecodeOutputResource) it looks up cached
  // information on any previous creation of that resource, including
  // the full filename and any filter-specific metadata. If such
  // information is available, this method will return non-NULL.
  //
  // Note: cached_result() will also be non-NULL if you explicitly
  // create the result from a filter by calling EnsureCachedResultCreated()
  //
  // The output is const because we do not check that the CachedResult has not
  // been written.  If you want to modify the CachedResult, use
  // EnsureCachedResultCreated instead.
  const CachedResult* cached_result() const { return cached_result_; }

  // If there is no cached output information, creates an empty one,
  // without any information filled in (so no url(), or timestamps).
  //
  // The primary use of this method is to let filters store any metadata they
  // want before calling RewriteDriver::Write.
  // This never returns null.
  // We will DCHECK that the cached result has not been written.
  CachedResult* EnsureCachedResultCreated();

  void clear_cached_result();

  // Sets the cached-result to an already-existing, externally owned
  // buffer.  We need to make sure not to free it on destruction.
  void set_cached_result(CachedResult* cached_result) {
    clear_cached_result();
    cached_result_ = cached_result;
  }

  OutputResourceKind kind() const { return kind_; }

  // This is called by CacheCallback::Done in rewrite_driver.cc.
  void SetWritten(bool written) { writing_complete_ = true; }

  virtual const RewriteOptions* rewrite_options() const {
    return rewrite_options_;
  }

  // Interface for directly setting the value of the resource.
  // It must not have been set otherwise! The return value of
  // BeginWrite is owned by this OutputResource.
  Writer* BeginWrite(MessageHandler* message_handler);
  void EndWrite(MessageHandler* message_handler);

  virtual bool UseHttpCache() const { return true; }

  // Extra suffix to be added to Cache-Control in the response headers
  // when serving the response.  E.g. a filter might want to set
  // no-transform on its output.
  const GoogleString& cache_control_suffix() const {
    return cache_control_suffix_;
  }
  void set_cache_control_suffix(const GoogleString& x) {
    DCHECK(cache_control_suffix_.empty());
    cache_control_suffix_ = x;
  }

 protected:
  virtual ~OutputResource();
  REFCOUNT_FRIEND_DECLARATION(OutputResource);

 private:
  friend class RewriteDriver;
  friend class ServerContext;
  friend class ServerContextTest;

  void SetHash(StringPiece hash);
  StringPiece extension() const { return full_name_.ext(); }
  GoogleString ComputeSignature();
  bool CheckSignature();

  // Name of the file used by DumpToDisk.
  GoogleString DumpFileName() const;

  bool writing_complete_;

  // TODO(jmarantz): We have a complicated semantic for CachedResult
  // ownership as we transition from rewriting inline while html parsing
  // to rewriting asynchronously.  In the asynchronous world, the
  // CachedResult object will be owned at a higher level.  So it is not
  // safe to call cached_result_.release() or .reset() directly.  Instead,
  // go through the clear_cached_result() method.
  bool cached_result_owned_;
  CachedResult* cached_result_;

  // The resolved_base_ is the domain as reported by UrlPartnership.
  //   It takes into account domain-mapping via ModPagespeedMapRewriteDomain.
  //   However, the resolved base is not affected by sharding.  Shard-selection
  //   is done when url() is called, relying on the content hash.
  // The unmapped_base_ is the same domain as resolved_base_ but before domain
  //   mapping was applied; it is also known as the source domain since it is
  //   the domain of the resource's link.
  // The original_base_ is the domain of the page that contains the resource
  //   link; it is also known as the CNAMEd domain since the page's URL is
  //   one that we manage and is one that we are rwriting.
  // For example, given an HTML page with URL http://www.example.com/index.html
  // containing elements "<base href='http://static.example.com/'>" and
  // "<link rel='stylesheet' href='styles.css'>", and also a rule rewriting
  // static.example.com -> cdn.com/example/static, then the OutputResource for
  // the link element's href will have:
  //   resolved_base_ == http://cdn.com/example/static/
  //   unmapped_base_ == http://static.example.com/
  //   original_base_ == http://www.example.com/
  GoogleString resolved_base_;
  GoogleString unmapped_base_;
  GoogleString original_base_;

  GoogleString cache_control_suffix_;

  ResourceNamer full_name_;

  // Lazily evaluated and cached result of the url() method, which is const.
  mutable GoogleString computed_url_;

  const RewriteOptions* rewrite_options_;

  // Output resource have a 'kind' associated with them that controls the kind
  // of caching we would like to be performed on them when written out.
  OutputResourceKind kind_;

  DISALLOW_COPY_AND_ASSIGN(OutputResource);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_OUTPUT_RESOURCE_H_
