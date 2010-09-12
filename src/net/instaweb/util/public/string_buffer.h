/**
 * Copyright 2010 Google Inc.
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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_STRING_BUFFER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_STRING_BUFFER_H_

#include <vector>
#include "base/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;
class Writer;

// A string-buffer that can grow large without fragmenting memory.
// TODO(jmarantz): consider a shared implementation.  Consider
// pros and cons of an immutable interface (Append returns a new
// StringBuffer that references the old StringBuffer).  If we decide
// that's the right approach we should go find a 'rope' implementation.
class StringBuffer {
 public:
  explicit StringBuffer(const StringPiece& str) : size_(0) {
    Append(str);
  }
  StringBuffer() : size_(0) { }
  ~StringBuffer() { Clear(); }

  // Gets a read-buffer of size kReadBufferSize.  The caller
  // can then populate the read buffer, and must either commit
  // it with CommitReadBuffer or release it with AbandonReadBuffer.
  // This must be done before any other calls to the StringBuffer.
  char* AllocReadBuffer();
  void CommitReadBuffer(char* read_buffer, int size);
  void AbandonReadBuffer(char* read_buffer);

  // Appends more characters to the string buffer.
  void Append(const StringPiece& str);

  // Writes the string-buffer.
  bool Write(Writer* writer, MessageHandler* message_handler) const;

  void Clear();

  int size() const {return size_; }

  // Needless to say,this method is fragmentative, and should
  // only be used for debugging and testing.
  std::string ToString() const;

  bool operator==(const StringBuffer& that) const;
  bool operator!=(const StringBuffer& that) const {
    return !(*this == that);
  }

  void CopyFrom(const StringBuffer& src);

  // Exposes an interface to iterate over the strings in the buffer
  // that requires we store those pieces in a vector.
  // TODO(jmarantz): consider changing this and all call-sites to
  // use a proper iterator so we could switch represent the buffer
  // with a tree or list if desired.
  int num_pieces() const { return strings_.size(); }
  const StringPiece piece(int i) const { return StringPiece(*strings_[i]); }

  static size_t kReadBufferSize;
  static size_t npos;

  std::string SubString(size_t pos, size_t size = npos) const;

 private:
  friend class StringBufferTest;
  static size_t kMinStringSize;

  std::vector<std::string*> strings_;
  int size_;

  DISALLOW_COPY_AND_ASSIGN(StringBuffer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_STRING_BUFFER_H_
