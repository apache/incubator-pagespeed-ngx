// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DNS_QUERY_H_
#define NET_BASE_DNS_QUERY_H_
#pragma once

#include <string>

#include "net/base/io_buffer.h"
#include "net/base/net_api.h"
#include "net/base/net_util.h"

namespace net{

// Represents on-the-wire DNS query message as an object.
class NET_TEST DnsQuery {
 public:
  // Constructs a query message from |dns_name| which *MUST* be in a valid
  // DNS name format, and |qtype| which must be either kDNS_A or kDNS_AAA.

  // Every generated object has a random ID, hence two objects generated
  // with the same set of constructor arguments are generally not equal;
  // there is a 1/2^16 chance of them being equal due to size of |id_|.
  DnsQuery(const std::string& dns_name, uint16 qtype, uint64 (*prng)());
  ~DnsQuery();

  // Clones |this| verbatim with ID field of the header regenerated.
  DnsQuery* CloneWithNewId() const;

  // DnsQuery field accessors.
  uint16 id() const;
  uint16 qtype() const;

  // Returns the size of the Question section of the query.  Used when
  // matching the response.
  size_t question_size() const;

  // Returns pointer to the Question section of the query.  Used when
  // matching the response.
  const char* question_data() const;

  // IOBuffer accessor to be used for writing out the query.
  IOBufferWithSize* io_buffer() const { return io_buffer_; }

 private:
  // Copy constructor to be used only by CloneWithNewId; it's not public
  // since there is a semantic difference -- this does not construct an
  // exact copy!
  DnsQuery(const DnsQuery& rhs);

  // Not implemented; just to prevent the assignment.
  void operator=(const DnsQuery&);

  // Randomizes ID field of the query message.
  void RandomizeId();

  // Size of the DNS name (*NOT* hostname) we are trying to resolve; used
  // to calculate offsets.
  size_t dns_name_size_;

  // Contains query bytes to be consumed by higher level Write() call.
  scoped_refptr<IOBufferWithSize> io_buffer_;

  // PRNG function for generating IDs.
  uint64 (*prng_)();
};

}  // namespace net

#endif  // NET_BASE_DNS_QUERY_H_
