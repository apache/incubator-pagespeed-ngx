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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SHARED_DYNAMIC_STRING_MAP_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SHARED_DYNAMIC_STRING_MAP_H_

#include <cstddef>
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;
class Writer;

struct Entry {
  int value;
  size_t string_offset;
};

// A shared memory string to int dictionary/map, no deletion.
// Currently the map is designed to fill with number_of_strings strings of
// average length average_string_length.  Once the map is full it ignores
// attempts to add additional information.
// TODO(jhoch): make map dynamically sized
class SharedDynamicStringMap {
 public:
  // Number of strings will be rounded up to a power of 2.
  // Average string length should include terminating null character.
  // Map will be able to hold exactly number_of_strings * average_string_length
  // chars worth of string data.
  SharedDynamicStringMap(size_t number_of_strings,
                         size_t average_string_length,
                         AbstractSharedMem* shm_runtime,
                         const GoogleString& filename_prefix);

  // Initialize the shared memory segment.  This method should complete before
  // any other methods are executed.
  // parent = true means invoked in root process, initialize the shared memory
  //        = false means invoked in child process -- attach to existing segment
  bool InitSegment(bool parent, MessageHandler* message_handler);

  // Increments value corresponding to given string by 1.
  // Adds the string to the map with initial value 1 if the string is not
  // present.
  // Returns the new value corresponding to the element.
  // If the map is full it does nothing and returns 0.
  int IncrementElement(const StringPiece& string);

  // Retrieve the value corresponding to the string (returns 0 if the string is
  // not in the map)
  int LookupElement(const StringPiece& string) const;

  // Dumps table's strings into StringSet
  void GetKeys(StringSet* strings);

  // Retrieve the number of strings inserted into the table.
  int GetNumberInserted() const;

  // Destroy shared memory segment and other relevant clean-up
  void GlobalCleanup(MessageHandler* message_handler);

  // Iterates through the string data that is present at the time of calling
  // and dumps out each string with its associated value.  The value produced
  // for a given string is going to be the value present whenever that string
  // is dumped.
  void Dump(Writer* writer, MessageHandler* message_handler);

 private:
  void ClearSegment(MessageHandler* message_handler);

  // ***If lock = true, locks the mutex associated with the returned entry***
  //    (to be unlocked by caller)
  // Finds the index of the entry with the given string, or the first empty
  // entry encountered, which can either be used for insertion purposes or
  // to know that the string is not present, since the table does not support
  // deletion.
  //   -1 is returned if the entire table is traversed without finding the
  // string or an empty spot (this should not happen if the table never exceeds
  // 50% capacity).
  //   entry_pointer is an output parameter.
  //   Note that lock must always be set to true for any writing operation, and
  // setting write to false for a read operation can result in a false read,
  // albeit in very rare circumstances.  The circumstance is that an entry "AB"
  // is being added to the table, where A and B are arbitrary strings, and a
  // read of "A" is occurring, and the read of "A" catches the write of "AB"
  // mid-write at exactly the moment where it can only see "A."
  //   The fact that looking up without locking is possible relies on the
  // assumption that entries are not deleted and that null characters fill up
  // the char space upon initialization.
  //   If 100% accurate lookup is needed then a new LookupElement method could
  // be added that calls FindEntry(lock = true).
  int FindEntry(const StringPiece& string,
                bool lock,
                Entry** entry_pointer) const;
  Entry* GetEntry(size_t n) const;
  Entry* GetFirstEntry() const;

  // Gets the mutex for the nth table entry.
  AbstractMutex* GetMutex(size_t n) const;
  // Inserts the given string into the table (if there is room), by adding it
  // to char storage and setting the entry_pointer's char offset and value (the
  // former to string_offset and the latter to 1), and returns the resulting
  // value of the entry (1 if it was successfully inserted, 0 otherwise)
  //   The entry should be locked when this method is called.
  int InsertString(const StringPiece& string, Entry* entry_pointer);
  char* GetStringAtOffset(size_t offset) const;

  // Math utility function, returns the smallest power of two greater than or
  // equal to the input.
  static size_t NextPowerOfTwo(size_t n);

  //                               |
  // Structure content             | Offset
  //                               |
  //  ___________________________  | mutex_offset_ = 0
  // | Mutex0                    | |  - memory location = segment_->Base()
  // |                           | |
  // | Mutex1                    | | mutex_offset_ + mutex_size
  // |                           | |
  // | Mutex2                    | | mutex_offset_ + mutex_size_ * 2
  // | .                         | |
  // | .                         | |   - each mutex has size mutex_size
  // | .                         | |
  // |                           | |
  // | MutexN                    | | mutex_offset_ + mutex_size_ * N
  // | .                         | |
  // | .                         | |   - there are table_size_ + 1 mutexes
  // | .                         | |       (last mutex is for string_offset
  // | .                         | |        and number_inserted)
  // | .                         | |
  // |___________________________| | strings_offset_ = mutex_offset_ +
  // | String0======= |            |     kTableSize * mutex_size_
  // |                |            |   = String offset0
  // |                |            |
  // | String1======= |            | String offset1
  // |           _____|            |
  // |          |                  |
  // | String2= |                  | String offset2
  // |          |________________  |
  // |                           | |
  // | String3================== | | etc.
  // |                  _________| |
  // |                 |           |
  // | String4======== |           |   - strings are variable length, null
  // |                 |_          |       terminated
  // |                   |         |
  // | String5========== |         |   - total allocated space is
  // |                  _|         |       number_of_strings times average
  // |                 |           |       string length
  // | String6======== |           |
  // |                 |_          |   - there are as many strings as have been
  // | .                 |         |       added
  // | .                 |         |
  // | .                 |         |   - location at which to add next string is
  // |___________________|         |       stored at string_offset_offset_
  // |                 |           |       (see below)
  // |                 |           |
  // |  String offset  |           | string_offset_offset_ = strings_offset_ +
  // |_________________|           |     number_of_strings_ *
  // |        |                    |     average_string_length_
  // | Number |                    |
  // |  Inse- |                    | number_inserted_offset_ =
  // |   rted |                    |     string_offset_offset_ + kOffsetSize
  // |        |                    |
  // |________|________________    | table_offset_ = number_inserted_offset_ +
  // | Value0 | String offset0 |   |     kIntSize
  // |  (int) |  (size_t)      |   |
  // |        |                |   |
  // | Value1 | String offset1 |   | table_offset_ + kEntrySize
  // |        |                |   |
  // | Value2 | String offset2 |   | table_offset_ + kEntrySize* 2
  // | .      | .              |   |
  // | .      | .              |   |   - each value and string offset makes an
  // | .      | .              |   |       Entry struct
  // |        |                |   |
  // | ValueN | String offsetN |   | table_offset_ + kEntrySize * N
  // | .      | .              |   |
  // | .      | .              |   |   - there are table_size entries
  // | .      | .              |   |
  // |________|________________|   |

  size_t number_of_strings_;
  size_t average_string_length_;
  // Sizes of various portions of the memory
  size_t mutex_size_;
  size_t table_size_;
  // Offset from segment_->Base() at which various portions of the structure
  // begin (see above depiction).
  //   mutex_offset_ is the beginning of the (table_size_ + 1) mutexes
  //   strings_offset_ is the beginning of the strings
  //   string_offset_offset_ is where the offset of the next string to be
  //     inserted is located
  //   number_inserted_offset_ is where the number of inserted strings is
  //     located
  //   table_offset_ is the beginning of the table_size_ entries
  size_t mutex_offset_;
  size_t strings_offset_;
  size_t string_offset_offset_;
  size_t number_inserted_offset_;
  size_t table_offset_;
  // Total size of shared memory segment
  size_t total_size_;

  // The mutex for inserting strings, i.e. the one shared by the
  // string_offset_ and number_inserted_ values.
  scoped_ptr<AbstractMutex> insert_string_mutex_;

  const GoogleString segment_name_;
  AbstractSharedMem* shm_runtime_;
  scoped_ptr<AbstractSharedMemSegment> segment_;

  DISALLOW_COPY_AND_ASSIGN(SharedDynamicStringMap);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SHARED_DYNAMIC_STRING_MAP_H_
