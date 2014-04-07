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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_CONTEXT_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_CONTEXT_H_

#include <set>
#include <vector>

#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_segment_encoder.h"

namespace net_instaweb {

class AsyncFetch;
class GoogleUrl;
class MessageHandler;
class NamedLock;
class RequestTrace;
class ResponseHeaders;
class RewriteDriver;
class RewriteOptions;
class Statistics;
class Variable;
class Writer;
class FreshenMetadataUpdateManager;

// RewriteContext manages asynchronous rewriting of some n >= 1 resources (think
// CSS, JS, or images) into m >= 0 improved versions (typically, n = m = 1).
// It also helps update the references in the containing document (called
// slots), such as <img src=> in HTML, or background-image: url() in CSS,
// and make any other changes to it needed to commit the optimization.
//
// It is normally used as a base class, with its own code helping take care
// of caching, fetching, etc., while subclasses describe how to transform the
// resources, and how to update the document containing them with the new
// version by overriding some virtuals like Rewrite() and Render().
//
// Filters parsing HTML create their RewriteContext subclasses for every
// group of resources they think should be optimized together (such as one
// RewriteContext for every image for image re-compression, or one for a group
// of CSS files that have compatible HTML markup for CSS combining). The
// framework may also ask a filter to make its RewriteContext subclass via
// MakeRewriteContext() in case it need to reconstruct an optimized resource
// that's not available in the cache.
//
// In the case of combining filters, a single RewriteContext may
// result in multiple rewritten resources that are partitioned based
// on data semantics.  Most filters will just work on one resource,
// and those can inherit from SingleRewriteContext which is simpler
// to implement.
//
// The most basic transformation steps subclasses will want to implement are:
//
// Partition:
//   Determines how many outputs, if any, will be created from all the inputs.
//   For example, a spriter may create separate partitions for groups of images
//   with similar colormaps. This step is also responsible for deciding what to
//   do if some inputs were not loaded successfully. SingleRewriteContext
//   provides the correct implementation for transformations that take in one
//   file and optimize it.
//
// Rewrite:
//   Takes inputs from one partition, and tries to produce an optimized output
//   for it, as well as a CachedResult, which caches any auxiliary information
//   that may be needed to update the container document. For example, the image
//   filter will store image dimensions inside the CachedResult object.
//
//   If a better version can be created, the subclass should call
//   RewriteDriver::Write with its data, and then RewriteDone(kRewriteOk).
//
//   If no improvement is possible, it should call RewriteDone(kRewriteFailed).
//   Note that this does not mean that nothing can be done, just that no new
//   resource has been created (for example an image filter might still insert
//   dimensions into the <img> tag even if it can't compress the image better).
//
// Render:
//   Updates the document based on information stored in CachedResult.
//   This is the only step that can touch the HTML DOM. Note that you
//   do not need to implement it if you just want to update the URL to the new
//   version: the ResourceSlot's will do it automatically.
//
// Which of the steps get invoked depends on how much information has been
// cached, as well as on timing of things (since the system tries not to
// hold up the web page noticeably to wait for an optimization). Common
// scenarios are:
//
// 1) New rewrite, finishes quickly:
//    Partition -> Rewrite -> Render
// 2) New rewrite, but too slow to render:
//    Partition -> Rewrite
// 3) Metadata cache hit:
//    Render
// 4) Reconstructing output from a .pagespeed. URL:
//    Rewrite
//
// Note in particular that (3) means that all rendering should be doable just
// from information inside the CachedResult.
//
// Top-level RewriteContexts are initialized from the HTML thread, by filters
// responding to parser events.  In particular, from this thread they can be
// constructed, and AddSlot() and Initiate() can be called.  Once Initiate is
// called, the RewriteContext runs purely in its two threads, until
// it completes.  At that time it will self-delete in coordination with
// RewriteDriver.
//
// RewriteContexts can also be nested, in which case they are constructed,
// slotted, and Initated all within the rewrite threads.  However, they
// are Propagated and destructed by their parent, which was initiated by the
// RewriteDriver.
//
// RewriteContext utilizes two threads (via QueuedWorkerPool::Sequence)
// to do most of its work. The "high priority" thread is used to run the
// dataflow graph: queue up fetches and cache requests, partition inputs,
// render results, etc. The actual Rewrite() methods, however, are invoked
// in the "low priority" thread and can be canceled during extreme load
// or shutdown.
//
// TODO(jmarantz): add support for controlling TTL on failures.
class RewriteContext {
 public:
  typedef std::vector<InputInfo*> InputInfoStarVector;
  static const char kNumRewritesAbandonedForLockContention[];
  static const char kNumDeadlineAlarmInvocations[];
  static const char kNumDistributedRewriteSuccesses[];
  static const char kNumDistributedRewriteFailures[];
  static const char kNumDistributedMetadataFailures[];
  // The extension used for all distributed fetch URLs.
  static const char kDistributedExt[];
  // The hash value used for all distributed fetch URLs.
  static const char kDistributedHash[];
  // Used to pass the result of the metadata cache lookups. Recipient must
  // take ownership.
  struct CacheLookupResult {
    CacheLookupResult()
        : cache_ok(false),
          can_revalidate(false),
          useable_cache_content(false),
          is_stale_rewrite(false),
          partitions(new OutputPartitions) {}

    bool cache_ok;
    bool can_revalidate;
    bool useable_cache_content;
    bool is_stale_rewrite;
    InputInfoStarVector revalidate;
    scoped_ptr<OutputPartitions> partitions;
  };

  // Used for LookupMetadataForOutputResource.
  class CacheLookupResultCallback {
   public:
    CacheLookupResultCallback() {}
    virtual ~CacheLookupResultCallback();
    virtual void Done(const GoogleString& cache_key,
                      CacheLookupResult* result) = 0;
   private:
    DISALLOW_COPY_AND_ASSIGN(CacheLookupResultCallback);
  };

  // Takes ownership of resource_context, which must be NULL or
  // allocated with 'new'.
  RewriteContext(RewriteDriver* driver,   // exactly one of driver & parent
                 RewriteContext* parent,  // is non-null
                 ResourceContext* resource_context);
  virtual ~RewriteContext();

  // Random access to slots.  This is not thread-safe.  Prior to
  // Initialize(), these can be called by the constructing thread.
  // After Initiate(), these should only be called by the Rewrite
  // thread.
  int num_slots() const { return slots_.size(); }
  ResourceSlotPtr slot(int index) const { return slots_[index]; }

  // Random access to outputs.  These should only be accessed by
  // the RewriteThread.
  int num_outputs() const { return outputs_.size(); }
  OutputResourcePtr output(int i) const { return outputs_[i]; }

  // These are generally accessed in the Rewrite thread,
  // but may also be accessed in ::Render.
  int num_output_partitions() const;
  const CachedResult* output_partition(int i) const;
  CachedResult* output_partition(int i);

  // Returns true if this context is chained to some predecessors, and
  // must therefore be started by a predecessor and not RewriteDriver.
  bool chained() const { return chained_; }

  // Resource slots must be added to a Rewrite before Initiate() can
  // be called.  Starting the rewrite sets in motion a sequence
  // of async cache-lookups &/or fetches.
  void AddSlot(const ResourceSlotPtr& slot);

  // Remove the last slot from the context's slot list. This
  // context must be the last one attached to the slot.
  void RemoveLastSlot();

  // Adds a new nested RewriteContext.  This RewriteContext will not
  // be considered complete until all nested contexts have completed.
  // This may be useful, for example for a CSS optimizer that also wants to
  // optimize images referred to from CSS (in which case the image rewrite
  // context will be nested inside the CSS context).
  void AddNestedContext(RewriteContext* context);

  // Starts a resource rewrite.  Once Inititated, the Rewrite object
  // should only be accessed from the Rewrite thread, until it
  // Completes, at which point top-level Contexts will call
  // RewriteComplete on their driver, and nested Contexts will call
  // NestedRewriteComplete on their parent.  Nested rewrites will be
  // Started directly from their parent context, and Initiate will not
  // be called.
  //
  // Precondition: this rewrite isn't anyone's successor (e.g. chain() == false)
  //               and has not been started before.
  void Initiate();

  // Fetch the specified output resource by reconstructing it from
  // its inputs, sending output into fetch.
  //
  // True is returned if an asynchronous fetch got queued up.
  // If false, fetch->Done() will not be called.
  bool Fetch(const OutputResourcePtr& output_resource,
             AsyncFetch* fetch,
             MessageHandler* message_handler);

  // If true, we have determined that this job can't be rendered just
  // from metadata cache (including all prerequisites).
  bool slow() const { return slow_; }

  // This particular rewrite was a metadata cache miss.
  bool is_metadata_cache_miss() const { return is_metadata_cache_miss_; }

  // Returns true if this is a nested rewriter.
  bool has_parent() const { return parent_ != NULL; }

  // Returns true if this is a child rewriter and its parent has the given
  // id.
  bool IsNestedIn(StringPiece id) const;

  // Allows a nested rewriter to walk up its parent hierarchy.
  RewriteContext* parent() { return parent_; }
  const RewriteContext* parent() const { return parent_; }

  // If called with true, forces a rewrite and re-generates the output.
  void set_force_rewrite(bool x) { force_rewrite_ = x; }

  bool rewrite_uncacheable() const { return rewrite_uncacheable_; }
  void set_rewrite_uncacheable(bool rewrite_uncacheable) {
    rewrite_uncacheable_ = rewrite_uncacheable;
  }

  const ResourceContext* resource_context() const {
    return resource_context_.get();
  }

  // Returns debug information about this RewriteContext.
  GoogleString ToString(StringPiece prefix) const;

  // Initializes statistics.
  static void InitStats(Statistics* stats);

 protected:
  typedef std::vector<GoogleUrl*> GoogleUrlStarVector;

  // -----------------------------------------------------------------------
  // Resource transformation APIs. If you are implementing an optimization,
  // you'll be dealing mainly with these.
  // -----------------------------------------------------------------------

  // Finds the ServerContext associated with this context.  Note that
  // this method might have to climb up the parent-tree, but it's typically
  // not a deep tree.  Same with Driver() and Options().
  ServerContext* FindServerContext() const;
  const RewriteOptions* Options() const;
  RewriteDriver* Driver() const {
    return driver_;
  }

  // Accessors for the nested rewrites.
  int num_nested() const { return nested_.size(); }
  RewriteContext* nested(int i) const { return nested_[i]; }

  OutputPartitions* partitions() { return partitions_.get(); }

  // Add a dummy other_dependency that will force the rewrite's OutputPartitions
  // to be rechecked after a modest TTL.
  void AddRecheckDependency();

  // If this returns true, running the rewriter isn't required for
  // correctness of the page, so the engine will be permitted to drop
  // the rewrite if needed to preserve system responsiveness.
  virtual bool OptimizationOnly() const { return true; }

  // Partitions the input resources into one or more outputs.  Return
  // 'true' if the partitioning could complete (whether a rewrite was
  // found or not), false if the attempt was abandoned and no
  // conclusion can be drawn.
  //
  // Note that if partitioner finds that the resources are not
  // rewritable, it will still return true; it will simply have
  // an empty inputs-array in OutputPartitions and leave
  // 'outputs' unmodified.  'false' is only returned if the subclass
  // skipped the rewrite attempt due to a lock conflict.
  //
  // You must override one of Partition() or PartitionAsync(). Partition()
  // is normally fine unless you need to do computations that can take a
  // noticeable amount of time, since there are some scenarios under which
  // page output may end up being held up for a partitioning step. If you
  // do need to do something computationally expensive in partitioning steps,
  // override PartitionAsync() instead.
  virtual bool Partition(OutputPartitions* partitions,
                         OutputResourceVector* outputs);

  // As above, but you report the result asynchronously by calling
  // PartitionDone(), which must be done from the main rewrite
  // sequence. One of Partition or PartitionAsync() must be overridden in
  // the subclass. The default implementation is implemented in terms of
  // Partition().
  virtual void PartitionAsync(OutputPartitions* partitions,
                              OutputResourceVector* outputs);

  // Call this from the main rewrite sequence to report results of
  // PartitionAsync. If the client is not in the main rewrite sequence,
  // use CrossThreadPartitionDone() instead.
  void PartitionDone(RewriteResult result);

  // Helper for queuing invocation of PartitionDone to run in the
  // main rewrite sequence.
  void CrossThreadPartitionDone(RewriteResult result);

  // Takes a completed rewrite partition and rewrites it.  When
  // complete, implementations should call RewriteDone(kRewriteOk) if
  // they successfully created an output resource using RewriteDriver::Write,
  // and RewriteDone(kRewriteFailed) if they didn't. They may also call
  // RewriteDone(kTooBusy) in case system load/resource usage makes it
  // dangerous for the filter to do optimization at this time.
  //
  // Any information about the inputs or output that may be needed to update
  // the containing document should be stored inside the CachedResult.
  //
  // If implementors wish to rewrite resources referred to from within the
  // inputs (e.g. images in CSS), they may create nested rewrite contexts
  // and call AddNestedContext() on each, and then StartNestedTasks()
  // when all have been added.
  //
  // TODO(jmarantz): check for resource completion from a different
  // thread (while we were waiting for resource fetches) when Rewrite
  // gets called.
  virtual void Rewrite(int partition_index,
                       CachedResult* partition,
                       const OutputResourcePtr& output) = 0;

  // Called by subclasses when an individual rewrite partition is
  // done.  Note that RewriteDone may 'delete this' so no
  // further references to 'this' should follow a call to RewriteDone.
  // This method can run in any thread.
  void RewriteDone(RewriteResult result, int partition_index);

  // Absolutify contents of an input resource and write it into writer.
  // This is called in case a rewrite fails in the fetch path or a deadline
  // is exceeded. Default implementation is just to write the input.
  // But contexts may need to specialize this to actually absolutify
  // subresources if the fetched resource is served on a different path
  // than the input resource.
  virtual bool AbsolutifyIfNeeded(const StringPiece& output_url_base,
                                  const StringPiece& input_contents,
                                  Writer* writer, MessageHandler* handler);

  // Called on the parent to initiate all nested tasks.  This is so
  // that they can all be added before any of them are started.
  // May be called from any thread.
  void StartNestedTasks();

  // Once any nested rewrites have completed, the results of these
  // can be incorporated into the rewritten data.  For contexts that
  // do not require any nested RewriteContexts, it is OK to skip
  // overriding this method -- the empty default implementation is fine.
  virtual void Harvest();

  // Performs rendering activities that span multiple HTML slots.  For
  // example, in a filter that combines N slots to 1, N-1 of the HTML
  // elements might need to be removed.  That can be performed in
  // Render().  This method is optional; the base-class implementation
  // is empty.
  //
  // Note that unlike Harvest(), this method runs in the HTML thread (for
  // top-level rewrites), and only runs if the rewrite completes prior to
  // the rewrite-deadline.  If the rewrite does make it by the deadline,
  // RewriteContext::Render() will be invoked regardless of whether any slots
  // were actually optimized successfully.
  virtual void Render();

  // Notifies the subclass that the filter will not be able to render its
  // output to the containing HTML document, because it wasn't ready in time.
  // Note that neither Render() nor WillNotRender() may be called in case
  // this rewrite got canceled due to disable_further_processing(), or in case
  // Partition() failed. This is called from the HTML thread, but should only be
  // used for read access, and subclasss implementations are required to be
  // reasonably quick since it's called with rewrite_mutex() held. It's called
  // after any earlier contexts in filter order had completed their rendering,
  // if any, but with no order guarantees with respect to other WillNotRender()
  // invocations.
  virtual void WillNotRender();

  // This method is invoked (in Rewrite thread) if this context got canceled
  // due to an earlier filter sharing a slot with it having called
  // set_disable_further_processing. Default implementation does nothing.
  virtual void Cancel();

  // This final set of protected methods can be optionally overridden
  // by subclasses.

  // All RewriteContexts define how they encode URLs and other
  // associated information needed for a rewrite into a URL.
  // The default implementation handles a single URL with
  // no extra data.  The RewriteContext owns the encoder.
  //
  // TODO(jmarantz): remove the encoder from RewriteFilter.
  virtual const UrlSegmentEncoder* encoder() const;

  // Allows subclasses to add additional text to be appended to the
  // metadata cache key.  The default implementation returns "".
  virtual GoogleString CacheKeySuffix() const;

  // Indicates user agent capabilities that must be stored in the cache key.
  //
  // Note that the context may be NULL as it may not be set before this. Since
  // it isn't going to be modified in the method, ResourceContext is passed
  // as a const pointer.
  // TODO(morlovich): This seems to overlap with CacheKeySuffix.
  virtual GoogleString UserAgentCacheKey(
      const ResourceContext* context) const {
    return "";
  }

  // Encodes User Agent into the ResourceContext.
  // A subclass ResourceContext should normally call
  // RewriteFilter::EncodeUserAgentIntoResourceContext if it has access to
  // a RewriteFilter.
  virtual void EncodeUserAgentIntoResourceContext(ResourceContext* context) {}

  // Returns the filter ID.
  virtual const char* id() const = 0;

  // Rewrites come in three flavors, as described in output_resource_kind.h,
  // so this method must be defined by subclasses to indicate which it is.
  //
  // For example, we will avoid caching output_resource content in the HTTP
  // cache for rewrites that are so quick to complete that it's fine to
  // do the rewrite on every request.  extend_cache is obviously in
  // this category, and it's arguable we could treat js minification
  // that way too (though we don't at the moment).
  virtual OutputResourceKind kind() const = 0;

  // -----------------------------------------------------------------------
  // Tracing API.
  // -----------------------------------------------------------------------

  // Creates a new request trace associated with this context with a given
  // |label|.
  void AttachDependentRequestTrace(const StringPiece& label);

  // Provides the dependent request trace associated with this context, if any.
  // Note that this is distinct from the root user request trace, available
  // in Driver().
  RequestTrace* dependent_request_trace() { return dependent_request_trace_; }

  // A convenience wrapper to log a trace annotation in both the request
  // trace (if present) as well as the root user request trace (if present).
  void TracePrintf(const char* fmt, ...);

  // -----------------------------------------------------------------------
  // Fetch state machine override APIs, as well as exports of some general
  // state machine state for overriders to use. If you just want to write an
  // optimization, you do not need these --- they are useful if you want to
  // write a new state machine that's similar but not quite identical to
  // what RewriteContext provides.
  // -----------------------------------------------------------------------

  // Called in fetch path if we have not found the resource available
  // in HTTP cache under an alternate location suggested by metadata cache
  // such as a different hash or the original, and thus need to fully
  // reconstruct it.
  //
  // The base implementation will do an asynchronous locking attempt,
  // scheduling to run FetchInputs when complete. Subclasses may override
  // this method to preload inputs in a different manner, and may delay
  // calling of base version until that is complete.
  virtual void StartFetchReconstruction();

  // Determines if the given rewrite should be distributed. This is based on
  // whether distributed servers have been configured, if the current filter is
  // configured to be distributed, where a filter is in a chain, if a
  // distributed fetcher is in place, and if distribution has been explicitly
  // disabled for this context.
  bool ShouldDistributeRewrite() const;

  // Determines if this rewrite-context is acting on behalf of a distributed
  // rewrite request from an HTML rewrite. Verifies the distributed rewrite key.
  bool IsDistributedRewriteForHtml() const;

  // Dispatches the rewrite to another task with a distributed fetcher. Should
  // not be called without first getting true from ShoulDistributeRewrite() as
  // it has guards (such as checking the number of slots).
  void DistributeRewrite();

  // Makes the rest of a fetch run in background, not producing
  // a result or invoking callbacks. Will arrange for appropriate
  // memory management with the rewrite driver itself; but the caller
  // is responsible for delivering results itself and invoking the
  // callback.
  void DetachFetch();

  // Decodes the output resource to find the resources to be fetched. The
  // default behavior decodes the output resource name into multiple paths and
  // absolutifies them with respect to the output resource base. Returns true if
  // the decoding is successful and false otherwise.
  virtual bool DecodeFetchUrls(const OutputResourcePtr& output_resource,
                               MessageHandler* message_handler,
                               GoogleUrlStarVector* url_vector);

  // Adjust headers sent out for a stale or in-place result. We may send out
  // stale results in the fallback fetch pathway, but these results should not
  // be cached much.  By default we strip Set-Cookie* headers and Etags, and
  // convert Cache-Control headers to private, max-age=300.
  virtual void FixFetchFallbackHeaders(const CachedResult& cached_result,
                                       ResponseHeaders* headers);

  // Callback once the fetch is done. This calls Driver()->FetchComplete() if
  // notify_driver_on_fetch_done is true.
  virtual void FetchCallbackDone(bool success);

  // Attempts to fetch a given URL from HTTP cache, and serves it
  // (with shortened HTTP headers) if available. If not, fallback to normal
  // full reconstruction path. Note that the hash can be an empty string if the
  // url is not rewritten.
  virtual void FetchTryFallback(const GoogleString& url,
                                const StringPiece& hash);

  // Freshens resources proactively to avoid expiration in the near future.
  void Freshen();

  bool notify_driver_on_fetch_done() const {
    return notify_driver_on_fetch_done_;
  }
  void set_notify_driver_on_fetch_done(bool value) {
    notify_driver_on_fetch_done_ = value;
  }

  // Returns true if this context will prevent any attempt at distributing a
  // rewrite (although its nested context still may be distributed). See
  // ShouldDistributeRewrite for more detail on when a rewrite should be
  // distributed.
  bool block_distribute_rewrite() const { return block_distribute_rewrite_; }
  void set_block_distribute_rewrite(const bool x) {
    block_distribute_rewrite_ = x;
  }

  // Note that the following must only be called in the fetch flow.
  AsyncFetch* async_fetch();

  // Is fetch_ detached? Only call this in the fetch flow.
  bool FetchContextDetached();

  // The message handler for the fetch.
  MessageHandler* fetch_message_handler();

  // Indicates whether we are serving a stale rewrite.
  bool stale_rewrite() const { return stale_rewrite_; }

  // Returns an interval in milliseconds to wait when configuring the deadline
  // alarm in FetchContext::SetupDeadlineAlarm(). Subclasses may configure the
  // deadline based on rewrite type, e.g., IPRO vs. HTML-path.
  virtual int64 GetRewriteDeadlineAlarmMs() const;

  // Should the context call LockForCreation before checking the cache?
  virtual bool CreationLockBeforeStartFetch() { return true; }

  // Backend to RewriteDriver::LookupMetadataForOutputResource, with
  // the RewriteContext of appropriate type and the OutputResource already
  // created. Takes ownership of rewrite_context.
  static bool LookupMetadataForOutputResourceImpl(
      OutputResourcePtr output_resource,
      const GoogleUrl& gurl,
      RewriteContext* rewrite_context,
      RewriteDriver* driver,
      GoogleString* error_out,
      CacheLookupResultCallback* callback);

 private:
  class DistributedRewriteCallback;
  class DistributedRewriteFetch;
  class OutputCacheCallback;
  class LookupMetadataForOutputResourceCallback;
  class HTTPCacheCallback;
  class ResourceCallbackUtils;
  class ResourceFetchCallback;
  class ResourceReconstructCallback;
  class ResourceRevalidateCallback;
  class InvokeRewriteFunction;
  class RewriteFreshenCallback;
  friend class RewriteDriver;

  typedef std::set<RewriteContext*> ContextSet;

  // This is passed to CanFetchFallbackToOriginal when trying to determine
  // whether using the 0th input resource would be an acceptable substitute
  // for output when:
  enum FallbackCondition {
    kFallbackDiscretional,   // trying to produce result quicker to improve
                             // latency
    kFallbackEmergency    // rewrite failed and output would otherwise not
                          // be available
  };

  // Callback helper functions.
  void Start();
  void SetPartitionKey();
  void StartFetch();
  void StartFetchImpl();
  void CancelFetch();
  void OutputCacheDone(CacheLookupResult* cache_result);
  void OutputCacheHit(bool write_partitions);
  void OutputCacheRevalidate(const InputInfoStarVector& to_revalidate);
  void OutputCacheMiss();
  void ResourceFetchDone(bool success, ResourcePtr resource, int slot_index);
  void ResourceRevalidateDone(InputInfo* input_info, bool success);
  void LogMetadataCacheInfo(bool cache_ok, bool can_revalidate);

  // When a RewriteContext 'B' discovers that it's doing the exact same rewrite
  // as a previous RewriteContext 'A', B adds itself to A->repeated_, and
  // suspends its work, expecting 'A' to call B->RepeatedSuccess(A) or
  // B->RepeatedFailure() to give it the result of the rewrite.
  void RepeatedSuccess(const RewriteContext* primary);
  void RepeatedFailure();

  // After a Rewrite is complete, writes the metadata for the rewrite
  // operation to the cache, and runs any further rewites that are
  // dependent on this one.
  //
  // If there are pending nested rewrites then this call has no
  // effect.  Once all the nested rewrites have been accounted for via
  // NestedRewriteDone() then Finalize can queue up its render and
  // enable successor rewrites to proceed.
  void Finalize();

  // Get reference to lock_, lazy-initializing if necessary.
  NamedLock* Lock();

  // Initiates an asynchronous fetch for the resources associated with
  // each slot, calling ResourceFetchDone() when complete.
  //
  // To avoid concurrent fetches across multiple processes or threads, the
  // caller must first lock each input by name, blocking or abandoning rewriting
  // as necessary.  Input fetches done on behalf of resource fetches must
  // succeed to avoid sending 404s to clients, and so they will break locks.
  // Input fetches done for async rewrite initiations should fail fast to help
  // avoid having multiple concurrent processes attempt the same rewrite.
  void FetchInputs();

  // Callback to a distributed rewrite fetch. Queued to run in the high-priority
  // thread. Fetch path: If the fetch succeeded then the rest of the flow is
  // skipped and that result is used, otherwise the original resource is fetched
  // and returned, bypassing rewriting.
  void DistributeRewriteDone(bool success);

  // If the response_headers have metadata in them, strip the metadata from the
  // headers, parse them and write them to cache_result.  Returns true if
  // the parse was successful otherwise false.
  bool ParseAndRemoveMetadataFromResponseHeaders(
      ResponseHeaders* response_headers, CacheLookupResult* cache_result);

  // Create an OutputResource initialized from CachedResult, response headers,
  // and content.
  bool CreateOutputResourceFromContent(const CachedResult& cached_result,
                                       const ResponseHeaders& response_headers,
                                       StringPiece content,
                                       OutputResourcePtr* output_resource);

  // The distributed rewrite path for HTML rewrites works by converting the
  // input URL on the ingress task into a .pagespeed. fetch for the distributed
  // task to reconstruct using the corresponding filter id. This function maps
  // the given input resource URL into a .pagespeed. URL for reconstruction. It
  // uses a hash value of 0 and an extension of "distributed". Returns an empty
  // string if the URL could not be constructed (e.g., was too long).
  //
  // Ex. input: http://www.example.com/a.png with an image compression context
  //    output: http://www.example.com/50x50xa.png.pagespeed.ic.0.distributed
  GoogleString DistributedFetchUrl(StringPiece url);

  // Returns true if this rewrite context was created to fetch a resource (e.g.,
  // IPRO or .pagespeed. URLs) and false otherwise.
  bool IsFetchRewrite() const { return fetch_.get() != NULL; }

  // Called on the parent from a nested Rewrite when it is complete.
  // Note that we don't track rewrite success/failure here.  We only
  // care whether the nested rewrites are complete, and whether there
  // are any dependencies.
  void NestedRewriteDone(const RewriteContext* context);

  // Generally a RewriteContext is waiting for one or more
  // asynchronous events to take place.  Activate is called
  // to run some action to help us advance to the next state.
  void Activate();

  // Runs after all Rewrites have been completed, and all nested
  // RewriteContexts have completed and harvested.
  //
  // For top-level Rewrites, this must be called from the HTML thread.
  // For nested Rewrites it runs from the Rewrite thread.
  //
  // If render_slots is true, then all the slots owned by this context
  // will have Render() called on them.  For top-level Rewrites, this
  // should only be done if the rewrite completes before the rewrite
  // deadline expires.  After that, the HTML elements referred to by
  // the slots have already been flushed to the network.  For nested
  // Rewrites it's done unconditionally.
  //
  // Rewriting and propagation continue even after this deadline, so
  // that we may cache the rewritten results, allowing the deadline to
  // be easier-to-hit next time the same resources need to be
  // rewritten.
  //
  // And in all cases, the successors Rewrites are queued up in the
  // Rewrite thread once any nested propagation is complete.  And, in
  // particular, each slot must be updated with any rewritten
  // resources, before the successors can be run, independent of
  // whether the slots can be rendered into HTML.
  void Propagate(bool render_slots);

  // With all resources loaded, the rewrite can now be done, writing:
  //    The metadata into the cache
  //    The output resource into the cache
  //    if the driver has not been detached,
  //      the url+data->rewritten_resource is written into the rewrite
  //      driver's map, for each of the URLs.
  void StartRewriteForHtml();
  void StartRewriteForFetch();

  // Determines whether the Context is in a state where it's ready to
  // rewrite.  This requires:
  //    - no preceding RewriteContexts in progress
  //    - no outstanding cache lookups
  //    - no outstanding fetches
  //    - rewriting not already complete.
  bool ReadyToRewrite() const;

  // Removes this RewriteContext from all slots.  This is done normally when
  // a RewriteContext is completed and we are ready to run the successors.
  // It is also done when aborting a RewriteContext due to cache being
  // unhealthy.
  void DetachSlots();

  // Activate any Rewrites that come after this one, for serializability
  // of access to common slots.
  void RunSuccessors();

  // Writes out the partition-table into the metadata cache (checking
  // ok_to_write_output_partitions_)
  void WritePartition();

  // Does all the bookkeeping needed after rewrite in HTML completes ---
  // writes out cache data, notifies any repeated rewrites, queues up
  // successors, cleans things up, etc.
  //
  // This method may call 'delete this' so it should be the last call at its
  // call-site.
  //
  // It will *not* call 'delete this' if there is a live RewriteDriver,
  // waiting for a convenient point to render the rewrites into HTML.
  void FinalizeRewriteForHtml();

  // Arranges for commit of all the state (if permit_render is true), and
  // notification of parents, rewrite driver, etc., as well as running of
  // successors if applicable. This is the tail portion of
  // FinalizeRewriteForHtml that must be called even if we didn't
  // actually get as far as computing a partition_key_.
  void RetireRewriteForHtml(bool permit_render);

  // Marks this job and any dependents slow as appropriate, notifying the
  // RewriteDriver of any changes.
  void MarkSlow();

  // Notes that we dropped parts of this rewrite due to system load, so we
  // should not cache it.
  void MarkTooBusy();

  // Collect all non-nested contexts that depend on this one (including
  // itself). Note that this might exclude some repeated jobs that haven't
  // gotten far enough to realize that yet.
  void CollectDependentTopLevel(ContextSet* contexts);

  // Actual implementation of RewriteDone that's queued to run in
  // high-priority rewrite thread.
  void RewriteDoneImpl(RewriteResult result, int partition_index);

  // Actual implementation of StartNestedTasks that's queued to run in
  // high-priority rewrite thread.
  void StartNestedTasksImpl();

  // Establishes that a slot has been rewritten.  So when Propagate()
  // is called, the resource update that has been written to this slot can
  // be propagated to the DOM.
  void RenderPartitionOnDetach(int partition_index);

  // Sets up all the state needed for Fetch, but doesn't register this context
  // or actually start the rewrite process.
  bool PrepareFetch(
      const OutputResourcePtr& output_resource,
      AsyncFetch* fetch,
      MessageHandler* message_handler);

  // Creates an output resource that corresponds to a full URL stored in
  // metadata cache.
  bool CreateOutputResourceForCachedOutput(const CachedResult* cached_result,
                                           OutputResourcePtr* output_resource);

  // Callback for metadata lookup on fetch path.
  void FetchCacheDone(CacheLookupResult* cache_result);

  // Callback for HTTP lookup on fetch path where the metadata cache suggests
  // we should try either serving a different path or the original.
  void FetchFallbackCacheDone(HTTPCache::FindResult result,
                              HTTPCache::Callback* data);

  // Returns true if we can attempt to serve the original file for a fetch
  // request in case something goes wrong with rewriting (circumstance ==
  // kFallbackEmergency) or the system thinks that would avoid a latency
  // spike or overload (kFallbackDiscretional).
  bool CanFetchFallbackToOriginal(FallbackCondition circumstance) const;

  // Checks whether an other dependency input info already exists in the
  // partition with the same data. Used to de-dup the field.
  bool HasDuplicateOtherDependency(const InputInfo& input);

  // Check if there is a duplicate and if there is none, add to the other
  // dependencies. Updates the internal other_dependency map that is used to
  // de-dup the contents.
  void CheckAndAddOtherDependency(const InputInfo& input);

  // Perform checks and freshen the input resource. Also updates metadata if
  // required.
  void CheckAndFreshenResource(const InputInfo& input_info,
                               ResourcePtr resource, int partition_index,
                               int input_index,
                               FreshenMetadataUpdateManager* freshen_manager);
  ResourcePtr CreateUrlResource(const StringPiece& input_url);

  // To perform a rewrite, we need to have data for all of its input slots.
  ResourceSlotVector slots_;

  // Not all of the slots require rendering from this RewriteContext.  If an
  // optimization was deemed non-beneficial then we skip rendering the slot.
  // So keep the slots requiring rendering in a bitvector.
  std::vector<bool> render_slots_;

  // It's feasible that callbacks for different resources will be delivered
  // on different threads, thus we must protect these counters with a mutex
  // or make them using atomic integers.
  //
  // TODO(jmarantz): keep the outstanding fetches as a set so they can be
  // terminated cleanly and immediately, allowing fast process shutdown.
  // For example, if Apache notifies our process that it's being shut down
  // then we should have a mechanism to cancel all pending fetches.  This
  // would require a new cancellation interface from both CacheInterface and
  // UrlAsyncFetcher.

  bool started_;
  scoped_ptr<OutputPartitions> partitions_;
  OutputResourceVector outputs_;
  int outstanding_fetches_;
  int outstanding_rewrites_;
  scoped_ptr<ResourceContext> resource_context_;
  GoogleString partition_key_;

  UrlSegmentEncoder default_encoder_;

  // Lock guarding output partitioning and rewriting.  Lazily initialized by
  // Lock(), unlocked on destruction or the end of Finish().
  scoped_ptr<NamedLock> lock_;

  // When this rewrite object is created on behalf of a fetch, we must
  // keep the response_writer, request_headers, and callback in the
  // FetchContext so they can be used once the inputs are available.
  class FetchContext;
  scoped_ptr<FetchContext> fetch_;

  // Track the RewriteContexts that must be run after this one because they
  // share a slot.
  std::vector<RewriteContext*> successors_;

  // Other places on the page (or CSS) that should be rewritten the same
  // way 'this' is (e.g. because they refer to the same URL, filter and
  // settings).
  std::vector<RewriteContext*> repeated_;

  // Track the number of nested contexts that must be completed before
  // this one can be marked complete.  Nested contexts are typically
  // added during the Rewrite() phase.
  int num_pending_nested_;
  std::vector<RewriteContext*> nested_;

  // If this context is nested, the parent is the context that 'owns' it.
  RewriteContext* parent_;

  // If this context was initiated from a RewriteDriver, either due to
  // a Resource Fetch or an HTML Rewrite, then we keep track of the
  // RewriteDriver, and notify it when the RewriteContext is complete.
  // That way it can stay around and 'own' all the resources associated
  // with all the resources it spawns, directly or indirectly.
  //
  // Nested RewriteContexts obtain their driver from their parent, but
  // store it here to permit Driver() to be a simple getter.
  RewriteDriver* driver_;

  // Track the number of ResourceContexts that must be run before this one.
  int num_predecessors_;

  // If true, this context's execution must follow some other context's
  // completion (which may have occurred already).
  bool chained_;

  // TODO(jmarantz): Refactor to replace a bunch bool member variables with
  // an explicit state_ member variable, with a set of possibilties that
  // look something like this:
  //
  // enum State {
  //   kCluster,     // Inputs are being clustered into RewriteContexts.
  //   kLookup,      // Looking up partitions & rewritten URLs in the cache.
  //                 //   - If successsful, skip to Render.
  //   kFetch,       // Waiting for URL fetches to complete.
  //   kPartition,   // Fetches complete; ready to partition into
  //                 // OutputResources.
  //   kRewrite,     // Partitioning complete, ready to Rewrite.
  //   kHarvest,     // Nested RewriteContexts complete, ready to harvest
  //                 // results.
  //   kRender,      // Ready to render the rewrites into the DOM.
  //   kComplete     // Ready to delete.
  // };

  // True if all the rewriting is done for this context.
  bool rewrite_done_;

  // True if it's valid to write the partition table to the metadata cache.
  // We would *not* want to do that if one of the Rewrites completed
  // with status kTooBusy or if we've just read these very partitions from
  // the metadata cache.
  //
  // Because both failure (kTooBusy) and success (we just read this from cache)
  // lead to ok_to_write_output_partitions_ being turned off, this is not copied
  // from nested rewrite contexts.  In the success case we want the parent to
  // write iff it has made changes, which is what it will do if we copy nothing;
  // in the failure case we also set was_too_busy_, which does get copied to the
  // parent.
  bool ok_to_write_output_partitions_;

  // True if the rewrite was incomplete due to heavy load; if this is true
  // ok_to_write_output_partitions_ must be false.  This is copied from nested
  // rewrite contexts because if one rewrite fails none should be saved.
  bool was_too_busy_;

  // We mark a job as "slow" when we cannot render it entirely from the
  // metadata cache (including rendering its predecessors). We only do this
  // for top-level jobs.
  bool slow_;

  // Starts at true, set to false if any content-change checks failed.
  bool revalidate_ok_;

  // Indicates that the context should call driver()->FetchComplete() once the
  // fetch is done.
  bool notify_driver_on_fetch_done_;

  // Indicates whether we want to force a rewrite. If true, we skip reading
  // from the metadata cache.
  bool force_rewrite_;

  // Indicates that the current rewrite involves at least one resource which
  // is stale.
  bool stale_rewrite_;

  // Indicates whether we have a metadata miss (or an unsuccessful revalidation
  // attempt) on the html path.
  bool is_metadata_cache_miss_;

  // If set to true, we'll try to rewrite un-cacheable resources.
  // The flag is expected to be set to true only from IPRO context.
  bool rewrite_uncacheable_;

  // An optional request trace associated with this context. May be NULL.
  // Always owned externally.
  RequestTrace* dependent_request_trace_;

  // Set true if this rewrite context should be blocked from distributing its
  // rewrite.
  bool block_distribute_rewrite_;

  // Stores the resulting headers and content of a distributed rewrite.
  scoped_ptr<DistributedRewriteFetch> distributed_fetch_;

  // Map to dedup partitions other dependency field.
  StringIntMap other_dependency_map_;

  Variable* const num_rewrites_abandoned_for_lock_contention_;
  Variable* const num_distributed_rewrite_failures_;
  Variable* const num_distributed_rewrite_successes_;
  Variable* const num_distributed_metadata_failures_;
  DISALLOW_COPY_AND_ASSIGN(RewriteContext);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_CONTEXT_H_
