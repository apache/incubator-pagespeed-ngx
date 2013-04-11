/*
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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_HTTP_VALUE_H_
#define NET_INSTAWEB_HTTP_PUBLIC_HTTP_VALUE_H_

#include <cstddef>                     // for size_t
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

class ResponseHeaders;
class MessageHandler;

// Provides shared, ref-counted, copy-on-write storage for HTTP
// contents, to aid sharing between active fetches and filters, and
// the cache, which from which data may be evicted at any time.
class HTTPValue : public Writer {
 public:
  HTTPValue() : contents_size_(0) {}

  // Clears the value (both headers and content)
  void Clear();

  // Is this HTTPValue empty
  bool Empty() const { return storage_.empty(); }

  // Sets the HTTP headers for this value. This method may only
  // be called once and must be called before or after all of the
  // contents are set (using the streaming interface Write).
  //
  // If Clear() is called, then SetHeaders() can be called once again.
  //
  // Does NOT take ownership of headers.
  // A non-const pointer is required for the response headers so that
  // the cache fields can be updated if necessary.
  void SetHeaders(ResponseHeaders* headers);

  // Writes contents into the HTTPValue object.  Write can be called
  // multiple times to append more data, and can be called before
  // or after SetHeaders.  However, SetHeaders cannot be interleaved
  // in between calls to Write.
  virtual bool Write(const StringPiece& str, MessageHandler* handler);
  virtual bool Flush(MessageHandler* handler);

  // Retrieves the headers, returning false if empty.
  bool ExtractHeaders(ResponseHeaders* headers, MessageHandler* handler) const;

  // Retrieves the contents, returning false if empty.  Note that the
  // contents are only guaranteed valid as long as the HTTPValue
  // object is in scope.
  bool ExtractContents(StringPiece* str) const;

  // Tests whether this reference is the only active one to the string object.
  bool unique() const { return storage_.unique(); }

  // Assigns the storage of an HTTPValue based on the provided storage.  This
  // can be used for a cache Get.  Returns false if the string is not
  // well-formed.
  //
  // Extracts the headers into the provided ResponseHeaders buffer.
  bool Link(SharedString* src, ResponseHeaders* headers,
            MessageHandler* handler);

  // Links two HTTPValues together, using the contents of 'src' and discarding
  // the contents of this.
  void Link(HTTPValue* src) {
    if (src != this) {
      storage_ = src->storage_;  // SharedString links via assignment.
      contents_size_ = src->contents_size();
    }
  }

  // Access the shared string, for insertion into a cache via Put.
  SharedString* share() { return &storage_; }

  size_t size() const { return storage_.size(); }
  int64 contents_size() { return contents_size_; }

 private:
  friend class HTTPValueTest;

  // Must be called with storage_ non-empty.
  char type_identifier() const { return *storage_.data(); }

  unsigned int SizeOfFirstChunk() const;
  void SetSizeOfFirstChunk(unsigned int size);
  int64 ComputeContentsSize() const;

  // Disconnects this HTTPValue from other HTTPValues that may share the
  // underlying storage, allowing a new buffer.
  void CopyOnWrite();

  SharedString storage_;
  // Member variable to keep the size of body in storage.
  int64 contents_size_;

  DISALLOW_COPY_AND_ASSIGN(HTTPValue);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_HTTP_VALUE_H_
