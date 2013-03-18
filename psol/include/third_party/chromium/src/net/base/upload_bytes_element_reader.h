// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_BYTES_ELEMENT_READER_H_
#define NET_BASE_UPLOAD_BYTES_ELEMENT_READER_H_

#include "base/compiler_specific.h"
#include "net/base/upload_element_reader.h"

namespace net {

// An UploadElementReader implementation for bytes.
class NET_EXPORT_PRIVATE UploadBytesElementReader : public UploadElementReader {
 public:
  UploadBytesElementReader(const char* bytes, int bytes_length);
  virtual ~UploadBytesElementReader();

  // UploadElementReader overrides:
  virtual int Init(const CompletionCallback& callback) OVERRIDE;
  virtual int InitSync() OVERRIDE;
  virtual uint64 GetContentLength() const OVERRIDE;
  virtual uint64 BytesRemaining() const OVERRIDE;
  virtual int ReadSync(char* buf, int buf_length) OVERRIDE;
  virtual bool IsInMemory() const OVERRIDE;

 private:
  const char* bytes_;
  int bytes_length_;
  int offset_;

  DISALLOW_COPY_AND_ASSIGN(UploadBytesElementReader);
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_BYTES_ELEMENT_READER_H_
