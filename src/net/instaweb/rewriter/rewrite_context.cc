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

#include <cstdarg>
#include <cstddef>                     // for size_t
#include <algorithm>
#include <utility>                      // for pair
#include <vector>
#include <memory>
#include <map>                          // for map<>::const_iterator

#include "base/logging.h"
#include "net/instaweb/config/rewrite_options_manager.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/base64_util.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/util/public/dynamic_annotations.h"  // RunningOnValgrind
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/queued_alarm.h"
#include "net/instaweb/util/public/request_trace.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/url_segment_encoder.h"
#include "net/instaweb/util/public/writer.h"
#include "pagespeed/kernel/base/callback.h"

namespace net_instaweb {

class RewriteFilter;

namespace {

const char kRewriteContextLockPrefix[] = "rc:";
// There is no partition index for other dependency fields. Use a constant
// to denote that.
const int kOtherDependencyPartitionIndex = -1;

}  // namespace

// Manages freshening of all the inputs of the given context. If any of the
// input resources change, this deletes the corresponding metadata. Otherwise,
// we update the metadata and write it out.
class FreshenMetadataUpdateManager {
 public:
  // Takes ownership of mutex.
  FreshenMetadataUpdateManager(const GoogleString& partition_key,
                               CacheInterface* metadata_cache,
                               AbstractMutex* mutex)
      : partition_key_(partition_key),
        metadata_cache_(metadata_cache),
        mutex_(mutex),
        num_pending_freshens_(0),
        all_freshens_triggered_(false),
        should_delete_cache_key_(false) {}

  ~FreshenMetadataUpdateManager() {}

  void Done(bool lock_failure, bool resource_ok) {
    bool should_cleanup = false;
    {
      ScopedMutex lock(mutex_.get());
      --num_pending_freshens_;
      if (!lock_failure && !resource_ok) {
        should_delete_cache_key_ = true;
      }
      should_cleanup = ShouldCleanup();
    }
    if (should_cleanup) {
      Cleanup();
    }
  }

  void MarkAllFreshensTriggered() {
    bool should_cleanup = false;
    {
      ScopedMutex lock(mutex_.get());
      all_freshens_triggered_ = true;
      should_cleanup = ShouldCleanup();
    }
    if (should_cleanup) {
      Cleanup();
    }
  }

  void IncrementFreshens(const OutputPartitions& partitions) {
    ScopedMutex lock(mutex_.get());
    if (partitions_.get() == NULL) {
      // Copy OutputPartitions lazily.
      OutputPartitions* cloned_partitions = new OutputPartitions;
      cloned_partitions->CopyFrom(partitions);
      partitions_.reset(cloned_partitions);
    }
    num_pending_freshens_++;
  }

  InputInfo* GetInputInfo(int partition_index, int input_index) {
    if (partition_index == kOtherDependencyPartitionIndex) {
      // This is referring to the other dependency input info.
      return partitions_->mutable_other_dependency(input_index);
    }
    return partitions_->mutable_partition(partition_index)->
        mutable_input(input_index);
  }

 private:
  bool ShouldCleanup() {
    mutex_->DCheckLocked();
    return (num_pending_freshens_ == 0) && all_freshens_triggered_;
  }

  void Cleanup() {
    if (should_delete_cache_key_) {
      // One of the resources changed. Delete the metadata.
      metadata_cache_->Delete(partition_key_);
    } else if (partitions_.get() != NULL) {
      GoogleString buf;
      {
        StringOutputStream sstream(&buf);  // finalizes buf in destructor
        partitions_->SerializeToZeroCopyStream(&sstream);
      }
      // Write the updated partition info to the metadata cache.
      metadata_cache_->PutSwappingString(partition_key_, &buf);
    }
    delete this;
  }

  // This is copied lazily.
  scoped_ptr<OutputPartitions> partitions_;
  GoogleString partition_key_;
  CacheInterface* metadata_cache_;
  scoped_ptr<AbstractMutex> mutex_;
  int num_pending_freshens_;
  bool all_freshens_triggered_;
  bool should_delete_cache_key_;

  DISALLOW_COPY_AND_ASSIGN(FreshenMetadataUpdateManager);
};

// Two callback classes for completed caches & fetches.  These gaskets
// help RewriteContext, which knows about all the pending inputs,
// trigger the rewrite once the data is available.  There are two
// versions of the callback.

// Callback to wake up the RewriteContext when the partitioning is looked up
// in the cache.  This takes care of parsing and validation of cached results.
// The RewriteContext can then decide whether to queue the output-resource for a
// DOM update, or re-initiate the Rewrite, depending on the metadata returned.
// Note that the parsing and validation happens in the caching thread and in
// Apache this will block other cache lookups from starting.  Hence this should
// be as streamlined as possible.
class RewriteContext::OutputCacheCallback : public CacheInterface::Callback {
 public:
  typedef void (RewriteContext::*CacheResultHandlerFunction)(
      CacheLookupResult* cache_result);

  OutputCacheCallback(RewriteContext* rc, CacheResultHandlerFunction function)
      : rewrite_context_(rc), function_(function),
        cache_result_(new CacheLookupResult) {}

  virtual ~OutputCacheCallback() {}

  virtual void Done(CacheInterface::KeyState state) {
    // Check if the cache content being used is stale. If so, mark it as a
    // cache hit but set the stale_rewrite flag in the context.
    if (cache_result_->useable_cache_content &&
        cache_result_->is_stale_rewrite &&
        !cache_result_->cache_ok) {
      cache_result_->cache_ok = true;
      rewrite_context_->stale_rewrite_ = true;
    }
    RewriteDriver* rewrite_driver = rewrite_context_->Driver();
    rewrite_driver->AddRewriteTask(MakeFunction(
        rewrite_context_, function_, cache_result_.release()));
    delete this;
  }

 protected:
  virtual bool ValidateCandidate(const GoogleString& key,
                                 CacheInterface::KeyState state) {
    DCHECK(!cache_result_->cache_ok);
    // The following is used to hold the cache lookup information obtained from
    // the current cache's value.  Note that the cache_ok field of this is not
    // used as we update cache_result_->cache_ok directly.
    CacheLookupResult candidate_cache_result;
    bool local_cache_ok = TryDecodeCacheResult(
        state, *value(), &candidate_cache_result);

    // cache_ok determines whether or not a second level cache is looked up. If
    // this is a stale rewrite, ensure there is an additional look up in the
    // remote cache in case there is fresh content elsewhere.
    bool stale_rewrite = candidate_cache_result.is_stale_rewrite;
    cache_result_->cache_ok = local_cache_ok && !stale_rewrite;

    // If local_cache_ok is true, then can_revalidate is guaranteed to be true
    // for the candidate cache result.
    bool use_this_revalidate = (candidate_cache_result.can_revalidate &&
                                (!cache_result_->can_revalidate ||
                                 (candidate_cache_result.revalidate.size() <
                                  cache_result_->revalidate.size())));
    // For the first call to ValidateCandidate if
    // candidate_cache_result.can_revalidate is true, then use_this_revalidate
    // will also be true (since cache_result_->can_revalidate will be false from
    // CacheLookupResult construction).
    bool use_partitions = true;
    if (!local_cache_ok) {
      if (use_this_revalidate) {
        cache_result_->can_revalidate = true;
        cache_result_->revalidate.swap(candidate_cache_result.revalidate);
        // cache_result_->partitions should be set to
        // candidate_cache_result.partitions, so that the pointers in
        // cache_result_->revalidate are valid.
      } else {
        // If the current cache value is not ok and if an earlier cache value
        // has a better revalidate than the current then do not use the current
        // candidate partitions and revalidate.
        use_partitions = false;
      }
    }
    // At this point the following holds:
    // use_partitions is true iff cache_result_->cache_ok is true or revalidate
    // has been moved to cache_result_->revalidate or local_cache_ok and
    // stale_rewrite is true.
    if (use_partitions) {
      cache_result_->partitions.reset(
          candidate_cache_result.partitions.release());
      // Remember that the cache contents are useable if needed. Also remember
      // if we are using stale contents.
      cache_result_->useable_cache_content = true;
      cache_result_->is_stale_rewrite = stale_rewrite;
    }
    // We return cache_result_->cache_ok.  This means for the last call to
    // ValidateCandidate we might return false when we might actually end up
    // using the cached result via revalidate.
    return cache_result_->cache_ok;
  }

  CacheLookupResult* ReleaseLookupResult() {
    return cache_result_.release();
  }

 private:
  bool AreInputInfosEqual(const InputInfo& input_info,
                          const InputInfo& fsmdc_info,
                          int64 mtime_ms) {
    return (fsmdc_info.has_last_modified_time_ms() &&
            fsmdc_info.has_input_content_hash() &&
            fsmdc_info.last_modified_time_ms() == mtime_ms &&
            fsmdc_info.input_content_hash() == input_info.input_content_hash());
  }

  // Checks if the stat() data about the input_info's file matches that in the
  // filesystem metadata cache; it needs to be for the input to be "valid".
  bool IsFilesystemMetadataCacheCurrent(CacheInterface* fsmdc,
                                        const GoogleString& file_key,
                                        const InputInfo& input_info,
                                        int64 mtime_ms) {
    //   Get the filesystem metadata cache (FSMDC) entry for the filename.
    //   If we found an entry,
    //     Extract the FSMDC timestamp and contents hash.
    //     If the FSMDC timestamp == the file's current timestamp,
    //       (the FSMDC contents hash is valid/current/correct)
    //       If the FSMDC content hash == the metadata cache's content hash,
    //         The metadata cache's entry is valid so its input_info is valid.
    //       Else
    //         Return false as the metadata cache's entry is not valid as
    //         someone has changed it on us.
    //     Else
    //       Return false as our FSMDC entry is out of date so we can't
    //       tell if the metadata cache's input_info is valid.
    //   Else
    //     Return false as we can't tell if the metadata cache's input_info is
    //     valid.
    CacheInterface::SynchronousCallback callback;
    fsmdc->Get(file_key, &callback);
    DCHECK(callback.called());
    if (callback.state() == CacheInterface::kAvailable) {
      StringPiece val_str = callback.value()->Value();
      ArrayInputStream input(val_str.data(), val_str.size());
      InputInfo fsmdc_info;
      if (fsmdc_info.ParseFromZeroCopyStream(&input)) {
        // We have a filesystem metadata cache entry: if its timestamp equals
        // the file's, and its contents hash equals the metadata caches's, then
        // the input is valid.
        return AreInputInfosEqual(input_info, fsmdc_info, mtime_ms);
      }
    }
    return false;
  }

  // Update the filesystem metadata cache with the timestamp and contents hash
  // of the given input's file (which is read from disk to compute the hash).
  // Returns false if the file cannot be read.
  bool UpdateFilesystemMetadataCache(ServerContext* server_context,
                                     const GoogleString& file_key,
                                     const InputInfo& input_info,
                                     int64 mtime_ms,
                                     CacheInterface* fsmdc,
                                     InputInfo* fsmdc_info) {
    GoogleString contents;
    if (!server_context->file_system()->ReadFile(
            input_info.filename().c_str(), &contents,
            server_context->message_handler())) {
      return false;
    }
    GoogleString contents_hash =
        server_context->contents_hasher()->Hash(contents);
    fsmdc_info->set_type(InputInfo::FILE_BASED);
    DCHECK_LT(0, mtime_ms);
    fsmdc_info->set_last_modified_time_ms(mtime_ms);
    fsmdc_info->set_input_content_hash(contents_hash);
    GoogleString buf;
    {
      // MUST be in a block so that sstream is destructed to finalize buf.
      StringOutputStream sstream(&buf);
      fsmdc_info->SerializeToZeroCopyStream(&sstream);
    }
    fsmdc->PutSwappingString(file_key, &buf);
    return true;
  }

  // Checks whether the given input is still unchanged.
  bool IsInputValid(const InputInfo& input_info, int64 now_ms, bool* purged,
                    bool* stale_rewrite) {
    switch (input_info.type()) {
      case InputInfo::CACHED: {
        // It is invalid if cacheable inputs have expired or ...
        DCHECK(input_info.has_expiration_time_ms());
        const RewriteOptions* options = rewrite_context_->Options();
        if (input_info.has_url()) {
          if (options->IsUrlPurged(input_info.url(), input_info.date_ms())) {
            *purged = true;
            return false;
          }
        }
        if (!input_info.has_expiration_time_ms()) {
          return false;
        }
        int64 ttl_ms = input_info.expiration_time_ms() - now_ms;
        if (ttl_ms > 0) {
          return true;
        } else if (
            !rewrite_context_->has_parent() &&
            ttl_ms + options->metadata_cache_staleness_threshold_ms() > 0) {
          *stale_rewrite = true;
          return true;
        }
        return false;
      }
      case InputInfo::FILE_BASED: {
        ServerContext* server_context = rewrite_context_->FindServerContext();

        // ... if file-based inputs have changed.
        DCHECK(input_info.has_last_modified_time_ms() &&
               input_info.has_filename());
        if (!input_info.has_last_modified_time_ms() ||
            !input_info.has_filename()) {
          return false;
        }
        int64 mtime_sec;
        server_context->file_system()->Mtime(input_info.filename(), &mtime_sec,
                                             server_context->message_handler());
        int64 mtime_ms = mtime_sec * Timer::kSecondMs;

        CacheInterface* fsmdc = server_context->filesystem_metadata_cache();
        if (fsmdc != NULL) {
          CHECK(fsmdc->IsBlocking());
          if (!input_info.has_input_content_hash()) {
            return false;
          }
          // Construct a host-specific key. The format is somewhat arbitrary,
          // all it needs to do is differentiate the same path on different
          // hosts. If the size of the key becomes a concern we can hash it
          // and hope.
          GoogleString file_key;
          StrAppend(&file_key, "file://", server_context->hostname(),
                    input_info.filename());
          if (IsFilesystemMetadataCacheCurrent(fsmdc, file_key, input_info,
                                               mtime_ms)) {
            return true;
          }
          InputInfo fsmdc_info;
          if (!UpdateFilesystemMetadataCache(server_context, file_key,
                                             input_info, mtime_ms, fsmdc,
                                             &fsmdc_info)) {
            return false;
          }
          // Check again now that we KNOW we have the most up-to-date data
          // in the filesystem metadata cache.
          return AreInputInfosEqual(input_info, fsmdc_info, mtime_ms);
        } else {
          DCHECK_LT(0, input_info.last_modified_time_ms());
          return (mtime_ms == input_info.last_modified_time_ms());
        }
      }
      case InputInfo::ALWAYS_VALID:
        return true;
    }

    DLOG(FATAL) << "Corrupt InputInfo object !?";
    return false;
  }

  // Check that a CachedResult is valid, specifically, that all the inputs are
  // still valid/non-expired.  If return value is false, it will also check to
  // see if we should re-check validity of the CachedResult based on input
  // contents, and set *can_revalidate accordingly. If *can_revalidate is true,
  // *revalidate will contain info on resources to re-check, with the InputInfo
  // pointers being pointers into the partition.
  bool IsCachedResultValid(CachedResult* partition,
                           bool* can_revalidate, bool* is_stale_rewrite,
                           InputInfoStarVector* revalidate) {
    bool valid = true;
    *can_revalidate = true;
    int64 now_ms = rewrite_context_->FindServerContext()->timer()->NowMs();
    for (int j = 0, m = partition->input_size(); j < m; ++j) {
      const InputInfo& input_info = partition->input(j);
      bool purged = false;
      if (!IsInputValid(input_info, now_ms, &purged, is_stale_rewrite)) {
        valid = false;
        // We currently do not attempt to re-check file-based resources
        // based on contents; as mtime is a lot more reliable than
        // cache expiration, and permitting 'touch' to force recomputation
        // is potentially useful.
        if (input_info.has_input_content_hash() &&
            input_info.has_index() &&
            (input_info.type() == InputInfo::CACHED) &&
            !purged) {
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

  // Checks whether all the entries in the given partition tables' other
  // dependency table are valid.
  bool IsOtherDependencyValid(const OutputPartitions* partitions,
                              bool* is_stale_rewrite) {
    int64 now_ms = rewrite_context_->FindServerContext()->timer()->NowMs();
    for (int j = 0, m = partitions->other_dependency_size(); j < m; ++j) {
      bool purged;
      if (!IsInputValid(partitions->other_dependency(j), now_ms, &purged,
                        is_stale_rewrite)) {
        return false;
      }
    }
    return true;
  }

  // Tries to decode result of a cache lookup (which may or may not have
  // succeeded) into partitions (in result->partitions), and also checks the
  // dependency tables.
  //
  // Returns true if cache hit, and all dependencies checked out.
  //
  // May also return false, but set result->can_revalidate to true and output a
  // list of inputs (result->revalidate) to re-check if the situation may be
  // salvageable if inputs did not change.
  //
  // Will return false with result->can_revalidate = false if the cached result
  // is entirely unsalvageable.
  bool TryDecodeCacheResult(CacheInterface::KeyState state,
                            const SharedString& value,
                            CacheLookupResult* result) {
    bool* can_revalidate = &(result->can_revalidate);
    InputInfoStarVector* revalidate = &(result->revalidate);
    OutputPartitions* partitions = result->partitions.get();
    bool* is_stale_rewrite = &(result->is_stale_rewrite);
    if (state != CacheInterface::kAvailable) {
      rewrite_context_->FindServerContext()->rewrite_stats()->
          cached_output_misses()->Add(1);
      *can_revalidate = false;
      return false;
    }
    // We've got a hit on the output metadata; the contents should
    // be a protobuf.  Try to parse it.
    StringPiece val_str = value.Value();
    ArrayInputStream input(val_str.data(), val_str.size());
    if (partitions->ParseFromZeroCopyStream(&input) &&
        IsOtherDependencyValid(partitions, is_stale_rewrite)) {
      bool ok = true;
      *can_revalidate = true;
      for (int i = 0, n = partitions->partition_size(); i < n; ++i) {
        CachedResult* partition = partitions->mutable_partition(i);
        bool can_revalidate_resource;
        if (!IsCachedResultValid(partition, &can_revalidate_resource,
                                 is_stale_rewrite, revalidate)) {
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

  RewriteContext* rewrite_context_;
  CacheResultHandlerFunction function_;
  scoped_ptr<CacheLookupResult> cache_result_;
};

// Like OutputCacheCallback but forwarding info to an external user rather
// than to RewriteContext
class RewriteContext::LookupMetadataForOutputResourceCallback
    : public RewriteContext::OutputCacheCallback {
 public:
  // Unlike base class, this takes ownership of 'rc'.
  LookupMetadataForOutputResourceCallback(
      const GoogleString& key, RewriteContext* rc,
      CacheLookupResultCallback* callback)
      : OutputCacheCallback(rc, NULL),
        key_(key),
        rewrite_context_(rc),
        callback_(callback) {
  }

  virtual void Done(CacheInterface::KeyState state) {
    callback_->Done(key_, ReleaseLookupResult());
    delete this;
  }

 private:
  GoogleString key_;
  scoped_ptr<RewriteContext> rewrite_context_;
  CacheLookupResultCallback* callback_;
};

// Bridge class for routing cache callbacks to RewriteContext methods
// in rewrite thread. Note that the receiver will have to delete the callback
// (which we pass to provide access to data without copying it)
class RewriteContext::HTTPCacheCallback : public OptionsAwareHTTPCacheCallback {
 public:
  typedef void (RewriteContext::*HTTPCacheResultHandlerFunction)(
      HTTPCache::FindResult, HTTPCache::Callback* data);

  HTTPCacheCallback(RewriteContext* rc, HTTPCacheResultHandlerFunction function)
      : OptionsAwareHTTPCacheCallback(rc->Options(),
                                      rc->Driver()->request_context()),
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
        rewrite_context_(rc),
        delegate_(rc, r, slot_index) {
  }

  virtual ~ResourceFetchCallback() {}
  virtual void Done(bool lock_failure, bool resource_ok) {
    if (lock_failure) {
      rewrite_context_->ok_to_write_output_partitions_ = false;
    }
    delegate_.Done(!lock_failure && resource_ok);
    delete this;
  }

 private:
  RewriteContext* rewrite_context_;
  ResourceCallbackUtils delegate_;
};

// Callback used when we need to reconstruct a resource we made to satisfy
// a fetch (due to rewrites being nested inside each other).
class RewriteContext::ResourceReconstructCallback
    : public AsyncFetchUsingWriter {
 public:
  // Takes ownership of the driver (e.g. will call Cleanup)
  ResourceReconstructCallback(RewriteDriver* driver, RewriteContext* rc,
                              const OutputResourcePtr& resource, int slot_index)
      : AsyncFetchUsingWriter(driver->request_context(),
                              resource->BeginWrite(driver->message_handler())),
        driver_(driver),
        delegate_(rc, ResourcePtr(resource), slot_index),
        resource_(resource) {
    set_response_headers(resource->response_headers());
  }

  virtual ~ResourceReconstructCallback() {
  }

  virtual void HandleDone(bool success) {
    // Compute the final post-write state of the object, including the hash.
    // Also takes care of dropping creation lock.
    resource_->EndWrite(driver_->message_handler());

    // Make sure to compute the URL, as we'll be killing the rewrite driver
    // shortly, and the driver is needed for URL computation.
    resource_->url();

    delegate_.Done(success);
    driver_->Cleanup();
    delete this;
  }

  virtual void HandleHeadersComplete() {}

 private:
  RewriteDriver* driver_;
  ResourceCallbackUtils delegate_;
  OutputResourcePtr resource_;
  DISALLOW_COPY_AND_ASSIGN(ResourceReconstructCallback);
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

  virtual void Done(bool lock_failure, bool resource_ok) {
    RewriteDriver* rewrite_driver = rewrite_context_->Driver();
    rewrite_driver->AddRewriteTask(
        MakeFunction(rewrite_context_, &RewriteContext::ResourceRevalidateDone,
                     input_info_, !lock_failure && resource_ok));
    delete this;
  }

 private:
  RewriteContext* rewrite_context_;
  InputInfo* input_info_;
};

// Callback that is invoked after freshening a resource. This invokes the
// FreshenMetadataUpdateManager with the relevant updates.
class RewriteContext::RewriteFreshenCallback
    : public Resource::FreshenCallback {
 public:
  RewriteFreshenCallback(const ResourcePtr& resource,
                         int partition_index,
                         int input_index,
                         FreshenMetadataUpdateManager* manager)
      : FreshenCallback(resource),
        partition_index_(partition_index),
        input_index_(input_index),
        manager_(manager) {}

  virtual ~RewriteFreshenCallback() {}

  virtual InputInfo* input_info() {
    return manager_->GetInputInfo(partition_index_, input_index_);
  }

  virtual void Done(bool lock_failure, bool resource_ok) {
    manager_->Done(lock_failure, resource_ok);
    delete this;
  }

 private:
  int partition_index_;
  int input_index_;
  FreshenMetadataUpdateManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(RewriteFreshenCallback);
};

// This class helps to prepare a distributed fetch and calls
// DistributeRewriteDone once a dispatched fetch is complete.
class RewriteContext::DistributedRewriteFetch : public AsyncFetch {
 public:
  DistributedRewriteFetch(const RequestContextPtr& request_ctx,
                          StringPiece url,
                          const RequestHeaders* request_headers,
                          RewriteContext* rewrite_context,
                          UrlAsyncFetcher* fetcher, MessageHandler* handler)
      : AsyncFetch(request_ctx),
        url_(url.as_string()),
        rewrite_context_(rewrite_context),
        fetcher_(fetcher),
        message_handler_(handler) {
    // Copy the request headers instead of making clean ones as they might have
    // important information such as user-agent.
    RequestHeaders* new_req_headers = new RequestHeaders();
    new_req_headers->CopyFrom(*request_headers);
    SetRequestHeadersTakingOwnership(new_req_headers);
  }

  virtual ~DistributedRewriteFetch() {}

  void DispatchForHTML() {
    DCHECK(fetcher_ != NULL);
    StringPiece distributed_key =
        rewrite_context_->Options()->distributed_rewrite_key();
    request_headers()->Add(HttpAttributes::kXPsaDistributedRewriteForHtml,
                           distributed_key);
    request_headers()->Add(HttpAttributes::kXPsaRequestMetadata,
                           distributed_key);
    // Note: We're defaulting to a kGet request. We don't always *have* to do a
    // kGet here, but it's a good idea as some situations might require it (such
    // as chained rewriters so that the next filter has its input, and
    // in_place_wait_for_optimized needs the output as well for harvesting).
    RewriteOptionsManager* rewrite_options_manager =
        rewrite_context_->FindServerContext()->rewrite_options_manager();
    rewrite_options_manager->PrepareRequest(
        rewrite_context_->Options(), &url_, request_headers(),
        NewCallback(this, &DistributedRewriteFetch::StartFetch));
  }

  StringPiece contents() {
    StringPiece contents;
    http_value_.ExtractContents(&contents);
    return contents;
  }

 protected:
  virtual void HandleDone(bool success) {
    if (http_value_.Empty()) {
      // If there have been no writes so far, write an empty string to the
      // HTTPValue. Note that this is required since empty writes aren't
      // propagated while fetching and we need to write something to the
      // HTTPValue so that we can successfully extract empty content from it.
      http_value_.Write("", message_handler_);
    }
    RewriteDriver* rewrite_driver = rewrite_context_->Driver();
    rewrite_driver->AddRewriteTask(MakeFunction(
        rewrite_context_, &RewriteContext::DistributeRewriteDone, success));
  }
  virtual void HandleHeadersComplete() {}
  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    return http_value_.Write(content, handler);
  }
  virtual bool HandleFlush(MessageHandler* handler) { return true; }

 private:
  void StartFetch(bool success) {
    if (!success) {
      rewrite_context_->DistributeRewriteDone(false);
      return;
    }
    fetcher_->Fetch(url_, message_handler_, this);
  }

  GoogleString url_;
  RewriteContext* rewrite_context_;
  UrlAsyncFetcher* fetcher_;
  HTTPValue http_value_;
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(DistributedRewriteFetch);
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
        original_output_url_(output_resource->UrlEvenIfHashNotSet()),
        handler_(handler),
        deadline_alarm_(NULL),
        success_(false),
        detached_(false),
        skip_fetch_rewrite_(false),
        num_deadline_alarm_invocations_(
            rewrite_context_->Driver()->statistics()->GetVariable(
                kNumDeadlineAlarmInvocations)) {
  }

  static void InitStats(Statistics* stats) {
    stats->AddVariable(kNumDeadlineAlarmInvocations);
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
    StringPiece expected_key = driver->options()->distributed_rewrite_key();
    bool distributed_block = false;
    if (!expected_key.empty() &&
        driver->request_headers()->HasValue(
            HttpAttributes::kXPsaDistributedRewriteBlock, expected_key)) {
      distributed_block = true;
    }

    if (driver->is_nested() || distributed_block) {
      // If we're being used to help reconstruct a .pagespeed. resource during
      // chained optimizations within HTML, we do not want fetch-style deadlines
      // to be active, as if they trigger, the main rewrite that created us
      // would get a cache-control: private fallback as its input, causing it
      // to cache 'my input wasn't rewritable' metadata result. Further, the
      // HTML-targeted rewrite already has a way of dealing with slowness, by
      // detaching from rendering.

      // We also do not want nested rewrites to early-return in case of fetches
      // as it can affect correctness of JS combine, as the names of the
      // OutputResources, and hence the JS variables may turn out not be
      // what was expected.

      // If a distributed request came from a nested driver it will set
      // kXPsaDistributedRewriteBlock, and likewise we should not set the alarm.
      return;
    }

    Timer* timer = rewrite_context_->FindServerContext()->timer();

    // Negative rewrite deadline means unlimited.
    int deadline_ms = rewrite_context_->GetRewriteDeadlineAlarmMs();
    bool test_force_alarm =
        driver->options()->test_instant_fetch_rewrite_deadline();
    if (deadline_ms >= 0 || test_force_alarm) {
      if (test_force_alarm) {
        deadline_ms = 0;
      }
      // Startup an alarm which will cause us to return unrewritten content
      // rather than hold up the fetch too long on firing.
      deadline_alarm_ =
          new QueuedAlarm(
              driver->scheduler(), driver->rewrite_worker(),
              timer->NowUs() + (deadline_ms * Timer::kMsUs),
              MakeFunction(this, &FetchContext::HandleDeadline));
    }
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
    // It's very tempting to log the output URL here, but it's not safe to do
    // so, as OutputResource::UrlEvenIfHashNotSet can write to the hash,
    // which may race against normal setting of the hash in
    // RewriteDriver::Write called off low-priority thread.
    num_deadline_alarm_invocations_->Add(1);
    ResourcePtr input(rewrite_context_->slot(0)->resource());
    handler_->Message(
        kInfo, "Deadline exceeded for rewrite of resource %s with %s.",
        input->url().c_str(), rewrite_context_->id());
    FetchFallbackDoneImpl(input->contents(), input->response_headers());
  }

  // We need to be careful not to leak metadata.  So only add it when
  // it has been requested and we're configured to use distributed rewriting.
  bool ShouldAddMetadata() {
    RewriteDriver* driver = rewrite_context_->Driver();
    const RequestHeaders* request_headers = driver->request_headers();
    // TODO(jkarlin): DCHECK that distributed rewrite is set in the request
    // header.
    // TODO(jkarlin): For Apache we'll also need to verify the src address is
    // from a trusted host or trusted network. This will require a new directive
    // and src address information in the request_context, which is not there
    // today.
    const RewriteOptions* options = rewrite_context_->Options();
    if (!options->distributed_rewrite_servers().empty() &&
        request_headers != NULL &&
        driver->MetadataRequested(*request_headers)) {
      return true;
    }
    return false;
  }

  // If the request headers asked for metadata then put base64 encoded
  // metadata in the response headers.
  void AddMetadataHeaderIfNecessary(ResponseHeaders* response_headers) {
    DCHECK(!response_headers->Has(HttpAttributes::kXPsaResponseMetadata));
    if (ShouldAddMetadata()) {
      GoogleString encoded, serialized;
      if (rewrite_context_->partitions()->SerializeToString(&serialized)) {
        Mime64Encode(serialized, &encoded);
        response_headers->Add(HttpAttributes::kXPsaResponseMetadata,  encoded);
      }
    }
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
        AddMetadataHeaderIfNecessary(response_headers);
        StringPiece contents = output_resource_->contents();
        async_fetch_->set_content_length(contents.size());
        async_fetch_->HeadersComplete();
        ok = async_fetch_->Write(contents, handler_);
      } else {
        // Our rewrite produced a different hash than what was requested;
        // we better not give it an ultra-long TTL.
        FetchFallbackDone(output_resource_->contents(),
                          output_resource_->response_headers());

        return;
      }
    } else {
      // Rewrite failed. If we can, fallback to the original as rewrite failing
      // may just mean the input isn't optimizable.
      if (rewrite_context_->CanFetchFallbackToOriginal(kFallbackEmergency)) {
        ResourcePtr input_resource(rewrite_context_->slot(0)->resource());
        if (input_resource.get() != NULL && input_resource->HttpStatusOk()) {
          // TODO(jkarlin): When we have an X-Psa-Distributed-Rewrite-Html
          // header we should guard this message (and the one ~20 lines down)
          // with it so that we don't print messages for what are ultimately
          // html-derived rewrites.  We might also want to guard Rewrite-IPRO
          // as well.
          handler_->Message(kWarning, "Rewrite %s failed while fetching %s",
                            input_resource->url().c_str(),
                            output_resource_->UrlEvenIfHashNotSet().c_str());
          // TODO(sligocki): Log variable for number of failed rewrites in
          // fetch path.

          response_headers->CopyFrom(*input_resource->response_headers());
          const CachedResult* cached_result =
              rewrite_context_->output_partition(0);
          CHECK(cached_result != NULL);
          rewrite_context_->FixFetchFallbackHeaders(*cached_result,
                                                    response_headers);
          // Use the most conservative Cache-Control considering all inputs.
          // Note that this is needed because FixFetchFallbackHeaders might
          // actually relax things a bit if the input was no-cache.
          ApplyInputCacheControl(response_headers);
          StringPiece contents = input_resource->contents();
          async_fetch_->set_content_length(contents.size());
          async_fetch_->HeadersComplete();
          ok = rewrite_context_->AbsolutifyIfNeeded(
              original_output_url_, contents, async_fetch_, handler_);
        } else {
          GoogleString url = input_resource->url();
          handler_->Warning(
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

    FetchFallbackDoneImpl(contents, headers);
  }

  // Backend for FetchFallbackCacheDone, but can be also invoked
  // for main rewrite when background rewrite is detached.
  void FetchFallbackDoneImpl(const StringPiece& contents,
                             const ResponseHeaders* headers) {
    async_fetch_->response_headers()->CopyFrom(*headers);
    CHECK_EQ(1, rewrite_context_->num_output_partitions());
    const CachedResult* cached_result = rewrite_context_->output_partition(0);
    CHECK(cached_result != NULL);
    rewrite_context_->FixFetchFallbackHeaders(*cached_result,
                                              async_fetch_->response_headers());
    // Use the most conservative Cache-Control considering all inputs.
    ApplyInputCacheControl(async_fetch_->response_headers());
    if (!detached_) {
      // If we're detached then we don't know what the state of the metadata is
      // here as the Rewrite() could still be ongoing in the low-priority
      // thread.  So only add metadata to the response when not detached.
      AddMetadataHeaderIfNecessary(async_fetch_->response_headers());
    }
    async_fetch_->set_content_length(contents.size());
    async_fetch_->HeadersComplete();

    bool ok = rewrite_context_->AbsolutifyIfNeeded(original_output_url_,
                                                   contents, async_fetch_,
                                                   handler_);
    // Like FetchDone, we success false if not a 200.
    ok &= headers->status_code() == HttpStatus::kOK;
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

  // To skip rewriting on the fetch path, set to true.
  void set_skip_fetch_rewrite(bool x) { skip_fetch_rewrite_ = x; }
  bool skip_fetch_rewrite() { return skip_fetch_rewrite_; }

 private:
  void ApplyInputCacheControl(ResponseHeaders* headers) {
    ResourceVector inputs;
    for (int i = 0; i < rewrite_context_->num_slots(); i++) {
      inputs.push_back(rewrite_context_->slot(i)->resource());
    }

    rewrite_context_->FindServerContext()->ApplyInputCacheControl(inputs,
                                                                  headers);
  }

  RewriteContext* rewrite_context_;
  AsyncFetch* async_fetch_;
  OutputResourcePtr output_resource_;

  // Roughly the URL we were requested under (may have wrong hash or extension);
  // for use in absolutification. We need this since we may be doing a fallback
  // simultaneously to a rewrite which may be mutating output_resource_.
  GoogleString original_output_url_;
  MessageHandler* handler_;
  GoogleString requested_hash_;  // hash we were requested as. May be empty.
  QueuedAlarm* deadline_alarm_;

  bool success_;
  bool detached_;
  bool skip_fetch_rewrite_;
  Variable* const num_deadline_alarm_invocations_;

  DISALLOW_COPY_AND_ASSIGN(FetchContext);
};

// Helper for running filter's Rewrite method in low-priority rewrite thread,
// which deals with cancellation of rewrites due to load shedding or shutdown by
// introducing a kTooBusy response if the job gets dumped.
class RewriteContext::InvokeRewriteFunction : public Function {
 public:
  InvokeRewriteFunction(RewriteContext* context, int partition,
                        const OutputResourcePtr& output)
      : context_(context), partition_(partition), output_(output) {
  }

  virtual ~InvokeRewriteFunction() {}

  virtual void Run() {
    context_->FindServerContext()->rewrite_stats()->num_rewrites_executed()
        ->IncBy(1);
    context_->Rewrite(partition_,
                      context_->partitions_->mutable_partition(partition_),
                      output_);
  }

  virtual void Cancel() {
    context_->FindServerContext()->rewrite_stats()->num_rewrites_dropped()
        ->IncBy(1);
    context_->RewriteDone(kTooBusy, partition_);
  }

 private:
  RewriteContext* context_;
  int partition_;
  OutputResourcePtr output_;
};

RewriteContext::CacheLookupResultCallback::~CacheLookupResultCallback() {
}

void RewriteContext::InitStats(Statistics* stats) {
  stats->AddVariable(kNumRewritesAbandonedForLockContention);
  stats->AddVariable(kNumDistributedRewriteSuccesses);
  stats->AddVariable(kNumDistributedRewriteFailures);
  stats->AddVariable(kNumDistributedMetadataFailures);
  RewriteContext::FetchContext::InitStats(stats);
}

const char RewriteContext::kNumRewritesAbandonedForLockContention[] =
    "num_rewrites_abandoned_for_lock_contention";
const char RewriteContext::kNumDeadlineAlarmInvocations[] =
    "num_deadline_alarm_invocations";
const char RewriteContext::kNumDistributedRewriteFailures[] =
    "num_distributed_rewrite_failures";
const char RewriteContext::kNumDistributedRewriteSuccesses[] =
    "num_distributed_rewrite_successes";
const char RewriteContext::kNumDistributedMetadataFailures[] =
    "num_distributed_metadata_failures";
// kDistributedExt shouldn't be longer than
// ContentType::MaxProducedExtensionLength otherwise URL length estimation will
// break.
const char RewriteContext::kDistributedExt[] = "dist";
const char RewriteContext::kDistributedHash[] = "0";

RewriteContext::RewriteContext(RewriteDriver* driver,
                               RewriteContext* parent,
                               ResourceContext* resource_context)
  : started_(false),
    outstanding_fetches_(0),
    outstanding_rewrites_(0),
    resource_context_(resource_context),
    num_pending_nested_(0),
    parent_(parent),
    driver_((driver == NULL) ? parent->Driver() : driver),
    num_predecessors_(0),
    chained_(false),
    rewrite_done_(false),
    ok_to_write_output_partitions_(true),
    was_too_busy_(false),
    slow_(false),
    revalidate_ok_(true),
    notify_driver_on_fetch_done_(false),
    force_rewrite_(false),
    stale_rewrite_(false),
    is_metadata_cache_miss_(false),
    rewrite_uncacheable_(false),
    dependent_request_trace_(NULL),
    block_distribute_rewrite_(false),
    num_rewrites_abandoned_for_lock_contention_(
        Driver()->statistics()->GetVariable(
            kNumRewritesAbandonedForLockContention)),
    num_distributed_rewrite_failures_(
        Driver()->statistics()->GetVariable(kNumDistributedRewriteFailures)),
    num_distributed_rewrite_successes_(
        Driver()->statistics()->GetVariable(kNumDistributedRewriteSuccesses)),
    num_distributed_metadata_failures_(
        Driver()->statistics()->GetVariable(kNumDistributedMetadataFailures)) {
  DCHECK((driver == NULL) != (parent == NULL));  // Exactly one is non-NULL.
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
  const int index = num_slots() - 1;
  slot(index)->DetachContext(this);
  RewriteContext* predecessor = slot(index)->LastContext();
  if (predecessor) {
    predecessor->successors_.erase(
        std::find(predecessor->successors_.begin(),
                  predecessor->successors_.end(), this));
    --num_predecessors_;
  }

  slots_.pop_back();
  render_slots_.pop_back();
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

  // See if any of the input slots are marked as unsafe for use,
  // and if so bail out quickly.
  // TODO(morlovich): Add API for filters to do something more refined.
  for (int c = 0; c < num_slots(); ++c) {
    if (slot(c)->disable_further_processing()) {
      rewrite_done_ = true;
      if (!has_parent()) {
        AbstractLogRecord* log_record = Driver()->log_record();
        ScopedMutex lock(log_record->mutex());
        MetadataCacheInfo* metadata_log_info =
            log_record->logging_info()->mutable_metadata_cache_info();
        metadata_log_info->set_num_disabled_rewrites(
            metadata_log_info->num_disabled_rewrites() + 1);
      }
      Cancel();
      RetireRewriteForHtml(false /* no rendering*/);
      return;
    }
  }

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
  CacheInterface* metadata_cache = FindServerContext()->metadata_cache();
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
// explicit limitation on the number of file.  Additionally,
// old file-systems may not be efficiently indexed, in which case
// adding some hierarchy should help.
GoogleString HashSplit(const Hasher* hasher, const StringPiece& str) {
  GoogleString hash_buffer = hasher->Hash(str);
  StringPiece hash(hash_buffer);
  return StrCat(hash.substr(0, 1), "/", hash.substr(1));
}

}  // namespace

// Utility to log metadata cache lookup info.
// This executes in driver's rewrite thread, i.e., all calls to this are from
// Functions added to the same QueuedWorkedPool::Sequence and so none of the
// calls will be concurrent.
void RewriteContext::LogMetadataCacheInfo(bool cache_ok, bool can_revalidate) {
  if (has_parent()) {
    // We do not log nested rewrites.
    return;
  }
  {
    AbstractLogRecord* log_record = Driver()->log_record();
    ScopedMutex lock(log_record->mutex());
    MetadataCacheInfo* metadata_log_info =
        log_record->logging_info()->mutable_metadata_cache_info();
    if (cache_ok) {
      metadata_log_info->set_num_hits(metadata_log_info->num_hits() + 1);
      if (stale_rewrite_) {
        metadata_log_info->set_num_stale_rewrites(
            metadata_log_info->num_stale_rewrites() + 1);
      }
    } else if (can_revalidate) {
      metadata_log_info->set_num_revalidates(
          metadata_log_info->num_revalidates() + 1);
    } else {
      metadata_log_info->set_num_misses(metadata_log_info->num_misses() + 1);
    }
  }
}

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
  StringVector url_keys;
  const Hasher* hasher = FindServerContext()->lock_hasher();
  GoogleString url_key;
  GoogleString signature = hasher->Hash(Options()->signature());
  GoogleString suffix = CacheKeySuffix();

  if (num_slots() == 1) {
    // Usually a resource-context-specific encoding such as the
    // image dimension will be placed ahead of the URL.  However,
    // in the cache context, we want to put it at the end, so
    // put this encoding right before any context-specific suffix.
    url_keys.push_back("");
    GoogleString encoding;
    encoder()->Encode(url_keys, resource_context_.get(), &encoding);
    GoogleString tmp = StrCat(encoding, "@",
                              UserAgentCacheKey(resource_context_.get()), "_",
                              suffix);
    suffix.swap(tmp);

    url_key = slot(0)->resource()->cache_key();
    // TODO(morlovich): What this is really trying to ask is whether the
    // cache key is long and lacking natural /-separated structure.
    if (IsDataUrl(url_key)) {
      url_key = HashSplit(hasher, url_key);
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
    url_key = "empty";
  } else {
    for (int i = 0, n = num_slots(); i < n; ++i) {
      ResourcePtr resource(slot(i)->resource());
      url_keys.push_back(resource->cache_key());
    }
    encoder()->Encode(url_keys, resource_context_.get(), &url_key);
    url_key = HashSplit(hasher, url_key);
  }

  partition_key_ = StrCat(ServerContext::kCacheKeyResourceNamePrefix,
                          id(), "_", signature, "/",
                          url_key, "@", suffix);
}

void RewriteContext::AddRecheckDependency() {
  int64 ttl_ms = Options()->implicit_cache_ttl_ms();
  int64 now_ms = FindServerContext()->timer()->NowMs();
  if (num_slots() == 1) {
    ResourcePtr resource(slot(0)->resource());
    HTTPCache* http_cache = FindServerContext()->http_cache();
    switch (resource->fetch_response_status()) {
      case Resource::kFetchStatusOK:
        ttl_ms = std::max(ttl_ms, (resource->CacheExpirationTimeMs() - now_ms));
        break;
      case Resource::kFetchStatusDropped:
        ttl_ms = http_cache->remember_fetch_dropped_ttl_seconds() *
            Timer::kSecondMs;
        break;
      case Resource::kFetchStatusNotSet:
      case Resource::kFetchStatusOther:
        break;
      case Resource::kFetchStatus4xxError:
        ttl_ms = Driver()->options()->metadata_input_errors_cache_ttl_ms();
        break;
      case Resource::kFetchStatusUncacheable:
        ttl_ms = FindServerContext()->http_cache()->
            remember_not_cacheable_ttl_seconds() * Timer::kSecondMs;
        break;
    }
  }
  InputInfo* force_recheck = partitions_->add_other_dependency();
  force_recheck->set_type(InputInfo::CACHED);
  force_recheck->set_expiration_time_ms(now_ms + ttl_ms);
}

void RewriteContext::OutputCacheDone(CacheLookupResult* cache_result) {
  DCHECK_LE(0, outstanding_fetches_);

  scoped_ptr<CacheLookupResult> owned_cache_result(cache_result);

  partitions_.reset(owned_cache_result->partitions.release());
  LogMetadataCacheInfo(owned_cache_result->cache_ok,
                       owned_cache_result->can_revalidate);

  // If something already created output resources (like DistributedRewriteDone)
  // then don't append new ones here.
  bool create_outputs = outputs_.empty();

  // If OK or worth rechecking, set things up for the cache hit case.
  if (owned_cache_result->cache_ok || owned_cache_result->can_revalidate) {
    for (int i = 0, n = partitions_->partition_size(); i < n; ++i) {
      const CachedResult& partition = partitions_->partition(i);

      // Extract the further processing bit from InputInfo structures
      // back into the slots.
      for (int j = 0; j < partition.input_size(); ++j) {
        const InputInfo& input = partition.input(j);
        if (input.disable_further_processing()) {
          int slot_index = input.index();
          if (slot_index < 0 || slot_index >= static_cast<int>(slots_.size())) {
            LOG(DFATAL) << "Index of processing disabled slot out of range:"
                        << slot_index;
          } else {
            slots_[slot_index]->set_disable_further_processing(true);
          }
        }
      }

      // Create output resources, if appropriate.
      OutputResourcePtr output_resource;
      if (create_outputs) {
        if (partition.optimizable() &&
            CreateOutputResourceForCachedOutput(&partition, &output_resource)) {
          outputs_.push_back(output_resource);
        } else {
          outputs_.push_back(OutputResourcePtr(NULL));
        }
      }
    }
  }

  // If the cache gave a miss, or yielded unparsable data, then acquire a lock
  // and start fetching the input resources.
  if (owned_cache_result->cache_ok) {
    OutputCacheHit(false /* no need to write back to cache*/);
  } else {
    MarkSlow();
    if (owned_cache_result->can_revalidate) {
      OutputCacheRevalidate(owned_cache_result->revalidate);
    } else {
      OutputCacheMiss();
    }
  }
}

void RewriteContext::OutputCacheHit(bool write_partitions) {
  Freshen();
  for (int i = 0, n = partitions_->partition_size(); i < n; ++i) {
    if (outputs_[i].get() != NULL) {
      RenderPartitionOnDetach(i);
    }
  }
  ok_to_write_output_partitions_ = write_partitions;
  Finalize();
}

void RewriteContext::OutputCacheMiss() {
  is_metadata_cache_miss_ = true;
  outputs_.clear();
  partitions_->Clear();
  ServerContext* server_context = FindServerContext();
  if (server_context->shutting_down() && !RunningOnValgrind()) {
    FindServerContext()->message_handler()->Message(
        kInfo,
        "RewriteContext::OutputCacheMiss called with "
        "server_context->shutting_down(); leaking the context.");
  } else if (ShouldDistributeRewrite()) {
    DistributeRewrite();
  } else if (server_context->TryLockForCreation(Lock())) {
    FetchInputs();
  } else {
    num_rewrites_abandoned_for_lock_contention_->Add(1);
    MarkTooBusy();
    Activate();
  }
}

bool RewriteContext::IsDistributedRewriteForHtml() const {
  const RequestHeaders* request_headers = Driver()->request_headers();
  if (request_headers != NULL &&
      request_headers->HasValue(HttpAttributes::kXPsaDistributedRewriteForHtml,
                                Options()->distributed_rewrite_key())) {
    return true;
  }
  return false;
}

bool RewriteContext::ShouldDistributeRewrite() const {
  // We can distribute if the context allows it, if we're not currently serving
  // a distributed request, and if we're configured for distributed rewrites.
  const RequestHeaders* request_headers = Driver()->request_headers();

  // Only the first filter in a chain is allowed to be distributed. This is
  // because subsequent filters in the chain rely on the output of previous
  // filters which does not get passed with a distributed request.

  // TODO(jkarlin): We should relax this constraint so that other filters can
  // be distributed.  We'll have to pass the slot->resource as part of the
  // distributed call.
  if (chained()) {
    return false;
  }

  if (block_distribute_rewrite_
      || IsFetchRewrite()
      || request_headers == NULL
      || slots_.size() != 1  // Note: we can't distribute combiners.
      || Driver()->distributed_fetcher() == NULL
      || !Options()->Distributable(id())
      || Options()->distributed_rewrite_key().empty()
      || Options()->distributed_rewrite_servers().empty()) {
    return false;
  }
  // Don't redistribute an already distributed rewrite unless this is a nested
  // filter. For instance, if this is a distributed CSS request, we don't want
  // to redistribute the CSS rewrite but its nested image filters should be
  // allowed to be distributed.  The rewrite task of the nested filter will
  // not redistribute it. Note: We don't verify the distributed rewrite key
  // because we want to be conservative about loop detection.
  if (request_headers != NULL && parent() == NULL) {
    if (request_headers->Has(HttpAttributes::kXPsaDistributedRewriteFetch) ||
        request_headers->Has(HttpAttributes::kXPsaDistributedRewriteForHtml)) {
      return false;
    }
  }

  return true;
}

// Ex. input: http://www.example.com/a.png with an image compression context
//    output: http://www.example.com/50x50xa.png.pagespeed.ic.0.dist
GoogleString RewriteContext::DistributedFetchUrl(StringPiece url) {
  GoogleUrl gurl(url);

  // TODO(jkarlin): Could we instead use DecodeOutputResource to get the URL?

  // First encode the resource segment with resource_context information.
  StringVector leaves;
  leaves.push_back(gurl.LeafWithQuery().as_string());
  GoogleString encoded_leaf;
  encoder()->Encode(leaves, resource_context_.get(), &encoded_leaf);

  // TODO(jkarlin): Maybe we can store this output in outputs_ and write the
  // response data to it instead of replicating this work later.
  OutputResourcePtr output(Driver()->CreateOutputResourceWithPath(
      gurl.AllExceptLeaf(), gurl.AllExceptLeaf(), Driver()->base_url().Origin(),
      id(), encoded_leaf, kind()));

  if (output.get() == NULL) {
    return "";
  }

  output->mutable_full_name()->set_hash(kDistributedHash);
  output->mutable_full_name()->set_ext(kDistributedExt);
  return output->url();
}

void RewriteContext::DistributeRewrite() {
  const RequestHeaders* request_headers = Driver()->request_headers();
  DCHECK(request_headers != NULL)
      << "Need request headers when distributing rewrites.";
  DCHECK_EQ(1, static_cast<int>(
                   slots_.size()));  // Guarded in ShouldDistributeRewrite().
  ResourcePtr resource = slots_[0]->resource();

  // Convert the URL into a .pagespeed. URL whose reconstruction will result
  // in the optimization we need.
  GoogleString reconstruction_url = DistributedFetchUrl(resource->url());
  if (reconstruction_url.empty()) {
    DistributeRewriteDone(false);
    return;
  }
  distributed_fetch_.reset(new DistributedRewriteFetch(
      Driver()->request_context(), reconstruction_url, request_headers, this,
      Driver()->distributed_fetcher(), FindServerContext()->message_handler()));
  distributed_fetch_->DispatchForHTML();
}

bool RewriteContext::ParseAndRemoveMetadataFromResponseHeaders(
    ResponseHeaders* response_headers, CacheLookupResult* cache_result) {
  if (response_headers == NULL) {
    return false;
  }

  const char* encoded_serialized =
      response_headers->Lookup1(HttpAttributes::kXPsaResponseMetadata);
  if (encoded_serialized != NULL) {
    GoogleString decoded_serialized;
    if (Mime64Decode(encoded_serialized, &decoded_serialized)) {
      // Sanitize the headers.
      encoded_serialized = NULL;
      response_headers->RemoveAll(HttpAttributes::kXPsaResponseMetadata);

      cache_result->cache_ok = true;
      cache_result->can_revalidate = false;
      cache_result->partitions.reset(new OutputPartitions);
      if (cache_result->partitions->ParseFromString(decoded_serialized)) {
        return true;
      }
    }
    num_distributed_metadata_failures_->Add(1);
  }
  return false;
}

// The distributed rewrite fetch is complete. If it succeeded then use the
// response content to rewrite the resource otherwise fall back to the original
// URL.
void RewriteContext::DistributeRewriteDone(bool success) {
  DCHECK_EQ(1, static_cast<int>(
                   slots_.size()));  // Guarded in ShouldDistributeRewrite().

  // Note that failure can occur before the RPC is made (such as if the
  // reconstruction URL is too long).
  (success ? num_distributed_rewrite_successes_
           : num_distributed_rewrite_failures_)->Add(1);

  if (success) {
    // We got something back, let's fill in a CacheLookupResult as if we'd had
    // a cache hit.
    scoped_ptr<CacheLookupResult> result(new CacheLookupResult);

    ResponseHeaders* response_headers = distributed_fetch_->response_headers();
    StringPiece contents = distributed_fetch_->contents();

    if (ParseAndRemoveMetadataFromResponseHeaders(response_headers,
                                                  result.get())) {
      DCHECK_EQ(1, result->partitions->partition_size());
      // If we have any content, write the response headers and contents to an
      // output resource. Chained rewrites need this to communicate the output
      // of one rewrite to the input of the next through the slot. Nested
      // rewriters must do this to report their output for harvest.
      // Specifically, IPRO needs this if in_place_wait_for_optimized is true as
      // it expects its nested rewriters to have the optimized resource in their
      // output resource.
      if (!contents.empty()) {
        OutputResourcePtr output_resource;
        if (CreateOutputResourceFromContent(result->partitions->partition(0),
                                            *response_headers, contents,
                                            &output_resource)) {
          outputs_.push_back(output_resource);
          output_resource->DetermineContentType();
        }
      }
      // Pretend we actually got a metadata cache hit, but avoid writing
      // back to cache.  OutputCacheDone will not overwrite any outputs
      // that were created in this function.
      ok_to_write_output_partitions_ = false;
      OutputCacheDone(result.release());
      return;
    }
  }
  // We didn't get a usable response (we would have returned early if we did),
  // so give up on this rewrite context.
  ok_to_write_output_partitions_ = false;
  Finalize();
}

bool RewriteContext::CreateOutputResourceFromContent(
    const CachedResult& cached_result, const ResponseHeaders& response_headers,
    StringPiece content, OutputResourcePtr* output_resource) {
  if (CreateOutputResourceForCachedOutput(&cached_result, output_resource)) {
    (*output_resource)->response_headers()->CopyFrom(response_headers);
    MessageHandler* message_handler = Driver()->message_handler();
    Writer* writer = (*output_resource)->BeginWrite(message_handler);
    writer->Write(content, message_handler);
    (*output_resource)->EndWrite(message_handler);
    return true;
  }
  return false;
}

void RewriteContext::OutputCacheRevalidate(
    const InputInfoStarVector& to_revalidate) {
  DCHECK(!to_revalidate.empty());
  outstanding_fetches_ = to_revalidate.size();

  for (int i = 0, n = to_revalidate.size(); i < n; ++i) {
    InputInfo* input_info = to_revalidate[i];
    ResourcePtr resource = slots_[input_info->index()]->resource();
    resource->LoadAsync(
        Resource::kReportFailureIfNotCacheable,
        Driver()->request_context(),
        new ResourceRevalidateCallback(this, resource, input_info));
  }
}

void RewriteContext::RepeatedSuccess(const RewriteContext* primary) {
  CHECK(outputs_.empty());
  CHECK_EQ(num_slots(), primary->num_slots());
  CHECK_EQ(primary->outputs_.size(),
           static_cast<size_t>(primary->num_output_partitions()));
  // Copy over busy bit, partition tables, outputs, and render_slot_ (as well as
  // was_optimized) information --- everything we can set in normal
  // OutputCacheDone.
  if (primary->was_too_busy_) {
    MarkTooBusy();
  }
  partitions_->CopyFrom(*primary->partitions_.get());
  for (int i = 0, n = primary->outputs_.size(); i < n; ++i) {
    outputs_.push_back(primary->outputs_[i]);
    if ((outputs_[i].get() != NULL) && !outputs_[i]->loaded()) {
      // We cannot safely alias resources that are not loaded, as the loading
      // process is threaded, and would therefore race. Therefore, recreate
      // another copy matching the cache data.
      CreateOutputResourceForCachedOutput(&partitions_->partition(i),
                                          &outputs_[i]);
    }
  }

  for (int i = 0, n = primary->num_slots(); i < n; ++i) {
    slot(i)->set_was_optimized(primary->slot(i)->was_optimized());
    slot(i)->set_disable_further_processing(
        primary->slot(i)->disable_further_processing());
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
    result = FindServerContext()->MakeCreationLock(lock_name);
    lock_.reset(result);
  }
  return result;
}

void RewriteContext::FetchInputs() {
  ++num_predecessors_;

  for (int i = 0, n = slots_.size(); i < n; ++i) {
    const ResourceSlotPtr& slot = slots_[i];
    ResourcePtr resource(slot->resource());
    if (!(resource->loaded() && resource->HttpStatusOk())) {
      ++outstanding_fetches_;

      // Sometimes we can end up needing pagespeed resources as inputs.
      // This can happen because we are doing a fetch of something produced
      // by chained rewrites, or when handling a 2nd (or further) step of a
      // chain during an HTML rewrite if we don't have the bits inside the
      // resource object (e.g. if we got a metadata hit on the previous step).
      bool handled_internally = false;
      GoogleUrl resource_gurl(resource->url());
      if (FindServerContext()->IsPagespeedResource(resource_gurl)) {
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
          // As a temporary workaround for bugs where FetchOutputResource
          // does not fully sync OutputResource with what it gives the
          // callback, we use FetchResource here and sync to the
          // resource object in the callback.
          bool ret = nested_driver->FetchResource(resource->url(), callback);
          DCHECK(ret);
        } else {
          nested_driver->Cleanup();
        }
      }

      if (!handled_internally) {
        Resource::NotCacheablePolicy noncache_policy =
            Resource::kReportFailureIfNotCacheable;
        if (IsFetchRewrite()) {
          // This is a fetch.  We want to try to get the input resource even if
          // it was previously noted to be uncacheable. Note that this applies
          // only to top-level rewrites: anything nested will still fail.
          DCHECK(!has_parent());
          if (!has_parent()) {
            noncache_policy = Resource::kLoadEvenIfNotCacheable;
          }
        }
        resource->LoadAsync(
            noncache_policy, Driver()->request_context(),
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
      // Increment num_successful_revalidates.
      if (!has_parent()) {
        AbstractLogRecord* log_record = Driver()->log_record();
        ScopedMutex lock(log_record->mutex());
        MetadataCacheInfo* metadata_log_info =
            log_record->logging_info()->mutable_metadata_cache_info();
        metadata_log_info->set_num_successful_revalidates(
            metadata_log_info->num_successful_revalidates() + 1);
      }
      OutputCacheHit(true /* update the cache with new timestamps*/);
    } else {
      OutputCacheMiss();
    }
  }
}

bool RewriteContext::ReadyToRewrite() const {
  DCHECK(!rewrite_done_);
  const bool ready = ((outstanding_fetches_ == 0) && (num_predecessors_ == 0));
  return ready;
}

void RewriteContext::Activate() {
  if (ReadyToRewrite()) {
    if (!IsFetchRewrite()) {
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

void RewriteContext::PartitionDone(RewriteResult result_or_busy) {
  bool result = false;
  switch (result_or_busy) {
    case kRewriteFailed:
      result = false;
      break;
    case kRewriteOk:
      result = true;
      break;
    case kTooBusy:
      MarkTooBusy();
      result = false;
      break;
  }

  if (!result) {
    partitions_->clear_partition();
    outputs_.clear();
  }

  outstanding_rewrites_ = partitions_->partition_size();
  if (outstanding_rewrites_ == 0) {
    DCHECK(!IsFetchRewrite());
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

    // Note that we run the actual rewrites in the "low priority" thread,
    // which makes it easy to cancel them if our backlog gets too horrid.
    //
    // This path corresponds either to HTML rewriting or to a rewrite nested
    // inside a fetch (top-levels for fetches are handled inside
    // StartRewriteForFetch), so failing it due to load-shedding will not
    // prevent us from serving requests.
    CHECK_EQ(outstanding_rewrites_, static_cast<int>(outputs_.size()));
    for (int i = 0, n = outstanding_rewrites_; i < n; ++i) {
      InvokeRewriteFunction* invoke_rewrite =
          new InvokeRewriteFunction(this, i, outputs_[i]);
      Driver()->AddLowPriorityRewriteTask(invoke_rewrite);
    }
  }
}

void RewriteContext::WritePartition() {
  ServerContext* server_context = FindServerContext();
  // If this was an IPRO rewrite which was forced for uncacheable rewrite, we
  // should not write partition data.
  if (ok_to_write_output_partitions_ && !server_context->shutting_down()) {
    // rewrite_uncacheable() is set in IPRO flow only, therefore there'll be
    // just one slot. If this was uncacheable rewrite, we should skip writing
    // to the metadata cache.
    const bool is_uncacheable_rewrite = rewrite_uncacheable() &&
        !slots_[0]->resource()->IsValidAndCacheable();
    if (!is_uncacheable_rewrite) {
      CacheInterface* metadata_cache = server_context->metadata_cache();
      GoogleString buf;
      {
#ifndef NDEBUG
        for (int i = 0, n = partitions_->partition_size(); i < n; ++i) {
          const CachedResult& partition = partitions_->partition(i);
          if (partition.optimizable() && !partition.has_inlined_data()) {
            GoogleUrl gurl(partition.url());
            DCHECK(gurl.IsWebValid()) << partition.url();
          }
        }
#endif

        StringOutputStream sstream(&buf);  // finalizes buf in destructor
        partitions_->SerializeToZeroCopyStream(&sstream);
      }
      metadata_cache->PutSwappingString(partition_key_, &buf);
    }
  } else {
    // TODO(jmarantz): if our rewrite failed due to lock contention or
    // being too busy, then cancel all successors.
  }
  lock_.reset();
}

void RewriteContext::FinalizeRewriteForHtml() {
  DCHECK(!IsFetchRewrite());

  int num_repeated = repeated_.size();
  if (!has_parent() && num_repeated > 0) {
    AbstractLogRecord* log_record = Driver()->log_record();
    ScopedMutex lock(log_record->mutex());
    MetadataCacheInfo* metadata_log_info =
        log_record->logging_info()->mutable_metadata_cache_info();
    metadata_log_info->set_num_repeated_rewrites(
        metadata_log_info->num_repeated_rewrites() + num_repeated);
  }
  bool partition_ok = (partitions_->partition_size() != 0);
  // Tells each of the repeated rewrites of the same thing if we have a valid
  // result or not.
  for (int c = 0; c < num_repeated; ++c) {
    if (partition_ok) {
      repeated_[c]->RepeatedSuccess(this);
    } else {
      repeated_[c]->RepeatedFailure();
    }
  }
  Driver()->DeregisterForPartitionKey(partition_key_, this);
  WritePartition();

  RetireRewriteForHtml(true /* permit rendering, if attached */);
}

void RewriteContext::RetireRewriteForHtml(bool permit_render) {
  DCHECK(driver_ != NULL);
  if (parent_ != NULL) {
    Propagate(permit_render);
    parent_->NestedRewriteDone(this);
  } else {
    // The RewriteDriver is waiting for this to complete.  Defer to the
    // RewriteDriver to schedule the Rendering of this context on the main
    // thread.
    driver_->RewriteComplete(this, permit_render);
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

// Returns true if there is already an other_dependency input info with the
// same url.
bool RewriteContext::HasDuplicateOtherDependency(const InputInfo& input) {
  if (input.has_url()) {
    StringIntMap::const_iterator it = other_dependency_map_.find(input.url());
    if (it != other_dependency_map_.end()) {
      int index = it->second;
      const InputInfo& input_info = partitions_->other_dependency(index);
      if (input_info.expiration_time_ms() == input.expiration_time_ms()) {
        return true;
      }
    }
  }
  return false;
}

void RewriteContext::CheckAndAddOtherDependency(const InputInfo& input_info) {
  if (input_info.has_url() && HasDuplicateOtherDependency(input_info)) {
    return;
  }

  InputInfo* dep = partitions_->add_other_dependency();
  dep->CopyFrom(input_info);
  // The input index here is with respect to the nested context's inputs,
  // so would not be interpretable at top-level, and we don't use it for
  // other_dependency entries anyway, so be both defensive and frugal
  // and don't write it out.
  if (dep->has_index()) {
    dep->clear_index();
  }
  // Add this to the other_dependency_map.
  if (dep->has_url()) {
    int index = partitions_->other_dependency_size() - 1;
    other_dependency_map_[dep->url()] = index;
  }
}

void RewriteContext::NestedRewriteDone(const RewriteContext* context) {
  // Record any external dependencies we have.
  for (int p = 0; p < context->num_output_partitions(); ++p) {
    const CachedResult* nested_result = context->output_partition(p);
    for (int i = 0; i < nested_result->input_size(); ++i) {
      const InputInfo& input_info = nested_result->input(i);
      // De-dup while adding.
      CheckAndAddOtherDependency(input_info);
    }
  }

  for (int p = 0; p < context->partitions_->other_dependency_size(); ++p) {
    const InputInfo& other_dep = context->partitions_->other_dependency(p);
    CheckAndAddOtherDependency(other_dep);
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

void RewriteContext::RewriteDone(RewriteResult result, int partition_index) {
  // RewriteDone may be called from a low-priority rewrites thread.
  // Make sure the rest of the work happens in the high priority rewrite thread.
  Driver()->AddRewriteTask(
      MakeFunction(this, &RewriteContext::RewriteDoneImpl,
                   result, partition_index));
}

void RewriteContext::RewriteDoneImpl(RewriteResult result,
                                     int partition_index) {
  DCHECK(Driver()->request_context().get() != NULL);
  Driver()->request_context()->ReleaseDependentTraceContext(
      dependent_request_trace_);
  dependent_request_trace_ = NULL;
  if (result == kTooBusy) {
    MarkTooBusy();
  } else {
    CachedResult* partition =
        partitions_->mutable_partition(partition_index);
    bool optimizable = (result == kRewriteOk);

    // Persist disable_further_processing bits from slots in the corresponding
    // InputInfo entries in metadata cache.
    for (int i = 0; i < partition->input_size(); ++i) {
      InputInfo* input = partition->mutable_input(i);
      if (!input->has_index()) {
        LOG(DFATAL) << "No index on InputInfo. Huh?";
      } else {
        if (slot(input->index())->disable_further_processing()) {
          input->set_disable_further_processing(true);
        }
      }
    }

    partition->set_optimizable(optimizable);
    if (optimizable && (!IsFetchRewrite())) {
      // TODO(morlovich): currently in async mode, we tie rendering of slot
      // to the optimizable bit, making it impossible to do per-slot mutation
      // that doesn't involve the output URL.
      RenderPartitionOnDetach(partition_index);
    }
  }
  --outstanding_rewrites_;
  if (outstanding_rewrites_ == 0) {
    if (IsFetchRewrite()) {
      fetch_->set_success((result == kRewriteOk));
    }
    Finalize();
  }
}

void RewriteContext::Harvest() {
}

void RewriteContext::Render() {
}

void RewriteContext::WillNotRender() {
}

void RewriteContext::Cancel() {
}

void RewriteContext::Propagate(bool render_slots) {
  DCHECK(rewrite_done_ && (num_pending_nested_ == 0));
  if (rewrite_done_ && (num_pending_nested_ == 0)) {
    if (render_slots) {
      if (was_too_busy_) {
        WillNotRender();
      } else {
        Render();
      }
    }
    CHECK_EQ(num_output_partitions(), static_cast<int>(outputs_.size()));
    for (int p = 0, np = num_output_partitions(); p < np; ++p) {
      CachedResult* partition = output_partition(p);
      for (int i = 0, n = partition->input_size(); i < n; ++i) {
        int slot_index = partition->input(i).index();
        if (render_slots_[slot_index]) {
          ResourcePtr resource(outputs_[p]);
          slots_[slot_index]->SetResource(resource);
          if (render_slots && partition->url_relocatable() && !was_too_busy_) {
            // This check for relocatable is potentially unsafe in that later
            // filters might still try to relocate the resource.  We deal with
            // this for the current case of javscript by having checks in each
            // potential later filter (combine and inline) that duplicate the
            // logic that went into setting url_relocatable on the partition.
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
  if (IsFetchRewrite()) {
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

void RewriteContext::DetachSlots() {
  for (int i = 0, n = slots_.size(); i < n; ++i) {
    slot(i)->DetachContext(this);
  }
}

void RewriteContext::AttachDependentRequestTrace(const StringPiece& label) {
  DCHECK(dependent_request_trace_ == NULL);
  RewriteDriver* driver = Driver();
  DCHECK(driver->request_context().get() != NULL);
  dependent_request_trace_ =
      driver->request_context()->CreateDependentTraceContext(label);
}

void RewriteContext::TracePrintf(const char* fmt, ...) {
  RewriteDriver* driver = Driver();
  if (driver->trace_context() == NULL) {
    return;
  }
  if (!driver->trace_context()->tracing_enabled()) {
    return;
  }
  va_list argp;
  va_start(argp, fmt);
  GoogleString buf;
  StringAppendV(&buf, fmt, argp);
  va_end(argp);
  // Log in the root trace.
  driver->trace_context()->TracePrintf("%s", buf.c_str());
  // Log to our context's request trace, if any.
  if (dependent_request_trace_ != NULL) {
    dependent_request_trace_->TracePrintf("%s", buf.c_str());
  }
}

void RewriteContext::RunSuccessors() {
  DetachSlots();

  for (int i = 0, n = successors_.size(); i < n; ++i) {
    RewriteContext* successor = successors_[i];
    if (--successor->num_predecessors_ == 0) {
      successor->Initiate();
    }
  }
  successors_.clear();
  if (parent_ == NULL) {
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
    if (resource->loaded() && resource->HttpStatusOk() &&
        !(Options()->disable_rewrite_on_no_transform() &&
          resource->response_headers()->HasValue(HttpAttributes::kCacheControl,
                                                 "no-transform"))) {
      bool on_the_fly = (kind() == kOnTheFlyResource);
      Resource::HashHint hash_hint = on_the_fly ?
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
  if (ok_to_rewrite && !fetch_->skip_fetch_rewrite()) {
    // Generally, we want to do all rewriting in the low-priority thread,
    // to ensure the main rewrite thread is always responsive. However, the
    // low-priority thread's tasks may get cancelled due to load-shedding,
    // so we have to be careful not to do it for filters where falling back
    // to an input isn't an option (such as combining filters or filters that
    // set OptimizationOnly() to false).
    InvokeRewriteFunction* call_rewrite =
        new InvokeRewriteFunction(this, 0, output);
    if (CanFetchFallbackToOriginal(kFallbackDiscretional) ||
        IsDistributedRewriteForHtml()) {
      // To avoid rewrites from delaying fetches, we try to fallback to the
      // original version if rewriting takes too long. We treat distributed
      // fetches on behalf of HTML-based rewrite contexts the same way, as that
      // is how they would be treated if they weren't distributed.
      fetch_->SetupDeadlineAlarm();
      Driver()->AddLowPriorityRewriteTask(call_rewrite);
    } else {
      Driver()->AddRewriteTask(call_rewrite);
    }
  } else {
    partition->clear_input();
    AddRecheckDependency();
    RewriteDone(kRewriteFailed, 0);
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
  if (gurl.IsWebValid() && namer.Decode(gurl.LeafWithQuery())) {
    output_resource->reset(
        new OutputResource(FindServerContext(),
                           gurl.AllExceptLeaf() /* resolved_base */,
                           gurl.AllExceptLeaf() /* unmapped_base */,
                           Driver()->base_url().Origin() /* original_base */,
                           namer, Options(), kind()));
    // We trust the type here since we should have gotten it right when
    // writing it into the cache.
    (*output_resource)->SetType(content_type);
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
  PartitionDone(Partition(partitions, outputs) ? kRewriteOk : kRewriteFailed);
}

void RewriteContext::CrossThreadPartitionDone(RewriteResult result) {
  Driver()->AddRewriteTask(
      MakeFunction(this, &RewriteContext::PartitionDone, result));
}

// Helper function to create a resource pointer to freshen the resource.
ResourcePtr RewriteContext::CreateUrlResource(const StringPiece& input_url) {
  const GoogleUrl resource_url(input_url);
  ResourcePtr resource;
  if (resource_url.IsWebValid()) {
    resource = Driver()->CreateInputResource(resource_url);
  }
  return resource;
}

// Determine whether the input info is imminently expiring and needs to
// be freshened. Freshens the resource and update metadata if required.
void RewriteContext::CheckAndFreshenResource(
    const InputInfo& input_info, ResourcePtr resource, int partition_index,
    int input_index, FreshenMetadataUpdateManager* freshen_manager) {
  if (stale_rewrite_ ||
      ((input_info.type() == InputInfo::CACHED) &&
       input_info.has_expiration_time_ms() &&
       input_info.has_date_ms() &&
       ResponseHeaders::IsImminentlyExpiring(
           input_info.date_ms(),
           input_info.expiration_time_ms(),
           FindServerContext()->timer()->NowMs()))) {
    if (input_info.has_input_content_hash()) {
      RewriteFreshenCallback* callback =
          new RewriteFreshenCallback(resource, partition_index, input_index,
                                     freshen_manager);
      freshen_manager->IncrementFreshens(*partitions_.get());
      resource->Freshen(callback, FindServerContext()->message_handler());
    } else {
      // TODO(nikhilmadan): We don't actually update the metadata when the
      // InputInfo does not contain an input_content_hash. However, we still
      // re-fetch the original resource and update the HTTPCache.
      resource->Freshen(NULL, FindServerContext()->message_handler());
    }
  }
}

void RewriteContext::Freshen() {
  // Note: only CACHED inputs are freshened (not FILE_BASED or ALWAYS_VALID).
  FreshenMetadataUpdateManager* freshen_manager =
      new FreshenMetadataUpdateManager(
          partition_key_, FindServerContext()->metadata_cache(),
          FindServerContext()->thread_system()->NewMutex());
  for (int j = 0, n = partitions_->partition_size(); j < n; ++j) {
    const CachedResult& partition = partitions_->partition(j);
    for (int i = 0, m = partition.input_size(); i < m; ++i) {
      const InputInfo& input_info = partition.input(i);
      if (input_info.has_index()) {
        ResourcePtr resource(slots_[input_info.index()]->resource());
        CheckAndFreshenResource(input_info, resource, j, i, freshen_manager);
      }
    }
  }

  // Also trigger freshen for other dependency urls if they exist.
  // TODO(mpalem): Currently, the urls are stored in the input cache field
  // only if the proactive_resource_freshening() option is set. If this changes
  // in the future, remove this check so the freshen improvements apply.
  if (Options()->proactive_resource_freshening()) {
    for (int k = 0; k < partitions_->other_dependency_size(); ++k) {
      const InputInfo& input_info = partitions_->other_dependency(k);
      if (input_info.has_url()) {
        ResourcePtr resource = CreateUrlResource(input_info.url());
        if (resource.get() != NULL) {
          // Using a partition index of -1 to indicate that this is not
          // a partition input info but other dependency input info.
          CheckAndFreshenResource(input_info, resource,
                                  kOtherDependencyPartitionIndex, k,
                                  freshen_manager);
        }
      }
    }
  }

  freshen_manager->MarkAllFreshensTriggered();
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

    // Fix the output resource name based on the decoded urls and the real
    // options used while rewriting this request. Note that we must call
    // Encoder::Encode on the url vector before the urls in it are absolutified.
    GoogleString encoded_url;
    encoder()->Encode(urls, resource_context(), &encoded_url);
    Driver()->PopulateResourceNamer(
        id(), encoded_url, output_resource->mutable_full_name());

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

      if (check_for_multiple_rewrites) {
        scoped_ptr<GoogleUrl> orig_based_url(
            new GoogleUrl(original_base, urls[i]));
        if (FindServerContext()->IsPagespeedResource(*orig_based_url.get())) {
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
  Driver()->InitiateFetch(this);
  if (PrepareFetch(output_resource, fetch, message_handler)) {
    Driver()->AddRewriteTask(MakeFunction(this,
                                          &RewriteContext::StartFetch,
                                          &RewriteContext::CancelFetch));
    return true;
  } else {
    fetch->response_headers()->SetStatusAndReason(HttpStatus::kNotFound);
    return false;
  }
}

bool RewriteContext::PrepareFetch(
    const OutputResourcePtr& output_resource,
    AsyncFetch* fetch,
    MessageHandler* message_handler) {
  // Decode the URLs required to execute the rewrite.
  bool ret = false;
  RewriteDriver* driver = Driver();
  GoogleUrlStarVector url_vector;
  if (resource_context_ != NULL) {
    EncodeUserAgentIntoResourceContext(resource_context_.get());
  }
  if (DecodeFetchUrls(output_resource, message_handler, &url_vector)) {
    bool is_valid = true;
    for (int i = 0, n = url_vector.size(); i < n; ++i) {
      GoogleUrl* url = url_vector[i];
      if (!url->IsWebValid()) {
        is_valid = false;
        break;
      }

      if (!FindServerContext()->url_namer()->ProxyMode() &&
          !driver->MatchesBaseUrl(*url)) {
        // Reject absolute url references unless we're proxying.
        is_valid = false;
        message_handler->Message(kError, "Rejected absolute url reference %s",
                                 url->spec_c_str());
        break;
      }

      ResourcePtr resource(driver->CreateInputResource(*url));
      if (resource.get() == NULL) {
        // TODO(jmarantz): bump invalid-input-resource count
         is_valid = false;
         break;
      }
      if (!IsDistributedRewriteForHtml()) {
        resource->set_is_background_fetch(false);
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
      ret = true;
    }
  }

  return ret;
}

bool RewriteContext::LookupMetadataForOutputResourceImpl(
    OutputResourcePtr output_resource,
    const GoogleUrl& gurl,
    RewriteContext* rewrite_context,
    RewriteDriver* driver,
    GoogleString* error_out,
    CacheLookupResultCallback* callback) {
  scoped_ptr<RewriteContext> context(rewrite_context);

  StringAsyncFetch dummy_fetch(driver->request_context());
  if (!context->PrepareFetch(output_resource, &dummy_fetch,
                             driver->message_handler())) {
    *error_out = "PrepareFetch failed.";
    return false;
  }

  const GoogleString key = context->partition_key_;
  CacheInterface* metadata_cache =
      context->FindServerContext()->metadata_cache();
  metadata_cache->Get(key,
                      new LookupMetadataForOutputResourceCallback(
                              key, context.release(), callback));
  return true;
}

void RewriteContext::CancelFetch() {
  AsyncFetch* fetch = fetch_->async_fetch();
    fetch->response_headers()->SetStatusAndReason(
        HttpStatus::kInternalServerError  /* 500 */);
  FetchCallbackDone(false);
}

void RewriteContext::FetchCacheDone(CacheLookupResult* cache_result) {
  // If we have metadata during a resource fetch, we see if we can use it
  // to find a pre-existing result in HTTP cache we can serve. This is done
  // by sanity-checking the metadata here, then doing an async cache lookup via
  // FetchTryFallback, which in turn calls FetchFallbackCacheDone.
  // If we're successful at that point FetchContext::FetchFallbackDone
  // serves out the bits with a shortened TTL; if we fail at any point
  // we call StartFetchReconstruction which will invoke the normal process of
  // locking things, fetching inputs, rewriting, and so on.

  scoped_ptr<CacheLookupResult> owned_cache_result(cache_result);
  partitions_.reset(owned_cache_result->partitions.release());
  LogMetadataCacheInfo(owned_cache_result->cache_ok,
                       owned_cache_result->can_revalidate);

  if (owned_cache_result->cache_ok && (num_output_partitions() == 1)) {
    CachedResult* result = output_partition(0);
    OutputResourcePtr output_resource;
    if (result->optimizable() &&
        CreateOutputResourceForCachedOutput(result, &output_resource)) {
      // TODO(jkarlin): Add a NamedLock::HadContention() function and then
      // we would only need to do this second lookup if there was contention
      // on the lock or if the hash is different.

      // Try to do a cache look up on the proper hash; if it's available,
      // we can serve it.
      FetchTryFallback(output_resource->HttpCacheKey(),
                       output_resource->hash());
      return;
    } else if (CanFetchFallbackToOriginal(kFallbackDiscretional)) {
      // The result is not optimizable, and it makes sense to use
      // the original instead, so try to do that.
      // (For simplicity, we will do an another rewrite attempt if it's not in
      // the cache).
      FetchTryFallback(slot(0)->resource()->url(), "");
      return;
    }
  }

  // Didn't figure out anything clever; so just rewrite on demand.
  StartFetchReconstruction();
}

void RewriteContext::FetchTryFallback(const GoogleString& url,
                                      const StringPiece& hash) {
  FindServerContext()->http_cache()->Find(
      url, Driver()->CacheFragment(),
      FindServerContext()->message_handler(),
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
  async_fetch()->Done(success);  // deletes this.
  if (notify_driver != NULL) {
    notify_driver->FetchComplete();
  }
}

bool RewriteContext::CanFetchFallbackToOriginal(
    FallbackCondition condition) const {
  if (!OptimizationOnly() && (condition != kFallbackEmergency)) {
    // If the filter is non-discretionary we will run it unless it already
    // failed and we would rather serve -something-.
    return false;
  }
  // We can serve the original (well, perhaps with some absolutification) in
  // cases where there is a single input.
  return (num_slots() == 1);
}

void RewriteContext::StartFetch() {
  DCHECK_EQ(kind(), fetch_->output_resource()->kind());

  if (!CreationLockBeforeStartFetch()) {
    StartFetchImpl();
  } else {
    // Acquire the lock early, before checking the cache. This way, if another
    // context finished a rewrite while this one waited for the lock we can use
    // its cached output.
    FindServerContext()->LockForCreation(
        Lock(), Driver()->rewrite_worker(),
        MakeFunction(this, &RewriteContext::StartFetchImpl,
                     &RewriteContext::StartFetchImpl));
  }
}

void RewriteContext::StartFetchImpl() {
  // If we have an on-the-fly resource, we almost always want to reconstruct it
  // --- there will be no shortcuts in the metadata cache unless the rewrite
  // fails, and it's ultra-cheap to reconstruct anyway.
  if (kind() == kOnTheFlyResource) {
    StartFetchReconstruction();
  } else {
    // Try to lookup metadata, as it may mark the result as non-optimizable
    // or point us to the right hash.
    FindServerContext()->metadata_cache()->Get(
        partition_key_,
        new OutputCacheCallback(this, &RewriteContext::FetchCacheDone));
  }
}

void RewriteContext::StartFetchReconstruction() {
  // Note that in case of fetches we continue even if we didn't manage to
  // take the lock.
  partitions_->Clear();
  FetchInputs();
}

void RewriteContext::DetachFetch() {
  CHECK(IsFetchRewrite());
  fetch_->set_detached(true);
  Driver()->DetachFetch();
}

ServerContext* RewriteContext::FindServerContext() const {
  return Driver()->server_context();
}

const RewriteOptions* RewriteContext::Options() const {
  return Driver()->options();
}

void RewriteContext::FixFetchFallbackHeaders(
    const CachedResult& cached_result, ResponseHeaders* headers) {
  if (headers->Sanitize()) {
    headers->ComputeCaching();
  }

  // In the case of a resource fetch with hash mismatch, we will not have
  // inputs, so fix headers based on the metadata. As we do not consider
  // FILE_BASED inputs here, if all inputs are FILE_BASED, the TTL will be the
  // minimum of headers->cache_ttl_ms() and headers->implicit_cache_ttl_ms().
  int64 min_cache_expiry_time_ms = headers->cache_ttl_ms() + headers->date_ms();
  for (int i = 0, n = partitions_->partition_size(); i < n; ++i) {
    const CachedResult& partition = partitions_->partition(i);
    for (int j = 0, m = partition.input_size(); j < m; ++j) {
      const InputInfo& input_info = partition.input(j);
      if (input_info.type() == InputInfo::CACHED &&
          input_info.has_expiration_time_ms()) {
        int64 input_expiration_time_ms = input_info.expiration_time_ms();
        if (input_expiration_time_ms > 0) {
          min_cache_expiry_time_ms = std::min(input_expiration_time_ms,
                                              min_cache_expiry_time_ms);
        }
      }
    }
  }

  // Shorten cache length, and prevent proxies caching this, as it's under
  // the "wrong" URL.
  headers->SetDateAndCaching(
      headers->date_ms(),
      std::min(min_cache_expiry_time_ms - headers->date_ms(),
               headers->implicit_cache_ttl_ms()),
      ",private");
  headers->RemoveAll(HttpAttributes::kEtag);
  headers->ComputeCaching();
}

bool RewriteContext::FetchContextDetached() {
  DCHECK(IsFetchRewrite());
  return fetch_->detached();
}

bool RewriteContext::AbsolutifyIfNeeded(const StringPiece& /*output_url_base*/,
                                        const StringPiece& input_contents,
                                        Writer* writer,
                                        MessageHandler* handler) {
  return writer->Write(input_contents, handler);
}

AsyncFetch* RewriteContext::async_fetch() {
  DCHECK(IsFetchRewrite());
  return fetch_->async_fetch();
}

MessageHandler* RewriteContext::fetch_message_handler() {
  DCHECK(IsFetchRewrite());
  return fetch_->handler();
}

int64 RewriteContext::GetRewriteDeadlineAlarmMs() const {
  return Driver()->rewrite_deadline_ms();
}

namespace {

void AppendBool(GoogleString* out, const char* name, bool val,
                StringPiece prefix) {
  StrAppend(out, prefix, name, ": ", val ? "true\n": "false\n");
}

void AppendInt(GoogleString* out, const char* name, int val,
                StringPiece prefix) {
  StrAppend(out, prefix, name, ": ", IntegerToString(val), "\n");
}

}  // namespace

bool RewriteContext::IsNestedIn(StringPiece id) const {
  return parent_ != NULL && id == parent_->id();
}

GoogleString RewriteContext::ToString(StringPiece prefix) const {
  GoogleString out;
  StrAppend(&out, prefix, "Outputs(", IntegerToString(num_outputs()), "):");
  for (int i = 0; i < num_outputs(); ++i) {
    StrAppend(&out, " ", output(i)->UrlEvenIfHashNotSet());
  }
  StrAppend(&out, "\n");
  if (IsFetchRewrite()) {
    StrAppend(&out, prefix, "Fetch: ",
              fetch_->output_resource()->UrlEvenIfHashNotSet(), "\n");
  }
  AppendInt(&out, "num_slots()", num_slots(), prefix);
  AppendInt(&out, "outstanding_fetches", outstanding_fetches_, prefix);
  AppendInt(&out, "outstanding_rewrites", outstanding_rewrites_, prefix);
  AppendInt(&out, "succesors_.size()", successors_.size(), prefix);
  AppendInt(&out, "num_pending_nested", num_pending_nested_, prefix);
  AppendInt(&out, "num_predecessors", num_predecessors_, prefix);
  StrAppend(&out, prefix, "partition_key: ", partition_key_, "\n");
  AppendBool(&out, "started", started_, prefix);
  AppendBool(&out, "chained", chained_, prefix);
  AppendBool(&out, "rewrite_done", rewrite_done_, prefix);
  AppendBool(&out, "ok_to_write_output_partitions",
             ok_to_write_output_partitions_, prefix);
  AppendBool(&out, "was_too_busy", was_too_busy_, prefix);
  AppendBool(&out, "slow", slow_, prefix);
  AppendBool(&out, "revalidate_ok", revalidate_ok_, prefix);
  AppendBool(&out, "notify_driver_on_fetch_done", notify_driver_on_fetch_done_,
             prefix);
  AppendBool(&out, "force_rewrite", force_rewrite_, prefix);
  AppendBool(&out, "stale_rewrite", stale_rewrite_, prefix);
  return out;
}

}  // namespace net_instaweb
