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

#include "net/instaweb/http/public/http_value.h"

#include "base/logging.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace {

// The headers and body are both encoded into one Shared String, which can then
// be efficiently held in an in-memory cache, or passed around as an HTTPValue
// object.  The class supports both setting the headers first and then the body,
// and vice versa.  Both the headers and body are variable length, and to avoid
// having to re-shuffle memory, we encode which is first in the buffer as the
// first byte.  The next four bytes encode the size.
const char kHeadersFirst = 'h';
const char kBodyFirst = 'b';

const int kStorageTypeOverhead = 1;
const int kStorageSizeOverhead = 4;
const unsigned int kStorageOverhead =
    kStorageTypeOverhead + kStorageSizeOverhead;

}  // namespace

namespace net_instaweb {

class MessageHandler;

void HTTPValue::CopyOnWrite() {
  if (!storage_.unique()) {
    SharedString new_storage(*storage_);
    storage_ = new_storage;
  }
}

void HTTPValue::Clear() {
  CopyOnWrite();
  contents_size_ = 0;
  storage_->clear();
}

void HTTPValue::SetHeaders(ResponseHeaders* headers) {
  CopyOnWrite();
  GoogleString headers_string;
  StringWriter writer(&headers_string);
  headers->WriteAsBinary(&writer, NULL);
  if (storage_->empty()) {
    storage_->push_back(kHeadersFirst);
    SetSizeOfFirstChunk(headers_string.size());
  } else {
    CHECK(type_identifier() == kBodyFirst);
    // Using 'unsigned int' to facilitate bit-shifting in
    // SizeOfFirstChunk and SetSizeOfFirstChunk, and I don't
    // want to worry about sign extension.
    unsigned int size = SizeOfFirstChunk();
    CHECK(storage_->size() == (kStorageOverhead + size));
  }
  storage_->append(headers_string);
}

bool HTTPValue::Write(const StringPiece& str, MessageHandler* handler) {
  CopyOnWrite();
  if (storage_->empty()) {
    storage_->push_back(kBodyFirst);
    storage_->append("    ", 4);
    SetSizeOfFirstChunk(str.size());
  } else if (type_identifier() == kBodyFirst) {
    CHECK(storage_->size() >= kStorageOverhead);
    unsigned int string_size = SizeOfFirstChunk();
    CHECK(string_size == storage_->size() - kStorageOverhead);
    SetSizeOfFirstChunk(str.size() + string_size);
  } else {
    CHECK(type_identifier() == kHeadersFirst);
  }
  storage_->append(str.data(), str.size());
  contents_size_ += str.size();
  return true;
}

bool HTTPValue::Flush(MessageHandler* handler) {
  return true;
}

// Encode the size of the first chunk, which is either the headers or body,
// depending on the order they are called.  Rather than trying to assume any
// particular alignment for casting between char* and int*, we just manually
// encode one byte at a time.
void HTTPValue::SetSizeOfFirstChunk(unsigned int size) {
  CHECK(!storage_->empty()) << "type encoding should already be in first byte";
  unsigned char size_buffer[4];
  size_buffer[0] = size & 0xff;
  size_buffer[1] = (size >> 8) & 0xff;
  size_buffer[2] = (size >> 16) & 0xff;
  size_buffer[3] = (size >> 24) & 0xff;
  if (storage_->size() < kStorageOverhead) {
    // Ensure the buffer is exactly 5 bytes so we can overwrite
    // bytes 1-4 (the type code in byte 0).
    storage_->append("    ", kStorageOverhead - storage_->size());
  }
  memcpy(&((*storage_)[1]), &size_buffer, sizeof(size_buffer));
}

// Decodes the size of the first chunk, which is either the headers or body,
// depending on the order they are called.  Rather than trying to assume any
// particular alignment for casting between char* and int*, we just manually
// decode one byte at a time.
unsigned int HTTPValue::SizeOfFirstChunk() const {
  CHECK(storage_->size() >= kStorageOverhead);
  const unsigned char *size_buffer =
      reinterpret_cast<const unsigned char*>(storage_->data() + 1);
  unsigned int size = size_buffer[0];
  size |= size_buffer[1] << 8;
  size |= size_buffer[2] << 16;
  size |= size_buffer[3] << 24;
  return size;
}

// Note that we avoid CHECK, and instead return false on error.  So if
// our cache gets corrupted (say) on disk, we just consider it an
// invalid entry rather than aborting the server.
bool HTTPValue::ExtractHeaders(ResponseHeaders* headers,
                               MessageHandler* handler)
    const {
  bool ret = false;
  headers->Clear();
  if (storage_->size() >= kStorageOverhead) {
    char type_id = type_identifier();
    const char* start = storage_->data() + kStorageOverhead;
    unsigned int size = SizeOfFirstChunk();
    if (size <= storage_->size() - kStorageOverhead) {
      if (type_id == kBodyFirst) {
        start += size;
        size = storage_->size() - size - kStorageOverhead;
        ret = true;
      } else {
        ret = (type_id == kHeadersFirst);
      }
      if (ret) {
        ret = headers->ReadFromBinary(StringPiece(start, size), handler);
      }
    }
  }
  return ret;
}

// Note that we avoid CHECK, and instead return false on error.  So if
// our cache gets corrupted (say) on disk, we just consider it an
// invalid entry rather than aborting the server.
bool HTTPValue::ExtractContents(StringPiece* val) const {
  bool ret = false;
  if (storage_->size() >= kStorageOverhead) {
    char type_id = type_identifier();
    const char* start = storage_->data() + kStorageOverhead;
    unsigned int size = SizeOfFirstChunk();
    if (size <= storage_->size() - kStorageOverhead) {
      if (type_id == kHeadersFirst) {
        start += size;
        size = storage_->size() - size - kStorageOverhead;
        ret = true;
      } else {
        ret = (type_id == kBodyFirst);
      }
      *val = StringPiece(start, size);
    }
  }
  return ret;
}

int64 HTTPValue::ComputeContentsSize() const {
  // Return size as 0 if the cache is corrupted.
  int64 size = 0;
  if (storage_->size() >= kStorageOverhead) {
    // Get the type id which is stored first (head or body).
    char type_id = type_identifier();
    // Get the size of the type which is stored first.
    size = SizeOfFirstChunk();
    // If the headers are stored first then update the size with storage size -
    // first chunk size.
    if ((size <= static_cast<int64>(storage_->size() - kStorageOverhead)) &&
        (type_id == kHeadersFirst)) {
      size = storage_->size() - size - kStorageOverhead;
    }
  }
  return size;
}

bool HTTPValue::Link(SharedString* src, ResponseHeaders* headers,
                     MessageHandler* handler) {
  bool ok = false;
  if ((*src)->size() >= kStorageOverhead) {
    // The simplest way to ensure that src is well formed is to save the
    // existing storage_ in a temp, assign the storage, and make sure
    // Headers and Contents return true.  The drawback is that the headers
    // parsing is arguably a little heavyweight.  We could consider encoding
    // the headers in an easier-to-extract form, so we don't have to give up
    // the integrity checks.
    SharedString temp(storage_);
    storage_ = *src;
    contents_size_ = ComputeContentsSize();

    // TODO(jmarantz): this could be a lot lighter weight, but we are going
    // to be sure at this point that both the headers and the contents are
    // valid.  It would be nice to have an HTML headers parser that didn't
    // actually create new temp copies of all the names/values.
    ok = ExtractHeaders(headers, handler);
    if (!ok) {
      storage_ = temp;
    }
  }
  return ok;
}

}  // namespace net_instaweb
