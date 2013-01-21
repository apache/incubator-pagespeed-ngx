// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_DATA_H_
#define NET_BASE_UPLOAD_DATA_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_api.h"

namespace net {

class FileStream;

// Interface implemented by callers who require callbacks when new chunks
// of data are added.
class NET_TEST ChunkCallback {
 public:
  // Invoked when a new data chunk was given for a chunked transfer upload.
  virtual void OnChunkAvailable() = 0;

 protected:
  virtual ~ChunkCallback() {}
};

class NET_API UploadData : public base::RefCounted<UploadData> {
 public:
  enum Type {
    TYPE_BYTES,
    TYPE_FILE,
    TYPE_BLOB,

    // A block of bytes to be sent in chunked encoding immediately, without
    // waiting for rest of the data.
    TYPE_CHUNK,
  };

  class NET_API Element {
   public:
    Element();
    ~Element();

    Type type() const { return type_; }
    // Explicitly sets the type of this Element. Used during IPC
    // marshalling.
    void set_type(Type type) {
      type_ = type;
    }

    const std::vector<char>& bytes() const { return bytes_; }
    const FilePath& file_path() const { return file_path_; }
    uint64 file_range_offset() const { return file_range_offset_; }
    uint64 file_range_length() const { return file_range_length_; }
    // If NULL time is returned, we do not do the check.
    const base::Time& expected_file_modification_time() const {
      return expected_file_modification_time_;
    }
    const GURL& blob_url() const { return blob_url_; }

    void SetToBytes(const char* bytes, int bytes_len) {
      type_ = TYPE_BYTES;
      bytes_.assign(bytes, bytes + bytes_len);
    }

    void SetToFilePath(const FilePath& path) {
      SetToFilePathRange(path, 0, kuint64max, base::Time());
    }

    // If expected_modification_time is NULL, we do not check for the file
    // change. Also note that the granularity for comparison is time_t, not
    // the full precision.
    void SetToFilePathRange(const FilePath& path,
                            uint64 offset, uint64 length,
                            const base::Time& expected_modification_time) {
      type_ = TYPE_FILE;
      file_path_ = path;
      file_range_offset_ = offset;
      file_range_length_ = length;
      expected_file_modification_time_ = expected_modification_time;
    }

    // TODO(jianli): UploadData should not contain any blob reference. We need
    // to define another structure to represent WebKit::WebHTTPBody.
    void SetToBlobUrl(const GURL& blob_url) {
      type_ = TYPE_BLOB;
      blob_url_ = blob_url;
    }

    // Though similar to bytes, a chunk indicates that the element is sent via
    // chunked transfer encoding and not buffered until the full upload data
    // is available.
    void SetToChunk(const char* bytes, int bytes_len, bool is_last_chunk);

    bool is_last_chunk() const { return is_last_chunk_; }
    // Sets whether this is the last chunk. Used during IPC marshalling.
    void set_is_last_chunk(bool is_last_chunk) {
      is_last_chunk_ = is_last_chunk;
    }

    // Returns the byte-length of the element.  For files that do not exist, 0
    // is returned.  This is done for consistency with Mozilla.
    // Once called, this function will always return the same value.
    uint64 GetContentLength();

    // Returns a FileStream opened for reading for this element, positioned at
    // |file_range_offset_|.  The caller gets ownership and is responsible
    // for cleaning up the FileStream. Returns NULL if this element is not of
    // type TYPE_FILE or if the file is not openable.
    FileStream* NewFileStreamForReading();

   private:
    // Allows tests to override the result of GetContentLength.
    void SetContentLength(uint64 content_length) {
      override_content_length_ = true;
      content_length_ = content_length;
    }

    Type type_;
    std::vector<char> bytes_;
    FilePath file_path_;
    uint64 file_range_offset_;
    uint64 file_range_length_;
    base::Time expected_file_modification_time_;
    GURL blob_url_;
    bool is_last_chunk_;
    bool override_content_length_;
    bool content_length_computed_;
    uint64 content_length_;
    FileStream* file_stream_;

    FRIEND_TEST_ALL_PREFIXES(UploadDataStreamTest, FileSmallerThanLength);
    FRIEND_TEST_ALL_PREFIXES(HttpNetworkTransactionTest,
                             UploadFileSmallerThanLength);
  };

  UploadData();

  void AppendBytes(const char* bytes, int bytes_len);

  void AppendFile(const FilePath& file_path);

  void AppendFileRange(const FilePath& file_path,
                       uint64 offset, uint64 length,
                       const base::Time& expected_modification_time);

  void AppendBlob(const GURL& blob_url);

  // Adds the given chunk of bytes to be sent immediately with chunked transfer
  // encoding.
  void AppendChunk(const char* bytes, int bytes_len, bool is_last_chunk);

  // Sets the callback to be invoked when a new chunk is available to upload.
  void set_chunk_callback(ChunkCallback* callback);

  // Initializes the object to send chunks of upload data over time rather
  // than all at once.
  void set_is_chunked(bool set) { is_chunked_ = set; }
  bool is_chunked() const { return is_chunked_; }

  // Returns the total size in bytes of the data to upload.
  uint64 GetContentLength();

  std::vector<Element>* elements() {
    return &elements_;
  }

  void SetElements(const std::vector<Element>& elements);

  void swap_elements(std::vector<Element>* elements) {
    elements_.swap(*elements);
  }

  // Identifies a particular upload instance, which is used by the cache to
  // formulate a cache key.  This value should be unique across browser
  // sessions.  A value of 0 is used to indicate an unspecified identifier.
  void set_identifier(int64 id) { identifier_ = id; }
  int64 identifier() const { return identifier_; }

 private:
  friend class base::RefCounted<UploadData>;

  ~UploadData();

  std::vector<Element> elements_;
  int64 identifier_;
  ChunkCallback* chunk_callback_;
  bool is_chunked_;

  DISALLOW_COPY_AND_ASSIGN(UploadData);
};

#if defined(UNIT_TEST)
inline bool operator==(const UploadData::Element& a,
                       const UploadData::Element& b) {
  if (a.type() != b.type())
    return false;
  if (a.type() == UploadData::TYPE_BYTES)
    return a.bytes() == b.bytes();
  if (a.type() == UploadData::TYPE_FILE) {
    return a.file_path() == b.file_path() &&
           a.file_range_offset() == b.file_range_offset() &&
           a.file_range_length() == b.file_range_length() &&
           a.expected_file_modification_time() ==
              b.expected_file_modification_time();
  }
  if (a.type() == UploadData::TYPE_BLOB)
    return a.blob_url() == b.blob_url();
  return false;
}

inline bool operator!=(const UploadData::Element& a,
                       const UploadData::Element& b) {
  return !(a == b);
}
#endif  // defined(UNIT_TEST)

}  // namespace net

#endif  // NET_BASE_UPLOAD_DATA_H_
