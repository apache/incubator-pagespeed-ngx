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

#ifndef PAGESPEED_CORE_INSTRUMENTATION_DATA_H_
#define PAGESPEED_CORE_INSTRUMENTATION_DATA_H_

#include <vector>

#include "base/basictypes.h"

namespace pagespeed {

class InstrumentationData;

typedef std::vector<const InstrumentationData*> InstrumentationDataVector;
typedef InstrumentationDataVector InstrumentationDataStack;
typedef std::vector<InstrumentationDataStack> InstrumentationDataStackVector;

class InstrumentationDataVisitor {
 public:
  InstrumentationDataVisitor();
  virtual ~InstrumentationDataVisitor();

  static void Traverse(InstrumentationDataVisitor* visitor,
                       const InstrumentationDataStack& data);

  static void Traverse(InstrumentationDataVisitor* visitor,
                       const InstrumentationData& data);

  // Invoked for each node in the InstrumentationData instances,
  // visited in pre-order. The stack parameter contains the stack of
  // nodes being visited, with the rootmost node at index 0. Return 0
  // to prevent traversal of children of the InstrumentationData at
  // the top of the stack.
  virtual bool Visit(const InstrumentationDataStack& stack) = 0;

 private:
  static void TraverseImpl(InstrumentationDataVisitor* visitor,
                           InstrumentationDataStack* stack);

  DISALLOW_COPY_AND_ASSIGN(InstrumentationDataVisitor);
};

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_INSTRUMENTATION_DATA_H_
