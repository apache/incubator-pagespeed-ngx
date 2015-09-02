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

// Author: ksimbili@google.com (Kishore Simbili)

#ifndef PAGESPEED_KERNEL_BASE_JSON_WRITER_H_
#define PAGESPEED_KERNEL_BASE_JSON_WRITER_H_

#include <utility>
#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/writer.h"

namespace Json {
class Value;
}  // namespace Json

namespace net_instaweb {

static const char kInstanceHtml[] = "instance_html";

class HtmlElement;
class MessageHandler;

// Pair of panel json and start Element corresponding to the panel.
typedef std::pair<HtmlElement*, Json::Value*> ElementJsonPair;

// Writes bytes to top json of the stack.
class JsonWriter : public Writer {
 public:
  // It is assumed that the element_json_stack is available till the destruction
  // of the writer
  JsonWriter(Writer* writer,
             const std::vector<ElementJsonPair>* element_json_stack);
  virtual ~JsonWriter();

  virtual bool Write(const StringPiece& str, MessageHandler* message_handler);
  virtual bool Flush(MessageHandler* message_handler);
  // Updates the json dictionary with the buffer content so far.
  void UpdateDictionary();

 private:
  Writer* writer_;
  GoogleString buffer_;
  const std::vector<ElementJsonPair>* element_json_stack_;

  DISALLOW_COPY_AND_ASSIGN(JsonWriter);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_JSON_WRITER_H_
