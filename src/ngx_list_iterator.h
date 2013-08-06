/*
 * Copyright 2013 Google Inc.
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

// Author: jefftk@google.com (Jeff Kaufman)
//
// Simplifies iteration over nginx lists.
//
//

#ifndef NGX_LIST_ITERATOR_H_
#define NGX_LIST_ITERATOR_H_

extern "C" {
#include <ngx_http.h>
}

#include "base/basictypes.h"

namespace net_instaweb {

class NgxListIterator {
 public:
  explicit NgxListIterator(ngx_list_part_t* part);

  // Return the next element of the list if there is one, NULL otherwise.
  ngx_table_elt_t* Next();

 private:
  ngx_list_part_t* part_;
  ngx_uint_t index_within_part_;

  DISALLOW_COPY_AND_ASSIGN(NgxListIterator);
};

}  // namespace net_instaweb

#endif  // NGX_LIST_ITERATOR_H_
