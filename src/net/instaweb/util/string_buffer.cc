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

#include "net/instaweb/util/public/string_buffer.h"

#include <algorithm>  // for std::min
#include "base/logging.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

// Our strategy is to reduce memory fragmentation when accumulating
// large amounts of resource content by appending new bytes into
// new strings in a vector, rather than resizing the existing strings.
//
// Consider a multi-megabyte image.  We'll read it into our system,
// most likely, via a low-level call to read() with a buffer that's
// on the order of 10k.  (see net/instaweb/util/stack_buffer.h).
// As we accumulate 100 of these, we don't want to keep appending
// them onto one std::string, requiring megabytes of contiguous
// memory.
//
// However, if a caller is accumulating a few bytes at a time, it
// seems likely that the vector would grow very large, so let's
// grow each element until it hits about 1k.  1000 is picked
// rather than 1024 to leave room below a power-of-two for malloc
// overhead.
//
// Another option would be to have the strings be the same size.
// The advantage of that would be random-access and some code
// simplifications.  The disadvantage is that small StringBuffers
// would take more memory.  Another idea would be to have all but
// the back() string* be the same size.
size_t StringBuffer::kMinStringSize = 1000;
size_t StringBuffer::kReadBufferSize = 32000;
size_t StringBuffer::npos = std::string::npos;

void StringBuffer::Clear() {
  for (int i = 0, n = strings_.size(); i < n; ++i) {
    delete strings_[i];
  }
  strings_.resize(0);
  size_ = 0;
}

void StringBuffer::Append(const StringPiece& string_piece) {
  // If the last string we appended is a reasonable size,
  // or this is a new buffer, then append a new string.
  // TODO(jmarantz): this behavior should be observed and tweaked.
  size_t piece_size = string_piece.size();
  size_t back_size = strings_.empty() ? 0 : strings_.back()->size();
  if (strings_.empty() ||
      (back_size >= kMinStringSize) ||
      (piece_size >= kMinStringSize)) {
    strings_.push_back(new std::string(string_piece.data(), piece_size));
  } else {
    strings_.back()->append(string_piece.data(), piece_size);
  }
  size_ += piece_size;
}

bool StringBuffer::Write(Writer* writer, MessageHandler* message_handler)
    const {
  bool ret = true;
  for (int i = 0, n = strings_.size(); ret && (i < n); ++i) {
    ret = writer->Write(*strings_[i], message_handler);
  }
  return ret;
}

std::string StringBuffer::ToString() const {
  std::string buffer;
  buffer.reserve(size_);
  for (int i = 0, n = strings_.size(); i < n; ++i) {
    buffer.append(*strings_[i]);
  }
  return buffer;
}

bool StringBuffer::operator==(const StringBuffer& that) const {
  if (size_ != that.size_) {
    return false;
  }
  size_t this_vector_index = 0, this_char_index = 0;
  size_t that_vector_index = 0, that_char_index = 0;
  size_t compare_size = 0;
  for (int char_index = 0; char_index < size_; char_index += compare_size) {
    const std::string& this_string = *strings_[this_vector_index];
    const std::string& that_string = *that.strings_[that_vector_index];
    int this_remaining = this_string.size() - this_char_index;
    int that_remaining = that_string.size() - that_char_index;
    compare_size = std::min(this_remaining, that_remaining);
    StringPiece this_piece(this_string.data() + this_char_index, compare_size);
    StringPiece that_piece(that_string.data() + that_char_index, compare_size);
    if (this_piece != that_piece) {
      return false;
    }
    this_char_index += compare_size;
    that_char_index += compare_size;
    if (this_char_index == this_string.size()) {
      this_char_index = 0;
      ++this_vector_index;
    }
    if (that_char_index == that_string.size()) {
      that_char_index = 0;
      ++that_vector_index;
    }
  }
  return true;
}

void StringBuffer::CopyFrom(const StringBuffer& src) {
  Clear();
  for (int i = 0, n = src.strings_.size(); i < n; ++i) {
    strings_.push_back(new std::string(*src.strings_[i]));
  }
  size_ = src.size_;
}

char* StringBuffer::AllocReadBuffer() {
  std::string* buffer = new std::string;
  buffer->resize(kReadBufferSize);
  strings_.push_back(buffer);

  // buffer->data() is a const method, and may return a
  // pointer to shared storage, however, taking the address
  // of a the first character cannot:  the string implementation
  // must assume that the returned non-const char*& will be written.
  return &((*buffer)[0]);
}

void StringBuffer::CommitReadBuffer(char* read_buffer, int size) {
  std::string* buffer = strings_.back();
  CHECK(buffer->data() == read_buffer);  // pointer comparison
  CHECK(size <= static_cast<int>(kReadBufferSize));
  buffer->resize(size);
  size_ += size;
}

void StringBuffer::AbandonReadBuffer(char* read_buffer) {
  std::string* buffer = strings_.back();
  CHECK(buffer->data() == read_buffer);  // pointer comparison
  delete buffer;
  strings_.pop_back();
}

std::string StringBuffer::SubString(size_t pos, size_t size) const {
  CHECK(pos <= static_cast<size_t>(size_));
  if ((size == npos) || (size + pos > static_cast<size_t>(size_))) {
    size = size_ - pos;
  }
  std::string ret;
  ret.reserve(size);

  // TODO(jmarantz): could binary_search here if we want this to go faster,
  // or switch to fixed size buffers, which will also make file-reading faster.
  size_t char_index = 0;
  for (size_t i = 0; (i < strings_.size()) && (size > 0); ++i) {
    std::string* str = strings_[i];
    if ((pos >= char_index) && (pos < (char_index + str->size()))) {
      size_t pos_in_string = pos - char_index;
      size_t copy_size = std::min(size, str->size() - pos_in_string);
      ret.append(str->data() + pos_in_string, copy_size);
      pos += copy_size;
      size -= copy_size;
    }
    char_index += str->size();
  }
  return ret;
}

}  // namespace net_instaweb
