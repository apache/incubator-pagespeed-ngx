// Copyright 2011 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PAGESPEED_CORE_RESOURCE_FETCH_H_
#define PAGESPEED_CORE_RESOURCE_FETCH_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

#include "pagespeed/proto/resource.pb.h"

namespace pagespeed {

class BrowsingContext;
class PagespeedInput;
class Resource;
class ResourceEvaluation;
class ResourceFetchDelay;
class ResourceFetchDownload;

// Describes the fetch of a resource. There are potentially multiple fetches
// of a single resource. The resource fetch is scoped to a browsing context.
class ResourceFetch {
 public:
  // Do not create instances directly, but rather obtain them using
  // BrowsingContext::CreateResourceFetch()
  ResourceFetch(const std::string& uri, const BrowsingContext* context,
                const Resource* resource,
                const PagespeedInput* pagespeed_input);
  virtual ~ResourceFetch();

  // Set how the browser discovered the resource that is fetched here.
  void SetDiscoveryType(ResourceDiscoveryType discovery_type);

  // Adds a ResourceFetchDelay to this ResourceFetch, which describes causes
  // of why a resource fetch was delayed. Multiple causes apply in the order
  // they are added.
  // The returned object might be modified, but this ResourceFetch keeps the
  // ownership.
  ResourceFetchDelay* AddFetchDelay();

  // Sets a description of the location where the fetch was initiated from. If
  // the request originated from JavaScript, this list represents the script
  // stack trace, where the first entry in the list represents the topmost stack
  // frame.
  // Ownership of the CodeLocation instances is transferred over to this
  // ResourceFetch object. Ownership of the vector itself is not transferred,
  // but the vector passed will be emptied.
  bool AcquireCodeLocation(std::vector<CodeLocation*>* location);

  // Finalizes this ResourceFetch and makes it immutable. Non-const methods
  // cannot be called after calling Finalize. Calling this method also ensures
  // that the RedirectDownloads are created, if applicable.
  bool Finalize();

  // Returns true if this ResourceFetch is finalized.
  bool IsFinalized() const {
    return finalized_;
  }

  // Returns the Resource to which this fetch applies.
  const Resource& GetResource() const {
    return *resource_;
  }

  // Returns the URI uniquely identifying this fetch.
  const std::string& GetResourceFetchUri() const {
    return data_->uri();
  }

  // Returns an enumeration indicating how this resource was discovered by the
  // browser.
  ResourceDiscoveryType GetDiscoveryType() const {
    return data_->type();
  }

  // Returns a description of the location where the fetch was initiated from.
  // If the request originated from JavaScript, this list represents the script
  // stack trace, where the first entry in the list represents the topmost stack
  // frame.
  bool GetCodeLocation(std::vector<const CodeLocation*>* location) const;

  // Returns the number of CodeLocation objects recorded.
  int GetCodeLocationCount() const;

  // Returns the index-th CodeLocation object. The index must be in the range of
  // 0 <= index < GetCodeLocationCount().
  const CodeLocation& GetCodeLocation(int index) const;

  // Returns the number of ResourceFetchDelay objects recorded.
  int GetFetchDelayCount() const;

  // Returns the index-th ResourceFetchDelay object. The index must be in the
  // range of 0 <= index < GetFetchDelayCount().
  const ResourceFetchDelay& GetFetchDelay(int index) const;

  // Returns the logical download for this fetch. The logical download folds
  // potential redirects into this download, therefore hides redirects from the
  // clients and makes analysis simpler. Its requestor is the same as the
  // requestor of the first resource in a redirect chain. The download times
  // include the time to download the redirects.
  const ResourceFetchDownload& GetDownload() const {
    return *logical_download_;
  }

  ResourceFetchDownload* GetMutableDownload();

  // Returns the redirect download for this fetch. The redirect download is only
  // available if the resource was loaded due to a redirect. Its requestor is
  // the evaluation of the HTTP headers that actually referred to the resource.
  // The download times only include the time spent downloading this resource.
  // clients and makes analysis simpler.
  const ResourceFetchDownload* GetRedirectDownload() const {
    return redirect_download_.get();
  }

  // A convenience accessor for GetDownload()->GetRequestor().
  const ResourceEvaluation* GetRequestor() const;

  // A convenience accessor for GetDownload()->GetStartTick().
  int64 GetStartTick() const;

  // A convenience accessor for GetDownload()->GetFinishTick().
  int64 GetFinishTick() const;

  // Serializes this ResourceFetch and all the ResourceFetchDelays to the
  // specified ResourceFetchData message.
  bool SerializeData(ResourceFetchData* data) const;

 private:
  const PagespeedInput* pagespeed_input_;
  const Resource* resource_;
  const BrowsingContext* context_;

  bool finalized_;

  scoped_ptr<ResourceFetchDownload> logical_download_;
  scoped_ptr<ResourceFetchDownload> redirect_download_;

  scoped_ptr<ResourceFetchData> data_;
  std::vector<ResourceFetchDelay*> delays_;

  DISALLOW_COPY_AND_ASSIGN(ResourceFetch);
};

class ResourceFetchDownload {
 public:
  // Do not create instances directly, but rather obtain them using
  // ResourceFetch::GetDownload() and ResourceFetch::GetRedirectDownload()
  explicit ResourceFetchDownload(const PagespeedInput* pagespeed_input);
  virtual ~ResourceFetchDownload();

  // Sets the ResourceEvaluation that discovered the resource that is fetched
  // here. This sets the requestor of the logical download.
  bool SetRequestor(const ResourceEvaluation* requestor);

  // Sets the timing information for this resource fetch. Pass in -1 for
  // msecs / ticks that are unknown. This sets the timing of the logical
  // download.
  void SetLoadTiming(int64 start_tick, int64 start_time_msec, int64 finish_tick,
                     int64 finish_time_msec);

  // Copies all data from the specified ResourceFetchDownload. The state this
  // ResourceFetchDownload had before calling this method is lost. If
  // keep_finish_time is true, the finish time of the specified download wont
  // be copied, marking this download as running from
  // download.GetStartTick() to this->GetFinishTick(). In this case, the
  // caller must ensure that the download start time is before this finish time.
  bool CopyFrom(const ResourceFetchDownload& download, bool keep_finish_time);

  // Returns the ResourceEvaluationData that caused this resource download.
  const ResourceEvaluation* GetRequestor() const;

  // Gets the tick value that describes the order of this load start
  // event, relative to other load and/or eval events. The number does not
  // represent the absolute start time.
  int64 GetStartTick() const {
    return data_->start().tick();
  }

  // Gets the tick value that describes the order of this load finish
  // event, relative to other load and/or eval events. The number does not
  // represent the absolute finish time.
  int64 GetFinishTick() const {
    return data_->finish().tick();
  }

  // Serializes this ResourceFetchDownload to the specified
  // ResourceFetchDownloadData message.
  bool SerializeData(ResourceFetchDownloadData* data) const;

 private:
  const PagespeedInput* pagespeed_input_;

  scoped_ptr<ResourceFetchDownloadData> data_;

  DISALLOW_COPY_AND_ASSIGN(ResourceFetchDownload);
};

// Describes the cause of why a resource was not fetched right after an
// evaluation. This is a simplified representation of what happens within the
// browser and does only model a subset of the reality.
class ResourceFetchDelay {
 public:
  // Do not create instances directly, but rather obtain them using
  // ResourceFetch::AddFetchDelay()
  ResourceFetchDelay();
  virtual ~ResourceFetchDelay();

  // Copies all data from the specified ResourceFetchDelay. The state this
  // ResourceFetchDelay had before calling this method is lost.
  bool CopyFrom(const ResourceFetchDelay& delay);

  // The location where the timer / event-listener was installed where the first
  // entry in the list represents the topmost stack frame.
  // Ownership of the CodeLocation instances is transferred over to this
  // ResourceFetch object. Ownership of the vector itself is not transferred,
  // but the vector passed will be emptied.
  bool AcquireCodeLocation(std::vector<CodeLocation*>* data);

  // Indicates that this delay was caused due to a timeout. The timeout duration
  // is specified
  void SetTimeout(int32 timeout) {
    data_->set_type(TIMEOUT);
    data_->set_timeout_msec(timeout);
  }

  // Indicates that the resource was only loaded after the event with the
  // specified name fired.
  void SetEvent(const std::string& event_name) {
    data_->set_type(EVENT);
    data_->set_event_name(event_name);
  }

  // Returns the type of the delay.
  ResourceFetchDelayType GetType() const {
    return data_->type();
  }

  // Returns the location where the timer / event-listener was installed.
  // The first entry in the list represents the topmost JavaScript stack frame.
  bool GetCodeLocation(std::vector<const CodeLocation*>* location) const;

  // Returns the number of CodeLocation objects recorded.
  int GetCodeLocationCount() const;

  // Returns the index-th CodeLocation object. The index must be in the range of
  // 0 <= index < GetCodeLocationCount().
  const CodeLocation& GetCodeLocation(int index) const;

  // If the delay was caused because the load was bound to an event, this
  // returns the name of the event.
  const std::string& GetEventName() const {
    return data_->event_name();
  }

  // If the delay was caused due to an timeout, this returns the length of the
  // timeout.
  int32 GetTimeoutMsec() const {
    return data_->timeout_msec();
  }

  // Serializes this ResourceFetchDelay to the specified ResourceFetchDelayData
  // message.
  bool SerializeData(ResourceFetchDelayData* data) const;

 private:
  scoped_ptr<ResourceFetchDelayData> data_;

  DISALLOW_COPY_AND_ASSIGN(ResourceFetchDelay);
};

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_RESOURCE_FETCH_H_
