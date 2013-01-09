// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILE_VERSION_INFO_WIN_H_
#define BASE_FILE_VERSION_INFO_WIN_H_
#pragma once

#include <string>

#include "base/base_api.h"
#include "base/basictypes.h"
#include "base/file_version_info.h"
#include "base/memory/scoped_ptr.h"

struct tagVS_FIXEDFILEINFO;
typedef tagVS_FIXEDFILEINFO VS_FIXEDFILEINFO;

class FileVersionInfoWin : public FileVersionInfo {
 public:
  BASE_API FileVersionInfoWin(void* data, int language, int code_page);
  BASE_API ~FileVersionInfoWin();

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

  // Lets you access other properties not covered above.
  BASE_API bool GetValue(const wchar_t* name, std::wstring* value);

  // Similar to GetValue but returns a wstring (empty string if the property
  // does not exist).
  BASE_API std::wstring GetStringValue(const wchar_t* name);

  // Get the fixed file info if it exists. Otherwise NULL
  VS_FIXEDFILEINFO* fixed_file_info() { return fixed_file_info_; }

 private:
  scoped_ptr_malloc<char> data_;
  int language_;
  int code_page_;
  // This is a pointer into the data_ if it exists. Otherwise NULL.
  VS_FIXEDFILEINFO* fixed_file_info_;

  DISALLOW_COPY_AND_ASSIGN(FileVersionInfoWin);
};

#endif  // BASE_FILE_VERSION_INFO_WIN_H_
