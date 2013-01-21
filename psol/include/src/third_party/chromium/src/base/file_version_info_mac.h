// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILE_VERSION_INFO_MAC_H_
#define BASE_FILE_VERSION_INFO_MAC_H_
#pragma once

#include <string>

#include "base/file_version_info.h"
#include "base/memory/scoped_nsobject.h"

#ifdef __OBJC__
@class NSBundle;
#else
class NSBundle;
#endif

class FileVersionInfoMac : public FileVersionInfo {
 public:
  explicit FileVersionInfoMac(NSBundle *bundle);
  virtual ~FileVersionInfoMac();

  // Accessors to the different version properties.
  // Returns an empty string if the property is not found.
  virtual string16 company_name();
  virtual string16 company_short_name();
  virtual string16 product_name();
  virtual string16 product_short_name();
  virtual string16 internal_name();
  virtual string16 product_version();
  virtual string16 private_build();
  virtual string16 special_build();
  virtual string16 comments();
  virtual string16 original_filename();
  virtual string16 file_description();
  virtual string16 file_version();
  virtual string16 legal_copyright();
  virtual string16 legal_trademarks();
  virtual string16 last_change();
  virtual bool is_official_build();

 private:
  // Returns a string16 value for a property name.
  // Returns the empty string if the property does not exist.
  string16 GetString16Value(CFStringRef name);

  scoped_nsobject<NSBundle> bundle_;

  DISALLOW_COPY_AND_ASSIGN(FileVersionInfoMac);
};

#endif  // BASE_FILE_VERSION_INFO_MAC_H_
