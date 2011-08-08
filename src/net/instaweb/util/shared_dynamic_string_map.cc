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

// Author: jhoch@google.com (Jason Hoch)

#include "net/instaweb/util/public/shared_dynamic_string_map.h"

#include "base/logging.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/rolling_hash.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

namespace {

const int kTableFactor = 2;
const char kSharedDynamicStringMapSegmentName[] = "SharedDynamicStringMap";
const size_t kPointerSize = sizeof(char*); // NOLINT
const size_t kOffsetSize = sizeof(size_t); // NOLINT
const size_t kIntSize = sizeof(int); // NOLINT
const size_t kEntrySize = sizeof(Entry); // NOLINT

}  // namespace

SharedDynamicStringMap::SharedDynamicStringMap(
    size_t number_of_strings,
    size_t average_string_length,
    AbstractSharedMem* shm_runtime,
    const GoogleString& filename_prefix)
    : number_of_strings_(NextPowerOfTwo(number_of_strings)),
      average_string_length_(average_string_length),
      segment_name_(StrCat(filename_prefix,
                           kSharedDynamicStringMapSegmentName)),
      shm_runtime_(shm_runtime) {
  // Check to make sure number_of_strings_ is a power of 2.
  DCHECK_EQ(static_cast<size_t>(0),
            number_of_strings_ & (number_of_strings_ - 1));
  mutex_size_ = shm_runtime_->SharedMutexSize();
  table_size_ = number_of_strings_ * kTableFactor;
  // (See drawing in header file for further explanation)
  // First we have (table_size_ + 1) mutexes.
  // Then we have the strings, which are inserted one at a time and are
  //   of variable size.
  // Then we have the string_offset of the next string to be inserted.
  // Then we have the number of strings inserted.
  // Finally we have the table_size_ entries that contain values and string
  //   offsets.
  mutex_offset_ = 0;
  strings_offset_ = mutex_size_ * (table_size_ + 1);
  string_offset_offset_ =
      strings_offset_ + number_of_strings * average_string_length_;
  number_inserted_offset_ = string_offset_offset_ + kOffsetSize;
  table_offset_ = number_inserted_offset_ + kIntSize;
  total_size_ = table_offset_ + table_size_ * kEntrySize;
}

bool SharedDynamicStringMap::InitSegment(bool parent,
                                         MessageHandler* message_handler) {
  // Pointer returned by segment_->Base() is a char*
  // Table contains a pointer and an int for each entry.
  bool ok = true;
  if (parent) {
    // Initialize shared memory
    segment_.reset(shm_runtime_->CreateSegment(segment_name_,
                                               total_size_,
                                               message_handler));
    if (segment_.get() == NULL) {
      ok = false;
    } else {
      // Initialize mutexes - there is an extra mutex, the last one, shared
      // by the string_offset and number_inserted values known as the "insert
      // string mutex"
      for (int i = 0; i < static_cast<int>(table_size_) + 1; i++) {
        if (!segment_->InitializeSharedMutex(i * mutex_size_,
                                                 message_handler)) {
          ok = false;
          break;
        }
      }
    }
  } else {
    // In child process -> attach to existing segment
    segment_.reset(shm_runtime_->AttachToSegment(segment_name_,
                                                 total_size_,
                                                 message_handler));
    if (segment_.get() == NULL) {
      ok = false;
    }
  }
  if (ok) {
    insert_string_mutex_.reset(GetMutex(table_size_));
  } else {
    ClearSegment(message_handler);
  }
  return ok;
}

void SharedDynamicStringMap::ClearSegment(MessageHandler* message_handler) {
  segment_.reset(NULL);
  shm_runtime_->DestroySegment(segment_name_, message_handler);
}

int SharedDynamicStringMap::IncrementElement(const StringPiece& string) {
  if (segment_.get() == NULL) {
    return 0;
  }
  Entry* entry_pointer = 0;
  // We need to lock the entry for incrementation
  int entry = FindEntry(string, true, &entry_pointer);
  int value;
  // If entry is -1 then the table is full.
  if (entry == -1) {
    value = 0;
  } else {
    // The mutex for the entry is locked by FindEntry for continued use
    scoped_ptr<AbstractMutex> mutex(GetMutex(entry));
    if (entry_pointer->value == 0) {
      // The string is not yet in the table.
      value = InsertString(string, entry_pointer);
    } else {
      // The string is already in the table.
      value = ++(entry_pointer->value);
    }
    mutex->Unlock();
  }
  return value;
}

int SharedDynamicStringMap::LookupElement(const StringPiece& string) const {
  if (segment_.get() == NULL) {
    return 0;
  }
  Entry* entry_pointer = 0;
  // We don't need to lock the entry for lookup
  int entry = FindEntry(string, false, &entry_pointer);
  // If entry is -1 then the table is full.
  return (entry == -1) ? 0 : entry_pointer->value;
}

int SharedDynamicStringMap::FindEntry(const StringPiece& string,
                                      bool lock,
                                      Entry** entry_pointer_pointer) const {
  // lock should always be set to true for writes, and having lock set to false
  // for a read can occasionally result in a mistake - see header file.
  uint64 hash = RollingHash(string.data(), 0, string.size());
  // First 32 bits
  uint32 hash1 = hash >> 32;
  // Second 32 bits
  uint32 hash2 = hash;
  // For now we assume table_size_ is a power of two, so having hash2 be odd
  // makes sure that our secondary hashing cycles through all table entries
  hash2 |= 1;
  // hash1 dictates starting entry
  size_t entry = hash1 % table_size_;
  size_t starting_entry = entry;
  scoped_ptr<AbstractMutex> mutex;
  do {
    // Lock this entry
    mutex.reset(GetMutex(entry));
    if (lock) {
      mutex->Lock();
    }
    // Table contains value and then pointer to char
    *entry_pointer_pointer = GetEntry(entry);
    char* entry_string_data =
        GetStringAtOffset((*entry_pointer_pointer)->string_offset);
    if ((*entry_pointer_pointer)->value == 0) {
      // If the value is 0 the string is not yet in the table and/or this is the
      // place to insert it.
      return entry;
    } else if (strcmp(entry_string_data, string.data()) == 0) {
      // We've found the string
      return entry;
    } else {
      // Use secondary hashing to proceed to the next entry
      entry += hash2;
      // Same as entry %= table_size_ since table_size_ is a power of 2.
      entry &= (table_size_ - 1);
    }
    if (lock) {
      mutex->Unlock();
    }
  } while (entry != starting_entry);
  // If the above condition fails, the table is full.
  return -1;
}

Entry* SharedDynamicStringMap::GetEntry(size_t n) const {
  Entry* first_entry = SharedDynamicStringMap::GetFirstEntry();
  return first_entry + n;
}

Entry* SharedDynamicStringMap::GetFirstEntry() const {
  return reinterpret_cast<Entry*>(
      const_cast<char*>(segment_->Base() + table_offset_));
}

AbstractMutex* SharedDynamicStringMap::GetMutex(size_t n) const {
  return segment_->AttachToSharedMutex(mutex_offset_ + n * mutex_size_);
}

int SharedDynamicStringMap::InsertString(const StringPiece& string,
                                         Entry* entry_pointer) {
  // We store the offset of the next string to be inserted at
  // string_offset_offset_, which has an associated mutex
  ScopedMutex mutex(insert_string_mutex_.get());
  size_t* string_offset = reinterpret_cast<size_t*>(
      const_cast<char*>(segment_->Base() + string_offset_offset_));
  size_t size = string.size();
  // If we can't insert the string then we return 0;
  if (strings_offset_ + *string_offset + size >= table_offset_) {
    return 0;
  }
  char* inserted_string = GetStringAtOffset(*string_offset);
  memcpy(inserted_string, string.data(), size);
  // After copying the string we add a terminating null character
  *(inserted_string + size) = '\0';
  // Store the offset in the entry
  entry_pointer->string_offset = *string_offset;
  // +1 for the terminating null character
  *string_offset += size + 1;
  // Increment the number inserted
  int* number_inserted = reinterpret_cast<int*>(
      const_cast<char*>(segment_->Base() + number_inserted_offset_));
  ++(*number_inserted);
  // Finally, increment the entry value
  return ++(entry_pointer->value);
}

char* SharedDynamicStringMap::GetStringAtOffset(size_t offset) const {
  return const_cast<char*>(segment_->Base() + strings_offset_ + offset);
}

void SharedDynamicStringMap::GetKeys(StringSet* strings) {
  int number_inserted = GetNumberInserted();
  char* string = const_cast<char*>(segment_->Base() + strings_offset_);
  for (int i = 0; i < number_inserted; i++) {
    strings->insert(string);
    int size = strlen(string);
    string += size + 1;
  }
}

int SharedDynamicStringMap::GetNumberInserted() const {
  return *(reinterpret_cast<int*>(
      const_cast<char*> (segment_->Base() + number_inserted_offset_)));
}

void SharedDynamicStringMap::GlobalCleanup(MessageHandler* message_handler) {
  if (segment_.get() != NULL) {
    shm_runtime_->DestroySegment(segment_name_, message_handler);
  }
}

void SharedDynamicStringMap::Dump(Writer* writer,
                                  MessageHandler* message_handler) {
  int number_inserted = GetNumberInserted();
  char* string = const_cast<char*>(segment_->Base() + strings_offset_);
  for (int i = 0; i < number_inserted; i++) {
    int value = LookupElement(string);
    writer->Write(string, message_handler);
    writer->Write(": ", message_handler);
    writer->Write(IntegerToString(value), message_handler);
    writer->Write("\n", message_handler);
    int size = strlen(string);
    string += size + 1;
  }
}

size_t SharedDynamicStringMap::NextPowerOfTwo(size_t n) {
  if (n == 0) {
    return 1;
  }
  n--;
  for (int shift = 1; shift < static_cast<int>(kOffsetSize); shift *= 2)
    n |= n >> shift;
  return n + 1;
}

}  // namespace net_instaweb
