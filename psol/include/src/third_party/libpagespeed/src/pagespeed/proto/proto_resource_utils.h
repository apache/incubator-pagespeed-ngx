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

#ifndef PAGESPEED_APPS_PROTO_RESOURCE_UTILS_H_
#define PAGESPEED_APPS_PROTO_RESOURCE_UTILS_H_

namespace pagespeed {

class PagespeedInput;
class ProtoInput;
class ProtoResource;
class Resource;

namespace proto {

// De-serialization from protocol buffer

// Populate a Resource based on the contents of a ProtoResource protocol buffer.
void PopulateResource(const ProtoResource& input, Resource* output);

// Populate a PagespeedInput based on the contents of a ProtoInput
// protocol buffer.
void PopulatePagespeedInput(const ProtoInput& proto_input,
                            PagespeedInput* pagespeed_input);

// Serialization to protocol buffer

// Populate a ProtoResource protocol buffer from a Resource object.
void PopulateProtoResource(const Resource& input, ProtoResource* output);

// Populate a ProtoInput protocol buffer based on the contents of a
// PagespeedInput object.
void PopulateProtoInput(const PagespeedInput& input, ProtoInput* proto_input);

}  // namespace proto

}  // namespace pagespeed

#endif  // PAGESPEED_APPS_PROTO_RESOURCE_UTILS_H_
