// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_MOCK_FILTER_CONTEXT_H_
#define NET_BASE_MOCK_FILTER_CONTEXT_H_
#pragma once

#include <string>

#include "googleurl/src/gurl.h"
#include "net/base/filter.h"

namespace net {

class MockFilterContext : public FilterContext {
 public:
  MockFilterContext();
  virtual ~MockFilterContext();

  void SetMimeType(const std::string& mime_type) { mime_type_ = mime_type; }
  void SetURL(const GURL& gurl) { gurl_ = gurl; }
  void SetRequestTime(const base::Time time) { request_time_ = time; }
  void SetCached(bool is_cached) { is_cached_content_ = is_cached; }
  void SetDownload(bool is_download) { is_download_ = is_download; }
  void SetResponseCode(int response_code) { response_code_ = response_code; }
  void SetSdchResponse(bool is_sdch_response) {
    is_sdch_response_ = is_sdch_response;
  }

  virtual bool GetMimeType(std::string* mime_type) const;

  // What URL was used to access this data?
  // Return false if gurl is not present.
  virtual bool GetURL(GURL* gurl) const;

  // What was this data requested from a server?
  virtual base::Time GetRequestTime() const;

  // Is data supplied from cache, or fresh across the net?
  virtual bool IsCachedContent() const;

  // Is this a download?
  virtual bool IsDownload() const;

  // Was this data flagged as a response to a request with an SDCH dictionary?
  virtual bool IsSdchResponse() const;

  // How many bytes were fed to filter(s) so far?
  virtual int64 GetByteReadCount() const;

  virtual int GetResponseCode() const;

  virtual void RecordPacketStats(StatisticSelector statistic) const {}

 private:
  int buffer_size_;
  std::string mime_type_;
  GURL gurl_;
  base::Time request_time_;
  bool is_cached_content_;
  bool is_download_;
  bool is_sdch_response_;
  int response_code_;

  DISALLOW_COPY_AND_ASSIGN(MockFilterContext);
};

}  // namespace net

#endif  // NET_BASE_MOCK_FILTER_CONTEXT_H_
