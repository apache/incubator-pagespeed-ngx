/*
 * Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)
//
// Note: when making changes to this file, a very good sanity-check to run,
// once tests pass, is:
//
// valgrind --leak-check=full ..../src/out/Debug/pagespeed_automatic_test
//     "--gtest_filter=RewriteContextTest*"

#include "net/instaweb/rewriter/public/rewrite_context.h"

#include <cstddef>                     // for size_t
#include <algorithm>
#include <utility>                      // for pair
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_single_resource_filter.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/queued_alarm.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/url_segment_encoder.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

class RewriteFilter;

const char kRewriteContextLockPrefix[] = "rc:";

// Two callback classes for completed caches & fetches.  These gaskets
// help RewriteContext, which knows about all the pending inputs,
// trigger the rewrite once the data is available.  There are two
// versions of the callback.

// Callback to wake up the RewriteContext when the partitioning is looked up
// in the cache.  The RewriteContext can then decide whether to queue the
// output-resource for a DOM update, or re-initiate the Rewrite, depending
// on the metadata returned.
class RewriteContext::OutputCacheCallback : public CacheInterface::Callback {
 public:
  typedef void (RewriteContext::*CacheResultHandlerFunction)(
      CacheInterface::KeyState, SharedString value);

  OutputCacheCallback(RewriteContext* rc, CacheResultHandlerFunction function)
      : rewrite_context_(rc), function_(function) {}
  virtual ~OutputCacheCallback() {}
  virtual void Done(CacheInterface::KeyState state) {
    RewriteDriver* rewrite_driver = rewrite_context_->Driver();
    rewrite_driver->AddRewriteTask(MakeFunction(
        rewrite_context_, function_, state, *value()));
    delete this;
  }

 private:
  RewriteContext* rewrite_context_;
  CacheResultHandlerFunction function_;
};

// Bridge class for routing cache callbacks to RewriteContext methods
// in rewrite thread. Note that the receiver will have to delete the callback
// (which we pass to provide access to data without copying it)
class RewriteContext::HTTPCacheCallback : public OptionsAwareHTTPCacheCallback {
 public:
  typedef void (RewriteContext::*HTTPCacheResultHandlerFunction)(
      HTTPCache::FindResult, HTTPCache::Callback* data);

  HTTPCacheCallback(RewriteContext* rc, HTTPCacheResultHandlerFunction function)
      : OptionsAwareHTTPCacheCallback(rc->Options()),
        rewrite_context_(rc),
        function_(function) {}
  virtual ~HTTPCacheCallback() {}
  virtual void Done(HTTPCache::FindResult find_result) {
    RewriteDriver* rewrite_driver = rewrite_context_->Driver();
    rewrite_driver->AddRewriteTask(MakeFunction(
        rewrite_context_, function_, find_result,
        static_cast<HTTPCache::Callback*>(this)));
  }

 private:
  RewriteContext* rewrite_context_;
  HTTPCacheResultHandlerFunction function_;
  DISALLOW_COPY_AND_ASSIGN(HTTPCacheCallback);
};

// Common code for invoking RewriteContext::ResourceFetchDone for use
// in ResourceFetchCallback and ResourceReconstructCallback.
class RewriteContext::ResourceCallbackUtils {
 public:
  ResourceCallbackUtils(RewriteContext* rc, const ResourcePtr& resource,
                        int slot_index)
      : resource_(resource),
        rewrite_context_(rc),
        slot_index_(slot_index) {
  }

  void Done(bool success) {
    RewriteDriver* rewrite_driver = rewrite_context_->Driver();
    rewrite_driver->AddRewriteTask(
        MakeFunction(rewrite_context_, &RewriteContext::ResourceFetchDone,
                     success, resource_, slot_index_));
  }

 private:
  ResourcePtr resource_;
  RewriteContext* rewrite_context_;
  int slot_index_;
};

// Callback when reading a resource from the network.
class RewriteContext::ResourceFetchCallback : public Resource::AsyncCallback {
 public:
  ResourceFetchCallback(RewriteContext* rc, const ResourcePtr& r,
                        int slot_index)
      : Resource::AsyncCallback(r),
        delegate_(rc, r, slot_index) {
  }

  virtual ~ResourceFetchCallback() {}
  virtual void Done(bool success) {
    delegate_.Done(success);
    delete this;
  }

  virtual bool EnableThreaded() const { return true; }

 private:
  ResourceCallbackUtils delegate_;
};

// Callback used when we need to reconstruct a resource we made to satisfy
// a fetch (due to rewrites being nested inside each other).
class RewriteContext::ResourceReconstructCallback : public AsyncFetch {
 public:
  // Takes ownership of the driver (e.g. will call Cleanup)
  ResourceReconstructCallback(RewriteDriver* driver, RewriteContext* rc,
                              const OutputResourcePtr& resource, int slot_index)
      : driver_(driver),
        delegate_(rc, ResourcePtr(resource), slot_index),
        resource_(resource) {
  }

  virtual ~ResourceReconstructCallback() {
  }

  virtual void HandleDone(bool success) {
    // Make sure to release the lock here, as in case of nested reconstructions
    // that fail it would otherwise only get released on ~OutputResource, which
    // in turn will only happen once the top-level is done, which may take a
    // while.
    resource_->DropCreationLock();

    delegate_.Done(success);
    driver_->Cleanup();
    delete this;
  }

  // We ignore the output here as it's also put into the resource itself.
  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    return true;
  }
  virtual bool HandleFlush(MessageHandler* handler) {
    return true;
  }
  virtual void HandleHeadersComplete() {}

 private:
  RewriteDriver* driver_;
  ResourceCallbackUtils delegate_;
  OutputResourcePtr resource_;
};

// Callback used when we re-check validity of cached results by contents.
class RewriteContext::ResourceRevalidateCallback
    : public Resource::AsyncCallback {
 public:
  ResourceRevalidateCallback(RewriteContext* rc, const ResourcePtr& r,
                             InputInfo* input_info)
      : Resource::AsyncCallback(r),
        rewrite_context_(rc),
        input_info_(input_info) {
  }

  virtual ~ResourceRevalidateCallback() {
  }

  virtual void Done(bool success) {
    RewriteDriver* rewrite_driver = rewrite_context_->Driver();
    rewrite_driver->AddRewriteTask(
        MakeFunction(rewrite_context_, &RewriteContext::ResourceRevalidateDone,
                     input_info_, success));
    delete this;
  }

  virtual bool EnableThreaded() const { return true; }

 private:
  RewriteContext* rewrite_context_;
  InputInfo* input_info_;
};

// This class encodes a few data members used for responding to
// resource-requests when the output_resource is not in cache.
class RewriteContext::FetchContext {
 public:
  FetchContext(RewriteContext* rewrite_context,
               AsyncFetch* fetch,
               const OutputResourcePtr& output_resource,
               MessageHandler* handler)
      : rewrite_context_(rewrite_context),
        async_fetch_(fetch),
        output_resource_(output_resource),
        handler_(handler),
        deadline_alarm_(NULL),
        success_(false),
        detached_(false) {
  }

  void SetupDeadlineAlarm() {
    // No point in doing this for on-the-fly resources.
    if (rewrite_context_->kind() == kOnTheFlyResource) {
      return;
    }

    // Can't do this if a subclass forced us to be detached already.
    if (detached_) {
      return;
    }
    RewriteDriver* driver = rewrite_context_->Driver();
    Timer* timer = rewrite_context_->Manager()->timer();

    // Startup an alarm which will cause us to return unrewritten content
    // rather than hold up the fetch too long on firing. We use a longer
    // deadline here than for rendering because we are being asked for the
    // rewritten version, so the tradeoff is shifted a bit more towards
    // rewriting.
    deadline_alarm_ =
        new QueuedAlarm(
            driver->scheduler(), driver->rewrite_worker(),
            timer->NowUs() + 2 * driver->rewrite_deadline_ms() * Timer::kMsUs,
            MakeFunction(this, &FetchContext::HandleDeadline));
  }

  // Must be invoked from main rewrite thread.
  void CancelDeadlineAlarm() {
    if (deadline_alarm_ != NULL) {
      deadline_alarm_->CancelAlarm();
      deadline_alarm_ = NULL;
    }
  }

  // Fired by QueuedAlarm in main rewrite thread.
  void HandleDeadline() {
    deadline_alarm_ = NULL;  // avoid dangling reference.
    rewrite_context_->DetachFetch();
    handler_->Message(kError, "Deadline exceeded for resource %s",
                      output_resource_->UrlEvenIfLeafInvalid().c_str());
    // TODO(sligocki): Log a variable for number of deadline hits.
    ResourcePtr input(rewrite_context_->slot(0)->resource());
    bool absolutify_contents = true;
    FetchFallbackDoneImpl(input->contents(), input->response_headers(),
                          absolutify_contents);
  }

  // Note that the callback is called from the RewriteThread.
  void FetchDone() {
    CancelDeadlineAlarm();

    // Cache our results.
    DCHECK_EQ(1, rewrite_context_->num_output_partitions());
    rewrite_context_->WritePartition();

    // If we're running in background, that's basically all we will do.
    if (detached_) {
      rewrite_context_->Driver()->DetachedFetchComplete();
      return;
    }

    GoogleString output;
    bool ok = false;
    ResponseHeaders* response_headers = async_fetch_->response_headers();
    if (success_) {
      if (output_resource_->hash() == requested_hash_) {
        response_headers->CopyFrom(*(
            output_resource_->response_headers()));
        // Use the most conservative Cache-Control considering all inputs.
        ApplyInputCacheControl(response_headers);
        async_fetch_->HeadersComplete();
        ok = async_fetch_->Write(output_resource_->contents(), handler_);
      } else {
        // Our rewrite produced a different hash than what was requested;
        // we better not give it an ultra-long TTL.
        FetchFallbackDone(output_resource_->contents(),
                          output_resource_->response_headers());
        return;
      }
    } else {
      // Rewrite failed. If we have a single original, write it out instead.
      // NOTE: CSS filter can "fail" rewriting because it decides
      // that the file is not optimizable (can't make it smaller, etc.).
      //
      // TODO(sligocki): We should probably not do that in the fetch path.
      // For example, we could set_always_rewrite_css(true) for fetches.
      // On the other hand, it might be difficult to keep that from dirtying
      // the cache. So maybe we shouldn't do it.
      if (rewrite_context_->num_slots() == 1) {
        ResourcePtr input_resource(rewrite_context_->slot(0)->resource());
        if (input_resource.get() != NULL && input_resource->ContentsValid()) {
          handler_->Message(kError, "Rewrite %s failed while fetching %s",
                            input_resource->url().c_str(),
                            output_resource_->UrlEvenIfLeafInvalid().c_str());
          // TODO(sligocki): Log variable for number of failed rewrites in
          // fetch path.

          response_headers->CopyFrom(*input_resource->response_headers());
          rewrite_context_->FixFetchFallbackHeaders(response_headers);
          async_fetch_->HeadersComplete();

          ok = rewrite_context_->AbsolutifyIfNeeded(
              input_resource->contents(), async_fetch_, handler_);
        } else {
          GoogleString url = input_resource->url();
          handler_->Error(
              output_resource_->name().as_string().c_str(), 0,
              "Resource based on %s but cannot access the original",
              url.c_str());
        }
      }
    }

    if (!ok) {
      async_fetch_->response_headers()->SetStatusAndReason(
          HttpStatus::kNotFound);
      // TODO(sligocki): We could be calling this twice if Writes fail above.
      async_fetch_->HeadersComplete();
    }
    rewrite_context_->FetchCallbackDone(ok);
  }

  // This is used in case we used a metadata cache to find an alternative URL
  // to serve --- either a version with a different hash, or that we should
  // serve the original. In this case, we serve it out, but with shorter headers
  // than usual.
  void FetchFallbackDone(const StringPiece& contents,
                         ResponseHeaders* headers) {
    CancelDeadlineAlarm();
    if (detached_) {
      rewrite_context_->Driver()->DetachedFetchComplete();
      return;
    }

    bool absolutify_contents = false;
    FetchFallbackDoneImpl(contents, headers, absolutify_contents);
  }

  // Backend for FetchFallbackCacheDone, but can be also invoked
  // for main rewrite when background rewrite is detached.
  void FetchFallbackDoneImpl(const StringPiece& contents,
                             ResponseHeaders* headers,
                             bool absolutify_contents) {
    rewrite_context_->FixFetchFallbackHeaders(headers);
    // Use the most conservative Cache-Control considering all inputs.
    ApplyInputCacheControl(headers);

    async_fetch_->response_headers()->CopyFrom(*headers);
    async_fetch_->HeadersComplete();

    bool ok;
    if (absolutify_contents) {
      ok = rewrite_context_->AbsolutifyIfNeeded(contents, async_fetch_,
                                                handler_);
    } else {
      ok = async_fetch_->Write(contents, handler_);
    }

    rewrite_context_->FetchCallbackDone(ok);
  }

  void set_requested_hash(const StringPiece& hash) {
    hash.CopyToString(&requested_hash_);
  }

  AsyncFetch* async_fetch() { return async_fetch_; }
  bool detached() const { return detached_; }
  MessageHandler* handler() { return handler_; }
  OutputResourcePtr output_resource() { return output_resource_; }
  const GoogleString& requested_hash() const { return requested_hash_; }

  void set_success(bool success) { success_ = success; }
  void set_detached(bool value) { detached_ = value; }

 private:
  // Computes the most restrictive Cache-Control intersection of the input
  // resources, and the provided headers, and sets that cache-control on the
  // provided headers.  Does nothing if all of the resources are fully
  // cacheable, since in that case we will want to cache-extend.
  //
  // Disregards Cache-Control directives other than max-age, no-cache, no-store,
  // and private, and strips them if any resource is no-cache or private.  By
  // assumption, a resource can only be no-store if it is also no-cache.
  void ApplyInputCacheControl(ResponseHeaders* headers) {
    headers->ComputeCaching();
    bool proxy_cacheable = headers->IsProxyCacheable();
    bool cacheable = headers->IsCacheable();
    bool no_store = headers->HasValue(HttpAttributes::kCacheControl,
                                      "no-store");
    int64 max_age = headers->cache_ttl_ms();
    for (int i = 0; i < rewrite_context_->num_slots(); i++) {
      ResourcePtr input_resource(rewrite_context_->slot(i)->resource());
      if (input_resource.get() != NULL && input_resource->ContentsValid()) {
        ResponseHeaders* input_headers = input_resource->response_headers();
        input_headers->ComputeCaching();
        if (input_headers->cache_ttl_ms() < max_age) {
          max_age = input_headers->cache_ttl_ms();
        }
        proxy_cacheable &= input_headers->IsProxyCacheable();
        cacheable &= input_headers->IsCacheable();
        no_store |= input_headers->HasValue(HttpAttributes::kCacheControl,
                                            "no-store");
      }
    }
    if (cacheable) {
      if (proxy_cacheable) {
        return;
      } else {
        headers->SetDateAndCaching(headers->date_ms(), max_age, ",private");
      }
    } else {
      GoogleString directives = ",no-cache";
      if (no_store) {
        directives += ",no-store";
      }
      headers->SetDateAndCaching(headers->date_ms(), 0, directives);
    }
    headers->ComputeCaching();
  }

  RewriteContext* rewrite_context_;
  AsyncFetch* async_fetch_;
  OutputResourcePtr output_resource_;
  MessageHandler* handler_;
  GoogleString requested_hash_;  // hash we were requested as. May be empty.
  QueuedAlarm* deadline_alarm_;

  bool success_;
  bool detached_;

  DISALLOW_COPY_AND_ASSIGN(FetchContext);
};

// Helper for running filter's Rewrite method in low-priority rewrite thread,
// which deals with cancellation of rewrites due to load shedding or shutdown by
// introducing a kTooBusy response if the job gets dumped.
class RewriteContext::InvokeRewriteFunction : public Function {
 public:
  InvokeRewriteFunction(RewriteContext* context, int partition)
      : context_(context), partition_(partition) {}

  virtual ~InvokeRewriteFunction() {}

  virtual void Run() {
    context_->Rewrite(partition_,
                      context_->partitions_->mutable_partition(partition_),
                      context_->outputs_[partition_]);
  }

  virtual void Cancel() {
    context_->RewriteDone(RewriteSingleResourceFilter::kTooBusy, partition_);
  }

 private:
  RewriteContext* context_;
  int partition_;
};

RewriteContext::RewriteContext(RewriteDriver* driver,
                               RewriteContext* parent,
                               ResourceContext* resource_context)
  : started_(false),
    outstanding_fetches_(0),
    outstanding_rewrites_(0),
    resource_context_(resource_context),
    num_pending_nested_(0),
    parent_(parent),
    driver_(driver),
    num_predecessors_(0),
    chained_(false),
    rewrite_done_(false),
    ok_to_write_output_partitions_(true),
    was_too_busy_(false),
    slow_(false),
    revalidate_ok_(true),
    notify_driver_on_fetch_done_(false),
    force_rewrite_(false) {
  partitions_.reset(new OutputPartitions);
}

RewriteContext::~RewriteContext() {
  DCHECK_EQ(0, num_predecessors_);
  DCHECK_EQ(0, outstanding_fetches_);
  DCHECK(successors_.empty());
  STLDeleteElements(&nested_);
}

int RewriteContext::num_output_partitions() const {
  return partitions_->partition_size();
}

const CachedResult* RewriteContext::output_partition(int i) const {
  return &partitions_->partition(i);
}

CachedResult* RewriteContext::output_partition(int i) {
  return partitions_->mutable_partition(i);
}

void RewriteContext::AddSlot(const ResourceSlotPtr& slot) {
  CHECK(!started_);

  // TODO(jmarantz): eliminate this transitional code to allow JavascriptFilter
  // to straddle the old rewrite flow and the new async flow.
  if (slot.get() == NULL) {
    return;
  }

  slots_.push_back(slot);
  render_slots_.push_back(false);

  RewriteContext* predecessor = slot->LastContext();
  if (predecessor != NULL) {
    // Note that we don't check for duplicate connections between this and
    // predecessor.  They'll all get counted.
    DCHECK(!predecessor->started_);
    predecessor->successors_.push_back(this);
    ++num_predecessors_;
    chained_ = true;
  }
  slot->AddContext(this);
}

void RewriteContext::RemoveLastSlot() {
  int index = num_slots() - 1;
  slot(index)->DetachContext(this);
  RewriteContext* predecessor = slot(index)->LastContext();
  if (predecessor) {
    predecessor->successors_.erase(
        std::find(predecessor->successors_.begin(),
                  predecessor->successors_.end(), this));
    --num_predecessors_;
  }

  slots_.erase(slots_.begin() + index);
  render_slots_.erase(render_slots_.begin() + index);
}

void RewriteContext::Initiate() {
  CHECK(!started_);
  DCHECK_EQ(0, num_predecessors_);
  Driver()->AddRewriteTask(new MemberFunction0<RewriteContext>(
      &RewriteContext::Start, this));
}

// Initiate a Rewrite if it's ready to be started.  A Rewrite would not
// be startable if was operating on a slot that was already associated
// with another Rewrite.  We would wait for all the preceding rewrites
// to complete before starting this one.
void RewriteContext::Start() {
  DCHECK(!started_);
  DCHECK_EQ(0, num_predecessors_);
  started_ = true;

  // The best-case scenario for a Rewrite is that we have already done
  // it, and just need to look up in our metadata cache what the final
  // rewritten URL is.  In the simplest scenario, we are doing a
  // simple URL substitution.  In a more complex example, we have M
  // css files that get reduced to N combinations.  The
  // OutputPartitions held in the cache tells us that, and we don't
  // need to get any data about the resources that need to be
  // rewritten.  But in either case, we only need one cache lookup.
  //
  // Note that the output_key_name is not necessarily the same as the
  // name of the output.
  // Write partition to metadata cache.
  CacheInterface* metadata_cache = Manager()->metadata_cache();
  SetPartitionKey();

  // See if some other handler already had to do an identical rewrite.
  RewriteContext* previous_handler =
      Driver()->RegisterForPartitionKey(partition_key_, this);
  if (previous_handler == NULL) {
    // When the cache lookup is finished, OutputCacheDone will be called.
    if (force_rewrite_) {
      // Make the metadata cache lookup fail since we want to force a rewrite.
       (new OutputCacheCallback(
           this, &RewriteContext::OutputCacheDone))->Done(
               CacheInterface::kNotFound);
    } else {
      metadata_cache->Get(
          partition_key_, new OutputCacheCallback(
              this, &RewriteContext::OutputCacheDone));
    }
  } else {
    if (previous_handler->slow()) {
      MarkSlow();
    }
    previous_handler->repeated_.push_back(this);
  }
}

namespace {

// Hashes a string into (we expect) a base-64-encoded sequence.  Then
// inserts a "/" after the first character.  The theory is that for
// inlined and combined resources, there is no useful URL hierarchy,
// and we want to avoid creating, in the file-cache, a gigantic flat
// list of names.
//
// We do this split after one character so we just get 64
// subdirectories.  If we have too many subdirectories then the
// file-system will not cache the metadata efficiently.  If we have
// too few then the directories get very large.  The main limitation
// we are working against is in pre-ext4 file systems, there are a
// maximum of 32k subdirectories per directory, and there is not an
// explicit limitation on the number of file.  Additioanlly,
// old file-systems may not be efficiently indexed, in which case
// adding some hierarchy should help.
GoogleString HashSplit(const Hasher* hasher, const StringPiece& str) {
  GoogleString hash_buffer = hasher->Hash(str);
  StringPiece hash(hash_buffer);
  return StrCat(hash.substr(0, 1), "/", hash.substr(1));
}

}  // namespace

void RewriteContext::SetPartitionKey() {
  // In Apache, we are populating a file-cache.  To be friendly to
  // the file system, we want to structure it as follows:
  //
  //   rname/id_signature/encoded_filename
  //
  // Performance constraints:
  //   - max 32k links (created by ".." link from subdirectories) per directory
  //   - avoid excessive high-entropy hierarchy as it will not play well with
  //     the filesystem metadata cache.
  //
  // The natural hierarchy in URLs should be exploited for single-resource
  // rewrites; and in fact the http cache uses that, so it can't be too bad.
  //
  // Data URLs & combined URLs should be encoded & hashed because they lack
  // a useful natural hierarchy to reflect in the file-system.
  //
  // We need to run the URL encoder in order to serialize the
  // resource_context_, but this flattens the hierarchy by encoding
  // slashes.  We want the FileCache hierarchies to reflect the URL
  // hierarchies if possible.  So we use a dummy URL of "" in our
  // url-list for now.
  StringVector urls;
  const Hasher* hasher = Manager()->lock_hasher();
  GoogleString url;
  GoogleString signature = hasher->Hash(Options()->signature());
  GoogleString suffix = CacheKeySuffix();

  if (num_slots() == 1) {
    // Usually a resource-context-specific encoding such as the
    // image dimension will be placed ahead of the URL.  However,
    // in the cache context, we want to put it at the end, so
    // put this encoding right before any context-specific suffix.
    urls.push_back("");
    GoogleString encoding;
    encoder()->Encode(urls, resource_context_.get(), &encoding);
    suffix = StrCat(encoding, "@", suffix);

    url = slot(0)->resource()->url();
    if (StringPiece(url).starts_with("data:")) {
      url = HashSplit(hasher, url);
    }
  } else if (num_slots() == 0) {
    // Ideally we should not be writing cache entries for 0-slot
    // contexts.  However that is currently the case for
    // image-spriting.  It would be preferable to avoid creating an
    // identical empty encoding here for every degenerate sprite
    // attempt, but for the moment we can at least make all the
    // encodings the same so they can share the same cache entry.
    // Note that we clear out the suffix to avoid having separate
    // entries for each CSS files that lacks any images.
    //
    // TODO(morlovich): Maksim has a fix in progress which will
    // eliminate this case.
    suffix.clear();
    url = "empty";
  } else {
    for (int i = 0, n = num_slots(); i < n; ++i) {
      ResourcePtr resource(slot(i)->resource());
      urls.push_back(resource->url());
    }
    encoder()->Encode(urls, resource_context_.get(), &url);
    url = HashSplit(hasher, url);
  }

  partition_key_ = StrCat(ResourceManager::kCacheKeyResourceNamePrefix,
                          id(), "_", signature, "/",
                          url, "@", suffix);
}

// Check if this mapping from input to output URLs is still valid; and if not
// if we can re-check based on content.
bool RewriteContext::IsCachedResultValid(CachedResult* partition,
                                         bool* can_revalidate,
                                         InputInfoStarVector* revalidate) {
  bool valid = true;
  *can_revalidate = true;
  for (int j = 0, m = partition->input_size(); j < m; ++j) {
    const InputInfo& input_info = partition->input(j);
    if (!IsInputValid(input_info)) {
      valid = false;
      // We currently do not attempt to re-check file-based resources
      // based on contents; as mtime is a lot more reliable than
      // cache expiration, and permitting 'touch' to force recomputation
      // is potentially useful.
      if (input_info.has_input_content_hash() && input_info.has_index() &&
          (input_info.type() == InputInfo::CACHED)) {
        revalidate->push_back(partition->mutable_input(j));
      } else {
        *can_revalidate = false;
        // No point in checking further.
        return false;
      }
    }
  }
  return valid;
}

bool RewriteContext::IsOtherDependencyValid(
    const OutputPartitions* partitions) {
  for (int j = 0, m = partitions->other_dependency_size(); j < m; ++j) {
    if (!IsInputValid(partitions->other_dependency(j))) {
      return false;
    }
  }
  return true;
}

void RewriteContext::AddRecheckDependency() {
  int64 now_ms = Manager()->timer()->NowMs();
  InputInfo* force_recheck = partitions_->add_other_dependency();
  force_recheck->set_type(InputInfo::CACHED);
  force_recheck->set_expiration_time_ms(
      now_ms + ResponseHeaders::kImplicitCacheTtlMs);
}

bool RewriteContext::IsInputValid(const InputInfo& input_info) {
  switch (input_info.type()) {
    case InputInfo::CACHED: {
      // It is invalid if cacheable inputs have expired or ...
      DCHECK(input_info.has_expiration_time_ms());
      if (!input_info.has_expiration_time_ms()) {
        return false;
      }
      int64 now_ms = Manager()->timer()->NowMs();
      return (now_ms <= input_info.expiration_time_ms());
      break;
    }
    case InputInfo::FILE_BASED: {
      // ... if file-based inputs have changed.
      DCHECK(input_info.has_last_modified_time_ms() &&
             input_info.has_filename());
      if (!input_info.has_last_modified_time_ms() ||
          !input_info.has_filename()) {
        return false;
      }
      int64 mtime_sec;
      Manager()->file_system()->Mtime(input_info.filename(), &mtime_sec,
                                      Manager()->message_handler());
      return (mtime_sec * Timer::kSecondMs ==
                input_info.last_modified_time_ms());
      break;
    }
    case InputInfo::ALWAYS_VALID:
      return true;
  }

  DLOG(FATAL) << "Corrupt InputInfo object !?";
  return false;
}

bool RewriteContext::TryDecodeCacheResult(CacheInterface::KeyState state,
                                          const SharedString& value,
                                          bool* can_revalidate,
                                          InputInfoStarVector* revalidate) {
  if (state != CacheInterface::kAvailable) {
    Manager()->rewrite_stats()->cached_output_misses()->Add(1);
    *can_revalidate = false;
    return false;
  }

  // We've got a hit on the output metadata; the contents should
  // be a protobuf.  Try to parse it.
  const GoogleString* val_str = value.get();
  ArrayInputStream input(val_str->data(), val_str->size());
  if (partitions_->ParseFromZeroCopyStream(&input) &&
      IsOtherDependencyValid(partitions_.get())) {
    bool ok = true;
    *can_revalidate = true;
    for (int i = 0, n = partitions_->partition_size(); i < n; ++i) {
      CachedResult* partition = partitions_->mutable_partition(i);
      bool can_revalidate_resource;
      if (!IsCachedResultValid(partition, &can_revalidate_resource,
                               revalidate)) {
        ok = false;
        *can_revalidate = *can_revalidate && can_revalidate_resource;
      }
    }
    return ok;
  } else {
    // This case includes both corrupt protobufs and the case where
    // external dependencies are invalid. We do not attempt to reuse
    // rewrite results by input content hashes even in the second
    // case as that would require us to try to re-fetch those URLs as well.
    // TODO(jmarantz): count cache corruptions in a stat?
    *can_revalidate = false;
    return false;
  }
}

void RewriteContext::OutputCacheDone(CacheInterface::KeyState state,
                                     SharedString value) {
  DCHECK_LE(0, outstanding_fetches_);
  DCHECK_EQ(static_cast<size_t>(0), outputs_.size());

  bool cache_ok, can_revalidate;
  InputInfoStarVector revalidate;
  cache_ok = TryDecodeCacheResult(state, value, &can_revalidate, &revalidate);
  // If OK or worth rechecking, set things up for the cache hit case.
  if (cache_ok || can_revalidate) {
    for (int i = 0, n = partitions_->partition_size(); i < n; ++i) {
      const CachedResult& partition = partitions_->partition(i);
      OutputResourcePtr output_resource;
      if (partition.optimizable() &&
          CreateOutputResourceForCachedOutput(&partition, &output_resource)) {
        outputs_.push_back(output_resource);
      } else {
        outputs_.push_back(OutputResourcePtr(NULL));
      }
    }
  }

  // If the cache gave a miss, or yielded unparsable data, then acquire a lock
  // and start fetching the input resources.
  if (cache_ok) {
    OutputCacheHit(false /* no need to write back to cache*/);
  } else {
    MarkSlow();
    if (can_revalidate) {
      OutputCacheRevalidate(revalidate);
    } else {
      OutputCacheMiss();
    }
  }
}

void RewriteContext::OutputCacheHit(bool write_partitions) {
  for (int i = 0, n = partitions_->partition_size(); i < n; ++i) {
    if (outputs_[i].get() != NULL) {
      Freshen(partitions_->partition(i));
      RenderPartitionOnDetach(i);
    }
  }

  ok_to_write_output_partitions_ = write_partitions;
  Finalize();
}

void RewriteContext::OutputCacheMiss() {
  outputs_.clear();
  partitions_->Clear();
  if (Manager()->TryLockForCreation(Lock())) {
    FetchInputs();
  } else {
    // TODO(jmarantz): bump stat for abandoned rewrites due to lock contention.
    ok_to_write_output_partitions_ = false;
    Activate();
  }
}

void RewriteContext::OutputCacheRevalidate(
    const InputInfoStarVector& to_revalidate) {
  DCHECK(!to_revalidate.empty());
  outstanding_fetches_ = to_revalidate.size();

  for (int i = 0, n = to_revalidate.size(); i < n; ++i) {
    InputInfo* input_info = to_revalidate[i];
    ResourcePtr resource = slots_[input_info->index()]->resource();
    Manager()->ReadAsync(
        Resource::kReportFailureIfNotCacheable,
        new ResourceRevalidateCallback(this, resource, input_info));
  }
}

void RewriteContext::RepeatedSuccess(const RewriteContext* primary) {
  CHECK(outputs_.empty());
  CHECK_EQ(num_slots(), primary->num_slots());
  CHECK_EQ(primary->outputs_.size(),
           static_cast<size_t>(primary->num_output_partitions()));
  // Copy over partition tables, outputs, and render_slot_ (as well as
  // was_optimized) information --- everything we can set in normal
  // OutputCacheDone.
  partitions_->CopyFrom(*primary->partitions_.get());
  for (int i = 0, n = primary->outputs_.size(); i < n; ++i) {
    outputs_.push_back(primary->outputs_[i]);
    if ((outputs_[i].get() != NULL) && !outputs_[i]->loaded()) {
      // We cannot safely alias resources that are not loaded, as the loading
      // process is threaded, and would therefore race. Therefore, recreate
      // another copy matching the cache data.
      CreateOutputResourceForCachedOutput(
          &partitions_->partition(i), &outputs_[i]);
    }
  }

  for (int i = 0, n = primary->num_slots(); i < n; ++i) {
    slot(i)->set_was_optimized(primary->slot(i)->was_optimized());
    render_slots_[i] = primary->render_slots_[i];
  }

  ok_to_write_output_partitions_ = false;
  Finalize();
}

void RewriteContext::RepeatedFailure() {
  CHECK(outputs_.empty());
  CHECK_EQ(0, num_output_partitions());
  rewrite_done_ = true;
  ok_to_write_output_partitions_ = false;
  FinalizeRewriteForHtml();
}

NamedLock* RewriteContext::Lock() {
  NamedLock* result = lock_.get();
  if (result == NULL) {
    // NOTE: This lock is based on hashes so if you use a MockHasher, you may
    // only rewrite a single resource at a time (e.g. no rewriting resources
    // inside resources, see css_image_rewriter_test.cc for examples.)
    //
    // TODO(jmarantz): In the multi-resource rewriters that can generate more
    // than one partition, we create a lock based on the entire set of input
    // URLs, plus a lock for each individual output.  However, in
    // single-resource rewriters, we really only need one of these locks.  So
    // figure out which one we'll go with and use that.
    GoogleString lock_name = StrCat(kRewriteContextLockPrefix, partition_key_);
    result = Manager()->MakeCreationLock(lock_name);
    lock_.reset(result);
  }
  return result;
}

void RewriteContext::FetchInputs() {
  ++num_predecessors_;

  for (int i = 0, n = slots_.size(); i < n; ++i) {
    const ResourceSlotPtr& slot = slots_[i];
    ResourcePtr resource(slot->resource());
    if (!(resource->loaded() && resource->ContentsValid())) {
      ++outstanding_fetches_;

      // Sometimes we can end up needing pagespeed resources as inputs.
      // This can happen because we are doing a fetch of something produced
      // by chained rewrites, or when handling a 2nd (or further) step of a
      // chain during an HTML rewrite if we don't have the bits inside the
      // resource object (e.g. if we got a metadata hit on the previous step).
      bool handled_internally = false;
      GoogleUrl resource_gurl(resource->url());
      if (Manager()->IsPagespeedResource(resource_gurl)) {
        RewriteDriver* nested_driver = Driver()->Clone();
        RewriteFilter* filter = NULL;
        // We grab the filter now (and not just call DecodeOutputResource
        // earlier instead of IsPagespeedResource) so we get a filter that's
        // bound to the new RewriteDriver.
        OutputResourcePtr output_resource =
            nested_driver->DecodeOutputResource(resource_gurl, &filter);
        if (output_resource.get() != NULL) {
          handled_internally = true;
          slot->SetResource(ResourcePtr(output_resource));
          ResourceReconstructCallback* callback =
              new ResourceReconstructCallback(
                  nested_driver, this, output_resource, i);
          nested_driver->FetchOutputResource(output_resource, filter, callback);
        } else {
          Manager()->ReleaseRewriteDriver(nested_driver);
        }
      }

      if (!handled_internally) {
        Resource::NotCacheablePolicy noncache_policy =
            Resource::kReportFailureIfNotCacheable;
        if (fetch_.get() != NULL) {
          // This is a fetch.  We want to try to get the input resource even if
          // it was previously noted to be uncacheable.
          noncache_policy = Resource::kLoadEvenIfNotCacheable;
        }
        Manager()->ReadAsync(noncache_policy,
                             new ResourceFetchCallback(this, resource, i));
      }
    }
  }

  --num_predecessors_;
  Activate();  // TODO(jmarantz): remove.
}

void RewriteContext::ResourceFetchDone(
    bool success, ResourcePtr resource, int slot_index) {
  CHECK_LT(0, outstanding_fetches_);
  --outstanding_fetches_;

  if (success) {
    ResourceSlotPtr slot(slots_[slot_index]);

    // For now, we cannot handle if someone updated our slot before us.
    DCHECK(slot.get() != NULL);
    DCHECK_EQ(resource.get(), slot->resource().get());
  }
  Activate();
}

void RewriteContext::ResourceRevalidateDone(InputInfo* input_info,
                                            bool success) {
  bool ok = false;
  if (success) {
    ResourcePtr resource = slots_[input_info->index()]->resource();
    if (resource->IsValidAndCacheable()) {
      // The reason we check IsValidAndCacheable here is in case someone
      // added a Vary: header without changing the file itself.
      ok = (resource->ContentsHash() == input_info->input_content_hash());

      // Patch up the input_info with the latest cache information on resource.
      resource->FillInPartitionInputInfo(
          Resource::kIncludeInputHash, input_info);
    }
  }

  revalidate_ok_ = revalidate_ok_ && ok;
  --outstanding_fetches_;
  if (outstanding_fetches_ == 0) {
    if (revalidate_ok_) {
      OutputCacheHit(true /* update the cache with new timestamps*/);
    } else {
      OutputCacheMiss();
    }
  }
}

bool RewriteContext::ReadyToRewrite() const {
  DCHECK(!rewrite_done_);
  bool ready = ((outstanding_fetches_ == 0) && (num_predecessors_ == 0));
  return ready;
}

void RewriteContext::Activate() {
  if (ReadyToRewrite()) {
    if (fetch_.get() == NULL) {
      DCHECK(started_);
      StartRewriteForHtml();
    } else {
      StartRewriteForFetch();
    }
  }
}

void RewriteContext::StartRewriteForHtml() {
  CHECK(has_parent() || slow_) << "slow_ not set on a rewriting job?";
  PartitionAsync(partitions_.get(), &outputs_);
}

void RewriteContext::PartitionDone(bool result) {
  if (!result) {
    partitions_->clear_partition();
    outputs_.clear();
  }

  outstanding_rewrites_ = partitions_->partition_size();
  if (outstanding_rewrites_ == 0) {
    DCHECK(fetch_.get() == NULL);
    // The partitioning succeeded, but yielded zero rewrites.  Write out the
    // empty partition table and let any successor Rewrites run.
    rewrite_done_ = true;

    // TODO(morlovich): The filters really should be doing this themselves,
    // since there may be partial failures in cases of multiple inputs which
    // we do not see here.
    AddRecheckDependency();
    FinalizeRewriteForHtml();
  } else {
    // We will let the Rewrites complete prior to writing the
    // OutputPartitions, which contain not just the partition table
    // but the content-hashes for the rewritten content.  So we must
    // rewrite before calling WritePartition.

    // Note that we run the actual rewrites in the "low priority" thread except
    // if we're serving an attached fetch, since we do not want to fail it due
    // to load shedding. Of course, we're only inside this method for a fetch
    // if it's a nested rewrite for one, since its top-level will be
    // handled by StartRewriteForFetch().
    bool is_fetch = ((parent_ != NULL) && (parent_->fetch_.get() != NULL));
    bool is_detached_fetch = is_fetch && parent_->fetch_->detached();

    CHECK_EQ(outstanding_rewrites_, static_cast<int>(outputs_.size()));
    for (int i = 0, n = outstanding_rewrites_; i < n; ++i) {
      InvokeRewriteFunction* invoke_rewrite =
          new InvokeRewriteFunction(this, i);
      if (is_fetch && !is_detached_fetch) {
        Driver()->AddRewriteTask(invoke_rewrite);
      } else {
        Driver()->AddLowPriorityRewriteTask(invoke_rewrite);
      }
    }
  }
}

void RewriteContext::WritePartition() {
  ResourceManager* manager = Manager();
  if (ok_to_write_output_partitions_ &&
      !manager->metadata_cache_readonly()) {
    CacheInterface* metadata_cache = manager->metadata_cache();
    SharedString buf;
    {
#ifndef NDEBUG
      for (int i = 0, n = partitions_->partition_size(); i < n; ++i) {
        const CachedResult& partition = partitions_->partition(i);
        if (partition.optimizable() && !partition.has_inlined_data()) {
          GoogleUrl gurl(partition.url());
          DCHECK(gurl.is_valid()) << partition.url();
        }
      }
#endif

      StringOutputStream sstream(buf.get());
      partitions_->SerializeToZeroCopyStream(&sstream);
      // destructor of sstream prepares *buf.get()
    }
    metadata_cache->Put(partition_key_, &buf);
  } else {
    // TODO(jmarantz): if our rewrite failed due to lock contention or
    // being too busy, then cancel all successors.
  }
  lock_.reset();
}

void RewriteContext::FinalizeRewriteForHtml() {
  DCHECK(fetch_.get() == NULL);

  bool partition_ok = (partitions_->partition_size() != 0);
  // Tells each of the repeated rewrites of the same thing if we have a valid
  // result or not.
  for (int c = 0, n = repeated_.size(); c < n; ++c) {
    if (partition_ok) {
      repeated_[c]->RepeatedSuccess(this);
    } else {
      repeated_[c]->RepeatedFailure();
    }
  }
  Driver()->DeregisterForPartitionKey(partition_key_, this);
  WritePartition();

  if (parent_ != NULL) {
    DCHECK(driver_ == NULL);
    Propagate(true);
    parent_->NestedRewriteDone(this);
  } else {
    // The RewriteDriver is waiting for this to complete.  Defer to the
    // RewriteDriver to schedule the Rendering of this context on the main
    // thread.
    CHECK(driver_ != NULL);
    driver_->RewriteComplete(this);
  }
}

void RewriteContext::AddNestedContext(RewriteContext* context) {
  ++num_pending_nested_;
  nested_.push_back(context);
  context->parent_ = this;
}

void RewriteContext::StartNestedTasks() {
  // StartNestedTasks() can be called from the filter, potentially from
  // a low-priority thread, but we want to run Start() in high-priority
  // thread as some of the work it does needs to be serialized with respect
  // to other tasks in that thread.
  Driver()->AddRewriteTask(
      MakeFunction(this, &RewriteContext::StartNestedTasksImpl));
}

void RewriteContext::StartNestedTasksImpl() {
  for (int i = 0, n = nested_.size(); i < n; ++i) {
    if (!nested_[i]->chained()) {
      nested_[i]->Start();
      DCHECK_EQ(n, static_cast<int>(nested_.size()))
          << "Cannot add new nested tasks once the nested tasks have started";
    }
  }
}

void RewriteContext::NestedRewriteDone(const RewriteContext* context) {
  // Record any external dependencies we have.
  // TODO(morlovich): Eliminate duplicates?
  for (int p = 0; p < context->num_output_partitions(); ++p) {
    const CachedResult* nested_result = context->output_partition(p);
    for (int i = 0; i < nested_result->input_size(); ++i) {
      InputInfo* dep = partitions_->add_other_dependency();
      dep->CopyFrom(nested_result->input(i));
      // The input index here is with respect to the nested context's inputs,
      // so would not be interpretable at top-level, and we don't use it for
      // other_dependency entries anyway, so be both defensive and frugal
      // and don't write it out.
      dep->clear_index();
    }
  }

  for (int p = 0; p < context->partitions_->other_dependency_size(); ++p) {
    InputInfo* dep = partitions_->add_other_dependency();
    dep->CopyFrom(context->partitions_->other_dependency(p));
  }

  if (context->was_too_busy_) {
    MarkTooBusy();
  }

  DCHECK_LT(0, num_pending_nested_);
  --num_pending_nested_;
  if (num_pending_nested_ == 0) {
    DCHECK(!rewrite_done_);
    Harvest();
  }
}

void RewriteContext::RewriteDone(
    RewriteSingleResourceFilter::RewriteResult result,
    int partition_index) {
  // RewriteDone may be called from a low-priority rewrites thread.
  // Make sure the rest of the work happens in the high priority rewrite thread.
  Driver()->AddRewriteTask(
      MakeFunction(this, &RewriteContext::RewriteDoneImpl,
                   result, partition_index));
}

void RewriteContext::RewriteDoneImpl(
    RewriteSingleResourceFilter::RewriteResult result,
    int partition_index) {
  if (result == RewriteSingleResourceFilter::kTooBusy) {
    MarkTooBusy();
  } else {
    CachedResult* partition =
        partitions_->mutable_partition(partition_index);
    bool optimizable = (result == RewriteSingleResourceFilter::kRewriteOk);
    partition->set_optimizable(optimizable);
    if (optimizable && (fetch_.get() == NULL)) {
      // TODO(morlovich): currently in async mode, we tie rendering of slot
      // to the optimizable bit, making it impossible to do per-slot mutation
      // that doesn't involve the output URL.
      RenderPartitionOnDetach(partition_index);
    }
  }
  --outstanding_rewrites_;
  if (outstanding_rewrites_ == 0) {
    if (fetch_.get() != NULL) {
      fetch_->set_success((result == RewriteSingleResourceFilter::kRewriteOk));
    }
    Finalize();
  }
}

void RewriteContext::Harvest() {
}

void RewriteContext::Render() {
}

void RewriteContext::Propagate(bool render_slots) {
  DCHECK(rewrite_done_ && (num_pending_nested_ == 0));
  if (rewrite_done_ && (num_pending_nested_ == 0)) {
    if (render_slots) {
      Render();
    }
    CHECK_EQ(num_output_partitions(), static_cast<int>(outputs_.size()));
    for (int p = 0, np = num_output_partitions(); p < np; ++p) {
      CachedResult* partition = output_partition(p);
      for (int i = 0, n = partition->input_size(); i < n; ++i) {
        int slot_index = partition->input(i).index();
        if (render_slots_[slot_index]) {
          ResourcePtr resource(outputs_[p]);
          slots_[slot_index]->SetResource(resource);
          if (render_slots) {
            slots_[slot_index]->Render();
          }
        }
      }
    }
  }

  if (successors_.empty()) {
    for (int i = 0, n = slots_.size(); i < n; ++i) {
      slots_[i]->Finished();
    }
  }

  RunSuccessors();
}

void RewriteContext::Finalize() {
  rewrite_done_ = true;
  DCHECK_EQ(0, num_pending_nested_);
  if (fetch_.get() != NULL) {
    fetch_->FetchDone();
  } else {
    FinalizeRewriteForHtml();
  }
}

void RewriteContext::RenderPartitionOnDetach(int rewrite_index) {
  CachedResult* partition = output_partition(rewrite_index);
  for (int i = 0; i < partition->input_size(); ++i) {
    int slot_index = partition->input(i).index();
    slot(slot_index)->set_was_optimized(true);
    render_slots_[slot_index] = true;
  }
}

void RewriteContext::RunSuccessors() {
  for (int i = 0, n = slots_.size(); i < n; ++i) {
    slot(i)->DetachContext(this);
  }

  for (int i = 0, n = successors_.size(); i < n; ++i) {
    RewriteContext* successor = successors_[i];
    if (--successor->num_predecessors_ == 0) {
      successor->Initiate();
    }
  }
  successors_.clear();
  if (driver_ != NULL) {
    DCHECK(rewrite_done_ && (num_pending_nested_ == 0));
    Driver()->AddRewriteTask(
        new MemberFunction1<RewriteDriver, RewriteContext*>(
            &RewriteDriver::DeleteRewriteContext, driver_, this));
  }
}

void RewriteContext::StartRewriteForFetch() {
  // Make a fake partition that has all the inputs, since we are
  // performing the rewrite for only one output resource.
  CachedResult* partition = partitions_->add_partition();
  bool ok_to_rewrite = true;
  for (int i = 0, n = slots_.size(); i < n; ++i) {
    ResourcePtr resource(slot(i)->resource());
    if (resource->loaded() && resource->ContentsValid()) {
      Resource::HashHint hash_hint =
          (kind() == kOnTheFlyResource) ?
              Resource::kOmitInputHash : Resource::kIncludeInputHash;
      resource->AddInputInfoToPartition(hash_hint, i, partition);
    } else {
      ok_to_rewrite = false;
      break;
    }
  }
  OutputResourcePtr output(fetch_->output_resource());

  // During normal rewrite path, Partition() is responsible for syncing up
  // the output resource's CachedResult and the partition tables. As it does
  // not get run for fetches, we take care of the syncing here.
  output->set_cached_result(partition);
  ++outstanding_rewrites_;
  if (ok_to_rewrite) {
    // We do not use a deadline for combining filters since we can't
    // just substitute in an input as a fallback, we have to wait for
    // them to actually make the combination.
    if (num_slots() == 1) {
      fetch_->SetupDeadlineAlarm();
    }

    Rewrite(0, partition, output);
  } else {
    partition->clear_input();
    AddRecheckDependency();
    RewriteDone(RewriteSingleResourceFilter::kRewriteFailed, 0);
  }
}

void RewriteContext::MarkSlow() {
  if (has_parent()) {
    return;
  }

  ContextSet to_detach;
  CollectDependentTopLevel(&to_detach);

  int num_new_slow = 0;
  for (ContextSet::iterator i = to_detach.begin();
        i != to_detach.end(); ++i) {
    RewriteContext* c = *i;
    if (!c->slow_) {
      c->slow_ = true;
      ++num_new_slow;
    }
  }

  if (num_new_slow != 0) {
    Driver()->ReportSlowRewrites(num_new_slow);
  }
}

void RewriteContext::MarkTooBusy() {
  ok_to_write_output_partitions_ = false;
  was_too_busy_ = true;
}

void RewriteContext::CollectDependentTopLevel(ContextSet* contexts) {
  std::pair<ContextSet::iterator, bool> insert_result = contexts->insert(this);
  if (!insert_result.second) {
    // We were already there.
    return;
  }

  for (int c = 0, n = successors_.size(); c < n; ++c) {
    if (!successors_[c]->has_parent()) {
      successors_[c]->CollectDependentTopLevel(contexts);
    }
  }

  for (int c = 0, n = repeated_.size(); c < n; ++c) {
    if (!repeated_[c]->has_parent()) {
      repeated_[c]->CollectDependentTopLevel(contexts);
    }
  }
}

bool RewriteContext::CreateOutputResourceForCachedOutput(
    const CachedResult* cached_result,
    OutputResourcePtr* output_resource) {
  bool ret = false;
  GoogleUrl gurl(cached_result->url());
  const ContentType* content_type =
      NameExtensionToContentType(StrCat(".", cached_result->extension()));

  ResourceNamer namer;
  if (gurl.is_valid() && namer.Decode(gurl.LeafWithQuery())) {
    output_resource->reset(
        new OutputResource(Manager(),
                           gurl.AllExceptLeaf() /* resolved_base */,
                           gurl.AllExceptLeaf() /* unmapped_base */,
                           Driver()->base_url().Origin() /* original_base */,
                           namer, content_type, Options(), kind()));
    ret = true;
  }
  return ret;
}

bool RewriteContext::Partition(OutputPartitions* partitions,
                               OutputResourceVector* outputs) {
  LOG(FATAL) << "RewriteContext subclasses must reimplement one of "
                "PartitionAsync or Partition";
  return false;
}

void RewriteContext::PartitionAsync(OutputPartitions* partitions,
                                    OutputResourceVector* outputs) {
  PartitionDone(Partition(partitions, outputs));
}

void RewriteContext::CrossThreadPartitionDone(bool result) {
  Driver()->AddRewriteTask(
      MakeFunction(this, &RewriteContext::PartitionDone, result));
}

void RewriteContext::Freshen(const CachedResult& partition) {
  // TODO(morlovich): This isn't quite enough as this doesn't cause us to
  // update the expiration in the partition tables; it merely makes it
  // essentially prefetch things in the cache for the future, which might
  // help the rewrite get in by the deadline.
  for (int i = 0, m = partition.input_size(); i < m; ++i) {
    const InputInfo& input_info = partition.input(i);
    if ((input_info.type() == InputInfo::CACHED) &&
        input_info.has_expiration_time_ms() &&
        input_info.has_date_ms() &&
        input_info.has_index()) {
      if (Manager()->IsImminentlyExpiring(input_info.date_ms(),
                                          input_info.expiration_time_ms())) {
        ResourcePtr resource(slots_[input_info.index()]->resource());
        resource->Freshen(Manager()->message_handler());
      }
    }
  }
}

const UrlSegmentEncoder* RewriteContext::encoder() const {
  return &default_encoder_;
}

GoogleString RewriteContext::CacheKeySuffix() const {
  return "";
}

bool RewriteContext::DecodeFetchUrls(
    const OutputResourcePtr& output_resource,
    MessageHandler* message_handler,
    GoogleUrlStarVector* url_vector) {
  GoogleUrl original_base(output_resource->url());
  GoogleUrl decoded_base(output_resource->decoded_base());
  StringPiece original_base_sans_leaf(original_base.AllExceptLeaf());
  bool check_for_multiple_rewrites =
      (original_base_sans_leaf != decoded_base.AllExceptLeaf());
  StringVector urls;
  if (encoder()->Decode(output_resource->name(), &urls, resource_context_.get(),
                        message_handler)) {
    if (check_for_multiple_rewrites) {
      // We want to drop the leaf from the base URL before combining it
      // with the decoded name, in case the decoded name turns into a
      // query. (Since otherwise we would end up with http://base/,qfoo?foo
      // rather than http://base?foo).
      original_base.Reset(original_base_sans_leaf);
    }

    for (int i = 0, n = urls.size(); i < n; ++i) {
      // If the decoded name is still encoded (because originally it was
      // rewritten by multiple filters, such as CSS minified then combined),
      // keep the un-decoded base, otherwise use the decoded base.
      // For example, this encoded URL:
      //   http://cdn.com/my.com/I.a.css.pagespeed.cf.0.css
      // needs will be decoded to http://my.com/a.css so we need to use the
      // decoded domain here. But this encoded URL:
      //   http://cdn.com/my.com/I.a.css+b.css,Mcc.0.css.pagespeed.cf.0.css
      // needs will be decoded first to:
      //   http://cdn.com/my.com/I.a.css+b.css,pagespeed.cc.0.css
      // which will then be decoded to http://my.com/a.css and b.css so for the
      // first decoding here we need to retain the encoded domain name.
      GoogleUrl* url = NULL;
      ResourceNamer namer;

      if (check_for_multiple_rewrites) {
        scoped_ptr<GoogleUrl> orig_based_url(
            new GoogleUrl(original_base, urls[i]));
        if (Manager()->IsPagespeedResource(*orig_based_url.get())) {
          url = orig_based_url.release();
        }
      }

      if (url == NULL) {  // Didn't set one based on original_base
        url = new GoogleUrl(decoded_base, urls[i]);
      }
      url_vector->push_back(url);
    }
    return true;
  }
  return false;
}

bool RewriteContext::Fetch(
    const OutputResourcePtr& output_resource,
    AsyncFetch* fetch,
    MessageHandler* message_handler) {
  // Decode the URLs required to execute the rewrite.
  bool ret = false;
  RewriteDriver* driver = Driver();
  driver->InitiateFetch(this);
  GoogleUrlStarVector url_vector;
  if (DecodeFetchUrls(output_resource, message_handler, &url_vector)) {
    bool is_valid = true;
    for (int i = 0, n = url_vector.size(); i < n; ++i) {
      GoogleUrl* url = url_vector[i];
      if (!url->is_valid()) {
        is_valid = false;
        break;
      }
      ResourcePtr resource(driver->CreateInputResource(*url));
      if (resource.get() == NULL) {
        // TODO(jmarantz): bump invalid-input-resource count
         is_valid = false;
         break;
      }
      ResourceSlotPtr slot(new FetchResourceSlot(resource));
      AddSlot(slot);
    }
    STLDeleteContainerPointers(url_vector.begin(), url_vector.end());
    if (is_valid) {
      SetPartitionKey();
      fetch_.reset(
          new FetchContext(this, fetch, output_resource, message_handler));
      if (output_resource->has_hash()) {
        fetch_->set_requested_hash(output_resource->hash());
      }
      Driver()->AddRewriteTask(MakeFunction(this, &RewriteContext::StartFetch));
      ret = true;
    }
  }
  if (!ret) {
    fetch->response_headers()->SetStatusAndReason(HttpStatus::kNotFound);
  }

  return ret;
}

void RewriteContext::FetchCacheDone(
    CacheInterface::KeyState state, SharedString value) {
  // If we have metadata during a resource fetch, we see if we can use it
  // to find a pre-existing result in HTTP cache we can serve. This is done
  // by sanity-checking the metadata here, then doing an async cache lookup via
  // FetchTryFallback, which in turn calls FetchFallbackCacheDone.
  // If we're successful at that point FetchContext::FetchFallbackDone
  // serves out the bits with a shortened TTL; if we fail at any point
  // we call StartFetchReconstruction which will invoke the normal process of
  // locking things, fetching inputs, rewriting, and so on.

  // Note that we don't try to revalidate inputs on fetch reconstruct,
  // so these two are ignored.
  bool can_revalidate = false;
  InputInfoStarVector revalidate;
  if (TryDecodeCacheResult(state, value, &can_revalidate, &revalidate) &&
      (num_output_partitions() == 1)) {
    CachedResult* result = output_partition(0);
    OutputResourcePtr output_resource;
    if (result->optimizable() &&
        CreateOutputResourceForCachedOutput(result, &output_resource)) {
      if (fetch_->requested_hash() != output_resource->hash()) {
        // Try to do a cache look up on the proper hash; if it's available,
        // we can serve it.
        FetchTryFallback(output_resource->url(), output_resource->hash());
        return;
      }
    } else if (num_slots() == 1) {
      // The result is not optimizable, and there is only one input.
      // Try serving the original. (For simplicity, we will do an another
      // rewrite attempt if it's not in the cache).
      FetchTryFallback(slot(0)->resource()->url(), "");
      return;
    }
  }

  // Didn't figure out anything clever; so just rewrite on demand.
  StartFetchReconstruction();
}

void RewriteContext::FetchTryFallback(const GoogleString& url,
                                      const StringPiece& hash) {
  Manager()->http_cache()->Find(
      url,
      Manager()->message_handler(),
      new HTTPCacheCallback(
          this, &RewriteContext::FetchFallbackCacheDone));
}

void RewriteContext::FetchFallbackCacheDone(HTTPCache::FindResult result,
                                            HTTPCache::Callback* data) {
  scoped_ptr<HTTPCache::Callback> cleanup_callback(data);

  StringPiece contents;
  if ((result == HTTPCache::kFound) &&
      data->http_value()->ExtractContents(&contents) &&
      (data->response_headers()->status_code() == HttpStatus::kOK)) {
    // We want to serve the found result, with short cache lifetime.
    fetch_->FetchFallbackDone(contents, data->response_headers());
  } else {
    StartFetchReconstruction();
  }
}

void RewriteContext::FetchCallbackDone(bool success) {
  RewriteDriver* notify_driver =
      notify_driver_on_fetch_done_ ? Driver() : NULL;
  async_fetch()->Done(success);
  if (notify_driver != NULL) {
    notify_driver->FetchComplete();
  }
}

void RewriteContext::StartFetch() {
  // If we have an on-the-fly resource, we almost always want to reconstruct it
  // --- there will be no shortcuts in the metadata cache unless the rewrite
  // fails, and it's ultra-cheap to reconstruct anyway.
  if (kind() == kOnTheFlyResource) {
    StartFetchReconstruction();
  } else {
    // Try to lookup metadata, as it may mark the result as non-optimizable
    // or point us to the right hash.
    Manager()->metadata_cache()->Get(
        partition_key_,
        new OutputCacheCallback(this, &RewriteContext::FetchCacheDone));
  }
}

void RewriteContext::StartFetchReconstruction() {
  // Note that in case of fetches we continue even if we didn't manage to
  // take the lock.
  partitions_->Clear();
  Manager()->LockForCreation(
      Lock(), Driver()->rewrite_worker(),
      MakeFunction(this, &RewriteContext::FetchInputs,
                   &RewriteContext::FetchInputs));
}

void RewriteContext::DetachFetch() {
  CHECK(fetch_.get() != NULL);
  fetch_->set_detached(true);
  Driver()->DetachFetch();
}

RewriteDriver* RewriteContext::Driver() const {
  const RewriteContext* rc;
  for (rc = this; rc->driver_ == NULL; rc = rc->parent_) {
    CHECK(rc != NULL);
  }
  return rc->driver_;
}

ResourceManager* RewriteContext::Manager() const {
  return Driver()->resource_manager();
}

const RewriteOptions* RewriteContext::Options() {
  return Driver()->options();
}

void RewriteContext::FixFetchFallbackHeaders(ResponseHeaders* headers) {
  if (headers->Sanitize()) {
    headers->ComputeCaching();
  }

  // Shorten cache length, and prevent proxies caching this, as it's under
  // the "wrong" URL.
  headers->SetDateAndCaching(
      headers->date_ms(),
      std::min(headers->cache_ttl_ms(), ResponseHeaders::kImplicitCacheTtlMs),
      ",private");
  headers->ComputeCaching();
}

bool RewriteContext::AbsolutifyIfNeeded(const StringPiece& input_contents,
                                        Writer* writer,
                                        MessageHandler* handler) {
  return writer->Write(input_contents, handler);
}

AsyncFetch* RewriteContext::async_fetch() {
  DCHECK(fetch_.get() != NULL);
  return fetch_->async_fetch();
}

MessageHandler* RewriteContext::fetch_message_handler() {
  DCHECK(fetch_.get() != NULL);
  return fetch_->handler();
}

}  // namespace net_instaweb
