// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_FILE_ELEMENT_READER_H_
#define NET_BASE_UPLOAD_FILE_ELEMENT_READER_H_

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "net/base/upload_element_reader.h"

namespace net {

class FileStream;

// An UploadElementReader implementation for file.
class NET_EXPORT_PRIVATE UploadFileElementReader : public UploadElementReader {
 public:
  UploadFileElementReader(const FilePath& path,
                          uint64 range_offset,
                          uint64 range_length,
                          const base::Time& expected_modification_time);
  virtual ~UploadFileElementReader();

  // UploadElementReader overrides:
  virtual int Init(const CompletionCallback& callback) OVERRIDE;
  virtual int InitSync() OVERRIDE;
  virtual uint64 GetContentLength() const OVERRIDE;
  virtual uint64 BytesRemaining() const OVERRIDE;
  virtual int ReadSync(char* buf, int buf_length) OVERRIDE;

 private:
  // This method is used to implement Init().
  void OnInitCompleted(scoped_ptr<FileStream>* file_stream,
                       uint64* content_length,
                       int* result,
                       const CompletionCallback& callback);

  // Sets an value to override the result for GetContentLength().
  // Used for tests.
  struct NET_EXPORT_PRIVATE ScopedOverridingContentLengthForTests {
    ScopedOverridingContentLengthForTests(uint64 value);
    ~ScopedOverridingContentLengthForTests();
  };

  FilePath path_;
  uint64 range_offset_;
  uint64 range_length_;
  base::Time expected_modification_time_;
  scoped_ptr<FileStream> file_stream_;
  uint64 content_length_;
  uint64 bytes_remaining_;
  base::WeakPtrFactory<UploadFileElementReader> weak_ptr_factory_;

  FRIEND_TEST_ALL_PREFIXES(UploadDataStreamTest, FileSmallerThanLength);
  FRIEND_TEST_ALL_PREFIXES(HttpNetworkTransactionTest,
                           UploadFileSmallerThanLength);
  FRIEND_TEST_ALL_PREFIXES(HttpNetworkTransactionSpdy2Test,
                           UploadFileSmallerThanLength);
  FRIEND_TEST_ALL_PREFIXES(HttpNetworkTransactionSpdy3Test,
                           UploadFileSmallerThanLength);

  DISALLOW_COPY_AND_ASSIGN(UploadFileElementReader);
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_FILE_ELEMENT_READER_H_
