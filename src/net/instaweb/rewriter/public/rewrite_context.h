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

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/blocking_behavior.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/url_segment_encoder.h"

namespace net_instaweb {
class AbstractLock;
class CachedResult;
class OutputPartition;
class OutputPartitions;
class ResourceContext;
class ResourceManager;
class RewriteDriver;
class SharedString;
class Statistics;

// Class to retain state as we rewrite a collection of resources.  The
// rewriting flow is callback driven.  We use callbacks to wake up
// from async cache & URL fetches.  We do rewrites in response to
// those.
//
// The scope of a RewriteContext is up to the Filter that creates it.
// In single-resource-filters, there will typically be a distinct
// RewriteContext for each rewritten resource.  In combining filters,
// there will will typically be a single RewriteContext for multiple
// Resource inputs.  One way to look at a RewriteContext is a potentially
// multi-fanin multi-fanout node in a dependency graph.
//
// TODO(jmarantz): Support resource-fetch flow which (a) calls callback
// when rewrite is complete and (b) will break locks
// TODO(jmarantz): rigorously analyze system for thread safety inserting
// mutexes, etc.
class RewriteContext {
 public:
  // Transfers ownership of resource_context, which must be NULL or
  // allocated with 'new'.
  RewriteContext(RewriteDriver* driver,
                 ResourceContext* resource_context);
  virtual ~RewriteContext();

  // Static initializer for statistics variables.
  static void Initialize(Statistics* statistics);

  // If the rewrite_driver (really the request context) is detached
  // prior to the completion of the rewrite, the rewrite still
  // continues.  But we must detach it from the driver.  At this
  // point we also render the rewrite if it has been completed.
  void RenderAndDetach();

  // Random access to slots.
  int num_slots() const { return slots_.size(); }
  ResourceSlotPtr slot(int index) const { return slots_[index]; }

  // Resource slots must be added to a Rewrite before Start() can
  // be called.  Starting the rewrite sets in motion a sequence
  // of async cache-lookups &/or fetches.
  void AddSlot(const ResourceSlotPtr& slot);

  // Starts a resource rewrite.
  void Start();

  // Callback helper functions.  These are not intended to be called
  // by the client; but are public: to avoid 'friend' declarations
  // for the time being.
  void OutputCacheDone(CacheInterface::KeyState state, SharedString* value);
  void ResourceFetchDone(bool success, const ResourcePtr& resource,
                         int slot_index);

 protected:
  // The following methods are provided for the benefit of subclasses.

  const RewriteOptions* options() { return &options_; }
  ResourceManager* resource_manager() { return resource_manager_; }
  const ResourceContext* resource_context() { return resource_context_.get(); }

  // Establishes that a slot has been rewritten.  So when RenderAndDetach
  // is called, the resource update that has been written to this slot can
  // be propagated to the DOM.
  void RenderSlotOnDetach(const ResourceSlotPtr& slot) {
    render_slots_.push_back(slot);
  }


  // The next set of methods must be implemented by subclasses:

  // Takes a completed rewrite partition and performs the document mutations
  // needed to render the rewrite.
  //
  // A Resource object is provided that can be used to set into appropriate
  // slot(s).  Note that this is conceptutally an output resource but is
  // not guaranteed to be of type OutputResource; for rendering purposes
  // we primarily need a URL.
  //
  // It is the responsibility of RewriteContext, not its subclasses, to
  // verify the validity of the output resource, with respect to domain
  // legality, cache freshness, etc.
  //
  // TODO(jmarantz): verify domain lawyering, cache freshness, etc.
  virtual void Render(const OutputPartition& partition,
                      const ResourcePtr& output_resource) = 0;

  // Partitions the input resources into one or more outputs, writing
  // the end results into the http cache.  Return 'true' if the partitioning
  // could complete (whether a rewrite was found or not), false if the attempt
  // was abandoned and no conclusion can be drawn.
  virtual bool PartitionAndRewrite(OutputPartitions* partitions) = 0;


  // This final set of protected methods can be optionally overridden
  // by subclasses.

  // If this method returns true, the data output of this filter will not be
  // cached, and will instead be recomputed on the fly every time it is needed.
  // (However, the transformed URL and similar metadata in CachedResult will be
  //  kept in cache).
  //
  // The default implementation returns 'false'.
  //
  // A subclass will change this to return 'true' if the rewrite that it makes
  // is extremely quick, and so there is not much benefit to caching it as
  // an output.  CacheExtender is an obvious case, since it doesn't change the
  // bytes of the resource.
  virtual bool ComputeOnTheFly() const;

  // All RewriteContexts define how they encode URLs and other
  // associated information needed for a rewrite into a URL.
  // The default implementation handles a single URL with
  // no extra data.  The RewriteContext owns the encoder.
  //
  // TODO(jmarantz): remove the encoder from RewriteFilter.
  virtual const UrlSegmentEncoder* encoder() const;

  // Returrns the filter ID.
  virtual const char* id() const = 0;

 private:
  // With all resources loaded, the rewrite can now be done, writing:
  //    The metadata into the cache
  //    The output resource into the cache
  //    if the driver has not been detached,
  //      the url+data->rewritten_resource is written into the rewrite
  //      driver's map, for each of the URLs.
  void Finish();

  // Collects all rewritten results and queues them for rendering into
  // the DOM.
  //
  // TODO(jmarantz): This method should be made thread-safe so it can
  // be called from a worker thread once callbacks are done or rewrites
  // are complete.
  void RenderPartitions(const OutputPartitions& partitions);

  // Returns 'true' if the resources are not expired.  Freshens resources
  // proactively to avoid expiration in the near future.
  bool FreshenAndCheckExpiration(const CachedResult& group);

  // To perform a rewrite, we need to have data for all of its input slots.
  ResourceSlotVector slots_;

  // The slots that have been rewritten, and thus should be rendered
  // back into the DOM, are added back into this vector.
  ResourceSlotVector render_slots_;

  // A driver must be supplied to initiate a RewriteContext.  However, the
  // driver may not stay around until the rewrite is complete, so we also
  // keep track of the resource manager.  The driver_ field will be NULLed
  // on Detach, which might happen in a different thread from various
  // callbacks that wake up on the context.  Thus we must protect it with a
  // mutex.
  RewriteDriver* driver_;

  // The resource_manager_ is basically thread-safe.  and does not go
  // away until the process is shut down.  It's worth thinking about,
  // however, what happens as the process is shut down.  Likely we'll
  // have to track all outstanding rewrites in the resource manager as
  // well and dissociate.
  //
  // TODO(jmarantz): define 'thread-safe' usage above more preciesely.
  // TODO(jmarantz): define and test shut-down flow.
  ResourceManager* resource_manager_;

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

  // The rewrite_options_ are duplicated from the RewriteDriver, so that
  // rewrites can continue even if the deadline expires and the RewriteDriver
  // is released.
  RewriteOptions options_;

  bool started_;
  int outstanding_fetches_;
  scoped_ptr<ResourceContext> resource_context_;
  GoogleString partition_key_;

  UrlSegmentEncoder default_encoder_;

  // Lock guarding output partitioning and rewriting.  Lazily initialized by
  // LockForCreation, unlocked on destruction or the end of Finish().
  scoped_ptr<AbstractLock> lock_;
  BlockingBehavior block_;

  DISALLOW_COPY_AND_ASSIGN(RewriteContext);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_CONTEXT_H_
