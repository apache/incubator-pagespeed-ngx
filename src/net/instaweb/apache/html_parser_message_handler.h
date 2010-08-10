// Copyright 2010 Google Inc.
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

#ifndef HTML_REWRITER_HTML_PARSER_MESSAGE_HANDLER_H_
#define HTML_REWRITER_HTML_PARSER_MESSAGE_HANDLER_H_

#include <string>

#include "net/instaweb/util/public/message_handler.h"

using net_instaweb::MessageHandler;
using net_instaweb::MessageType;

namespace html_rewriter {

// Message handler for directing all parser error and warning messages to
// Apache log.
class HtmlParserMessageHandler : public MessageHandler {
 public:
  virtual void MessageV(MessageType type, const char* msg, va_list args);
  virtual void FileMessageV(MessageType type, const char* filename, int line,
                            const char* msg, va_list args);
  virtual void InfoV(
      const char* filename, int line, const char *msg, va_list args);
  virtual void WarningV(
      const char* filename, int line, const char *msg, va_list args);
  virtual void ErrorV(
      const char* filename, int line, const char *msg, va_list args);
  void FatalErrorV(
      const char* filename, int line, const char* msg, va_list args);

 private:
  const std::string& Format(const char* msg, va_list args);
  std::string buffer_;
};

}  // namespace html_rewriter

#endif  // HTML_REWRITER_HTML_PARSER_MESSAGE_HANDLER_H_
