// Copyright 2009 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_CORE_PAGESPEED_INPUT_H_
#define PAGESPEED_CORE_PAGESPEED_INPUT_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "pagespeed/core/input_capabilities.h"
#include "pagespeed/core/instrumentation_data.h"
#include "pagespeed/core/resource.h"
#include "pagespeed/core/resource_filter.h"

namespace pagespeed {

class ClientCharacteristics;
class DomDocument;
class ImageAttributes;
class ImageAttributesFactory;
class InputInformation;
class InstrumentationData;
class PagespeedInput;
class TopLevelBrowsingContext;

typedef std::map<std::string, ResourceSet> HostResourceMap;
typedef std::vector<const Resource*> ResourceVector;

// Implementations of this class can participate in the PagespeedInput::Freeze.
class PagespeedInputFreezeParticipant {
 public:
  PagespeedInputFreezeParticipant() {}
  virtual ~PagespeedInputFreezeParticipant() {}

  virtual void OnFreeze(PagespeedInput* pagespeed_input) = 0;
};

/**
 * Input set representation
 */
class PagespeedInput {
 public:
  enum OnloadState {
    // There is not enough information to know whether the onload
    // event has fired. This is the default state.
    UNKNOWN,

    // The onload event has already fired.
    ONLOAD_FIRED,

    // The onload event has not yet fired for this page.
    ONLOAD_NOT_YET_FIRED,
  };

  PagespeedInput();
  // PagespeedInput takes ownership of the passed resource_filter.
  explicit PagespeedInput(ResourceFilter* resource_filter);
  virtual ~PagespeedInput();

  // Setters

  // Adds a resource to the list.
  // Returns true if resource was added to the list.
  //
  // Ownership of the resource is transfered over to the
  // PagespeedInput object.
  bool AddResource(Resource* resource);

  // Specify the URL of the "primary" resource. Some rules want to exclude the
  // primary resource from their analysis. This is optional but should be
  // specified when there is a root resource, such as the main HTML
  // resource. This method should be called after the primary resource has
  // already been added via AddResource(); if called with a URL that is not in
  // the set of currently added resources, does nothing and returns false.
  bool SetPrimaryResourceUrl(const std::string& url);

  // Set the onload state for this page load. If setting to
  // ONLOAD_FIRED, you must also call SetOnloadTimeMillis with the onload
  // time. Note that it is not necessary to call this method if
  // setting to ONLOAD_FIRED; you can just call SetOnloadTimeMillis with
  // the onload time directly, which will update the onload state to
  // ONLOAD_FIRED.
  bool SetOnloadState(OnloadState state);

  // Set the onload time, in milliseconds, relative to the request
  // time of the first resource. Calling this method also sets the
  // onload state to ONLOAD_FIRED. It is not necessary to call
  // SetOnloadState(ONLOAD_FIRED) if you are calling this method.
  bool SetOnloadTimeMillis(int onload_millis);

  // Specify the client characteristics. ClientCharacteristics are
  // used to determine the relative impact of different kinds of
  // savings, e.g. to determine the relative impact of a byte saved as
  // compared to an RTT saved, etc.
  bool SetClientCharacteristics(const ClientCharacteristics& cc);

  // Set the DOM Document information.
  //
  // Ownership of the DomDocument is transfered over to the
  // PagespeedInput object.
  bool AcquireDomDocument(DomDocument* document);

  bool AcquireImageAttributesFactory(ImageAttributesFactory* factory);

  // Ownership of the InstrumentationData instances is transferred
  // over to the PagespeedInput object. Ownership of the vector itself
  // is not transferred, but the vector passed will be emptied.
  bool AcquireInstrumentationData(InstrumentationDataVector* data);

  // Sets the top level browsing context. If no top level browsing context is
  // set before Freeze(), it is constructed out of the DomDocument, if
  // available. Ownership of the context is transfered over to the
  // PagespeedInput object.
  bool AcquireTopLevelBrowsingContext(TopLevelBrowsingContext* context);

  // Call after populating the PagespeedInput. After calling Freeze(),
  // no additional modifications can be made to the PagespeedInput
  // structure.
  inline bool Freeze() {
    return Freeze(NULL);
  }

  // Call after populating the PagespeedInput. After calling Freeze(),
  // no additional modifications can be made to the PagespeedInput
  // structure. The freezeParticipant will be executed after the initialization
  // but before the input is frozen.
  bool Freeze(PagespeedInputFreezeParticipant* freezeParticipant);

  // Resource access.
  int num_resources() const;
  bool has_resource_with_url(const std::string& url) const;
  const Resource& GetResource(int idx) const;
  const Resource* GetResourceWithUrlOrNull(const std::string& url) const;

  // Get a non-const pointer to a resource. It is an error to call
  // these methods after this object has been frozen.
  Resource* GetMutableResource(int idx);
  Resource* GetMutableResourceWithUrl(const std::string& url);

  ImageAttributes* NewImageAttributes(const Resource* resource) const;

  const TopLevelBrowsingContext* GetTopLevelBrowsingContext() const;
  TopLevelBrowsingContext* GetMutableTopLevelBrowsingContext();

  // Get the map from hostname to all resources on that hostname.
  const HostResourceMap* GetHostResourceMap() const;

  // Get the set of all resources, sorted in request order. Will be
  // NULL if one or more resources does not have a request start
  // time.
  const ResourceVector* GetResourcesInRequestOrder() const;

  const InputInformation* input_information() const;
  const DomDocument* dom_document() const;
  const InstrumentationDataVector* instrumentation_data() const;

  const std::string& primary_resource_url() const;
  bool is_frozen() const;

  // Was the given resource loaded after onload? If timing data is
  // unavailable, or if onload has not yet fired, this method returns
  // false.
  bool IsResourceLoadedAfterOnload(const Resource& resource) const;

  // Estimate the InputCapabilities for this PagespeedInput.
  // Note that implementers should call this method
  // and also explicitly augment the bitmap with the capabilities they
  // provide.
  InputCapabilities EstimateCapabilities() const;

  int viewport_width() const { return viewport_width_; }
  int viewport_height() const { return viewport_height_; }

  bool SetViewportWidthAndHeight(int width, int height);

 private:
  bool IsValidResource(const Resource* resource) const;

  // Compute information about the set of resources. Called once at
  // the time the PagespeedInput is frozen.
  void PopulateInputInformation();
  void PopulateResourceInformationFromDom(
      std::map<const Resource*, ResourceType>*);
  void UpdateResourceTypes(const std::map<const Resource*, ResourceType>&);

  std::vector<Resource*> resources_;

  // Map from URL to Resource. The resources_ vector, above, owns the
  // Resource instances in this map.
  std::map<std::string, const Resource*> url_resource_map_;

  // Map from hostname to Resources on that hostname. The resources_
  // vector, above, owns the Resource instances in this map.
  HostResourceMap host_resource_map_;

  ResourceVector request_order_vector_;
  // List of timeline events.  The PagespeedInput object has ownership of these
  // InstrumentationData objects.
  // BEWARE: This field may be going away; we are not sure yet.  (mdsteele)
  InstrumentationDataVector timeline_data_;

  scoped_ptr<InputInformation> input_info_;
  scoped_ptr<DomDocument> document_;
  scoped_ptr<TopLevelBrowsingContext> top_level_browsing_context_;
  scoped_ptr<ResourceFilter> resource_filter_;
  scoped_ptr<ImageAttributesFactory> image_attributes_factory_;
  std::string primary_resource_url_;
  OnloadState onload_state_;
  int onload_millis_;

  enum InitializationState {
    INIT, FINALIZE, FROZEN
  };

  InitializationState initialization_state_;

  int viewport_width_;
  int viewport_height_;

  DISALLOW_COPY_AND_ASSIGN(PagespeedInput);
};

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_PAGESPEED_INPUT_H_
