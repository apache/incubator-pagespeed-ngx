// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ADDRESS_LIST_H_
#define NET_BASE_ADDRESS_LIST_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "net/base/net_api.h"
#include "net/base/net_util.h"

struct addrinfo;

namespace net {

// An AddressList object contains a linked list of addrinfo structures.  This
// class is designed to be copied around by value.
class NET_API AddressList {
 public:
  // Constructs an invalid address list. Should not call any methods on this
  // other than assignment.
  AddressList();

  AddressList(const AddressList& addresslist);
  ~AddressList();
  AddressList& operator=(const AddressList& addresslist);

  // Creates an address list for a list of IP literals.
  static AddressList CreateFromIPAddressList(
      const std::vector<IPAddressNumber>& addresses,
      uint16 port);

  // Creates an address list for a single IP literal.
  static AddressList CreateFromIPAddress(
      const IPAddressNumber& address,
      uint16 port);

  // Creates an address list for a single IP literal.  If
  // |canonicalize_name| is true, fill the ai_canonname field with the
  // canonicalized IP address.
  static AddressList CreateFromIPAddressWithCname(
      const IPAddressNumber& address,
      uint16 port,
      bool canonicalize_name);

  // Adopts the given addrinfo list (assumed to have been created by
  // the system, e.g. returned by getaddrinfo()) in place of the
  // existing one if any.  This hands over responsibility for freeing
  // the addrinfo list to the AddressList object.
  static AddressList CreateByAdoptingFromSystem(struct addrinfo* head);

  // Creates a new address list with a copy of |head|. This includes the
  // entire linked list.
  static AddressList CreateByCopying(const struct addrinfo* head);

  // Creates a new address list wich has a single address, |head|. If there
  // are other addresses in |head| they will be ignored.
  static AddressList CreateByCopyingFirstAddress(const struct addrinfo* head);

  // Creates an address list for a single socket address.
  // |address| the sockaddr to copy.
  // |socket_type| is either SOCK_STREAM or SOCK_DGRAM.
  // |protocol| is either IPPROTO_TCP or IPPROTO_UDP.
  static AddressList CreateFromSockaddr(
      const struct sockaddr* address,
      socklen_t address_length,
      int socket_type,
      int protocol);

  // Appends a copy of |head| and all its linked addrinfos to the stored
  // addrinfo. Note that this will cause a reallocation of the linked list,
  // which invalidates the head pointer.
  void Append(const struct addrinfo* head);

  // Sets the port of all addresses in the list to |port| (that is the
  // sin[6]_port field for the sockaddrs). Note that this will cause a
  // reallocation of the linked list, which invalidates the head pointer.
  void SetPort(uint16 port);

  // Retrieves the port number of the first sockaddr in the list. (If SetPort()
  // was previously used on this list, then all the addresses will have this
  // same port number.)
  uint16 GetPort() const;

  // Gets the canonical name for the address.
  // If the canonical name exists, |*canonical_name| is filled in with the
  // value and true is returned. If it does not exist, |*canonical_name| is
  // not altered and false is returned.
  // |canonical_name| must be a non-null value.
  bool GetCanonicalName(std::string* canonical_name) const;

  // Gets access to the head of the addrinfo list.
  //
  // IMPORTANT: Callers SHOULD NOT mutate the addrinfo chain, since under the
  //            hood this data might be shared by other AddressLists, which
  //            might even be running on other threads.
  //
  //            Additionally, non-const methods like SetPort() and Append() can
  //            cause the head to be reallocated, so do not cache the return
  //            value of head() across such calls.
  const struct addrinfo* head() const;

 private:
  struct Data;

  explicit AddressList(Data* data);

  scoped_refptr<Data> data_;
};

}  // namespace net

#endif  // NET_BASE_ADDRESS_LIST_H_
