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
// Output resources are created by a ResourceManager. They must be able to
// write contents and return their url (so that it can be href'd on a page).

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_OUTPUT_RESOURCE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_OUTPUT_RESOURCE_H_

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/blocking_behavior.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/file_writer.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CachedResult;
class HTTPValue;
class MessageHandler;
class NamedLock;
class ResourceManager;
class RewriteOptions;
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
  OutputResource(ResourceManager* resource_manager,
                 const StringPiece& resolved_base,
                 const ResourceNamer& resource_id,
                 const ContentType* type,
                 const RewriteOptions* options,
                 OutputResourceKind kind);

  virtual bool Load(MessageHandler* message_handler);
  virtual GoogleString url() const;

  // Attempt to obtain a named lock for the resource.  Return true if we do so.
  // If the resource is expensive to create, this lock should be held during
  // its creation to avoid multiple rewrites happening at once.
  // The lock will be unlocked on destruction, DropCreationLock, or EndWrite
  // (called from ResourceManager::Write)
  bool LockForCreation(BlockingBehavior block);

  // Drops the lock created by above, if any.
  void DropCreationLock();

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

  // output-specific
  const GoogleString& resolved_base() const { return resolved_base_; }
  const ResourceNamer& full_name() const { return full_name_; }
  StringPiece name() const { return full_name_.name(); }
  GoogleString filename() const;
  StringPiece suffix() const;
  StringPiece filter_prefix() const { return full_name_.id(); }

  // In a scalable installation where the sprites must be kept in a
  // database, we cannot serve HTML that references new resources
  // that have not been committed yet, and committing to a database
  // may take too long to block on the HTML rewrite.  So we will want
  // to refactor this to check to see whether the desired resource is
  // already known.  For now we'll assume we can commit to serving the
  // resource during the HTML rewriter.
  bool IsWritten() const;

  // Sets the suffix for an output resource.  This must be called prior
  // to Write if the content_type ctor arg was NULL.  This can happen if
  // we are managing a resource whose content-type is not known to us.
  // CacheExtender is currently the only place where we need this.
  void set_suffix(const StringPiece& ext);

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
  // want before calling ResourceManager::Write.
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

  // Transfers up ownership of any cached result and clears pointer to it.
  CachedResult* ReleaseCachedResult() {
    CHECK(cached_result_owned_);
    CachedResult* ret = cached_result_;
    cached_result_ = NULL;
    cached_result_owned_ = false;
    return ret;
  }

  // Resources rewritten via a UrlPartnership will have a resolved
  // base to use in lieu of the legacy UrlPrefix held by the resource
  // manager.
  void set_resolved_base(const StringPiece& base) {
    base.CopyToString(&resolved_base_);
    CHECK(EndsInSlash(base)) << "resolved_base must end in a slash.";
  }

  OutputResourceKind kind() const { return kind_; }

  // TODO(jmarantz): get rid of this bool when RewriteContext is fully deployed.
  bool written_using_rewrite_context_flow() const {
    return written_using_rewrite_context_flow_;
  }
  void set_written_using_rewrite_context_flow(bool x) {
    written_using_rewrite_context_flow_ = x;
  }

  bool has_lock() const { return locked_; }

  // This is called by CacheCallback::Done in rewrite_driver.cc.
  void set_written(bool written) { writing_complete_ = true; }

 protected:
  virtual ~OutputResource();
  REFCOUNT_FRIEND_DECLARATION(OutputResource);

 private:
  friend class ResourceManager;
  friend class ResourceManagerTest;
  friend class ResourceManagerTestingPeer;

  class OutputWriter {
   public:
    // file may be null if we shouldn't write to the filesystem.
    OutputWriter(FileSystem::OutputFile* file, HTTPValue* http_value)
        : http_value_(http_value) {
      if (file != NULL) {
        file_writer_.reset(new FileWriter(file));
      } else {
        file_writer_.reset(NULL);
      }
    }

    // Adds the given data to our http_value, and, if
    // non-null, our file.
    bool Write(const StringPiece& data, MessageHandler* handler);
   private:
    scoped_ptr<FileWriter> file_writer_;
    HTTPValue* http_value_;
  };

  void SetHash(const StringPiece& hash);
  StringPiece extension() const { return full_name_.ext(); }
  StringPiece hash() const { return full_name_.hash(); }
  bool has_hash() const { return !hash().empty(); }
  GoogleString TempPrefix() const;

  OutputWriter* BeginWrite(MessageHandler* message_handler);
  bool EndWrite(OutputWriter* writer, MessageHandler* message_handler);

  // Stores the current state of cached_result in the metadata cache
  // under the given key.
  // Pre-condition: cached_result() != NULL
  void SaveCachedResult(const GoogleString& key, MessageHandler* handler);

  // Loads the state of cached_result from the given cached key if possible,
  // and syncs our URL and content type with it. If fails, cached_result
  // will be set to NULL.
  void FetchCachedResult(const GoogleString& key, MessageHandler* handler);

  FileSystem::OutputFile* output_file_;
  bool writing_complete_;
  bool locked_;

  // TODO(jmarantz): We have a complicated semantic for CachedResult
  // ownership as we transition from rewriting inline while html parsing
  // to rewriting asynchronously.  In the asynchronous world, the
  // CachedResult object will be owned at a higher level.  So it is not
  // safe to call cached_result_.release() or .reset() directly.  Instead,
  // go through the clear_cached_result() method.
  bool cached_result_owned_;
  CachedResult* cached_result_;

  // The resolved_base_ is the domain as reported by UrlPartnership.
  // It takes into account domain-mapping via
  // ModPagespeedMapRewriteDomain.  However, the resolved base is
  // not affected by sharding.  Shard-selection is done when url() is called,
  // relying on the content hash.
  GoogleString resolved_base_;
  ResourceNamer full_name_;

  // Lock guarding resource creation.  Lazily initialized by LockForCreation,
  // unlocked on destruction, DropCreationLock or EndWrite.
  scoped_ptr<NamedLock> creation_lock_;

  // rewrite_options_ is NULL when we are creating an output resource on
  // behalf of a fetch.  This is because there's no point or need to implement
  // sharding on the fetch -- we are not rewriting a URL, we are just decoding
  // it.  However, when rewriting a resources, we need rewrite_options_ to
  // be non-null.
  const RewriteOptions* rewrite_options_;

  // Output resource have a 'kind' associated with them that controls the kind
  // of caching we would like to be performed on them when written out.
  OutputResourceKind kind_;

  // Outputs written using the newer async flow in RewriteContext.cc do not need
  // rname caches, because the meta-data is contained in the OutputPartitions.
  //
  // TODO(jmarantz): when the new flow is completley deployed this bool
  // can be removed.
  bool written_using_rewrite_context_flow_;

  DISALLOW_COPY_AND_ASSIGN(OutputResource);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_OUTPUT_RESOURCE_H_
