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
// Author: skerner@google.com (Sam Kerner)

#include "net/instaweb/spriter/image_library_interface.h"
#include <string>

namespace net_instaweb {
namespace spriter {

ImageLibraryInterface* ImageLibraryInterface::ImageLibraryInterfaceFactory(
    const std::string& library_name) {
  // TODO(skerner):  Implement some interfaces.  Will do OpenCV first.

  return NULL;
}

ImageLibraryInterface::ImageLibraryInterface(const FilePath& base_input_path,
                                             const FilePath& base_output_path,
                                             Delegate* delegate)
    : base_input_path_(base_input_path),
      base_output_path_(base_output_path),
      delegate_(delegate) {
}

}  // namespace spriter
}  // namespace net_instaweb
