// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TRANSPORT_SECURITY_STATE_H_
#define NET_BASE_TRANSPORT_SECURITY_STATE_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "net/base/net_api.h"
#include "net/base/x509_cert_types.h"

namespace net {

// TransportSecurityState
//
// Tracks which hosts have enabled *-Transport-Security. This object manages
// the in-memory store. A separate object must register itself with this object
// in order to persist the state to disk.
class NET_API TransportSecurityState :
    public base::RefCountedThreadSafe<TransportSecurityState> {
 public:
  // If non-empty, |hsts_hosts| is a JSON-formatted string to treat as if it
  // were a built-in entry (same format as persisted metadata in the
  // TransportSecurityState file).
  explicit TransportSecurityState(const std::string& hsts_hosts);

  // A DomainState is the information that we persist about a given domain.
  struct NET_API DomainState {
    enum Mode {
      // Strict mode implies:
      //   * We generate internal redirects from HTTP -> HTTPS.
      //   * Certificate issues are fatal.
      MODE_STRICT = 0,
      // Opportunistic mode implies:
      //   * We'll request HTTP URLs over HTTPS
      //   * Certificate issues are ignored.
      MODE_OPPORTUNISTIC = 1,
      // SPDY_ONLY (aka X-Bodge-Transport-Security) is a hopefully temporary
      // measure. It implies:
      //   * We'll request HTTP URLs over HTTPS iff we have SPDY support.
      //   * Certificate issues are fatal.
      MODE_SPDY_ONLY = 2,
      // None means there is no HSTS for this domain.
      MODE_NONE = 3,
    };

    DomainState();
    ~DomainState();

    // IsChainOfPublicKeysPermitted takes a set of public key hashes and
    // returns true if:
    //   1) |public_key_hashes| is empty, i.e. no public keys have been pinned.
    //   2) |hashes| and |public_key_hashes| are not disjoint.
    bool IsChainOfPublicKeysPermitted(
        const std::vector<SHA1Fingerprint>& hashes);

    Mode mode;
    base::Time created;  // when this host entry was first created
    base::Time expiry;  // the absolute time (UTC) when this record expires
    bool include_subdomains;  // subdomains included?
    std::vector<SHA1Fingerprint> public_key_hashes;  // optional; permitted keys

    // The follow members are not valid when stored in |enabled_hosts_|.
    bool preloaded;  // is this a preloaded entry?
    std::string domain;  // the domain which matched
  };

  // Enable TransportSecurity for |host|.
  void EnableHost(const std::string& host, const DomainState& state);

  // Delete any entry for |host|. If |host| doesn't have an exact entry then no
  // action is taken. Returns true iff an entry was deleted.
  bool DeleteHost(const std::string& host);

  // Returns true if |host| has TransportSecurity enabled, in the context of
  // |sni_available|. In that case, *result is filled out.
  // Note that *result is always overwritten on every call.
  bool IsEnabledForHost(DomainState* result,
                        const std::string& host,
                        bool sni_available);

  // Returns true if |host| has any SSL certificate pinning, in the context of
  // |sni_available|. In that case, *result is filled out.
  // Note that *result is always overwritten on every call.
  bool HasPinsForHost(DomainState* result,
                      const std::string& host,
                      bool sni_available);

  // Returns true if |host| has any HSTS metadata, in the context of
  // |sni_available|. (This include cert-pin-only metadata).
  // In that case, *result is filled out.
  // Note that *result is always overwritten on every call.
  bool HasMetadata(DomainState* result,
                   const std::string& host,
                   bool sni_available);

  // Deletes all records created since a given time.
  void DeleteSince(const base::Time& time);

  // Returns |true| if |value| parses as a valid *-Transport-Security
  // header value.  The values of max-age and and includeSubDomains are
  // returned in |max_age| and |include_subdomains|, respectively.  The out
  // parameters are not modified if the function returns |false|.
  static bool ParseHeader(const std::string& value,
                          int* max_age,
                          bool* include_subdomains);

  class Delegate {
   public:
    // This function may not block and may be called with internal locks held.
    // Thus it must not reenter the TransportSecurityState object.
    virtual void StateIsDirty(TransportSecurityState* state) = 0;

   protected:
    virtual ~Delegate() {}
  };

  void SetDelegate(Delegate*);

  bool Serialise(std::string* output);
  // Existing non-preloaded entries are cleared and repopulated from the
  // passed JSON string.
  bool LoadEntries(const std::string& state, bool* dirty);

  // The maximum number of seconds for which we'll cache an HSTS request.
  static const long int kMaxHSTSAgeSecs;

 private:
  friend class base::RefCountedThreadSafe<TransportSecurityState>;
  FRIEND_TEST_ALL_PREFIXES(TransportSecurityStateTest, IsPreloaded);

  ~TransportSecurityState();

  // If we have a callback configured, call it to let our serialiser know that
  // our state is dirty.
  void DirtyNotify();
  bool IsPreloadedSTS(const std::string& canonicalized_host,
                      bool sni_available,
                      DomainState* out);

  static std::string CanonicalizeHost(const std::string& host);
  static bool Deserialise(const std::string& state,
                          bool* dirty,
                          std::map<std::string, DomainState>* out);

  // The set of hosts that have enabled TransportSecurity. The keys here
  // are SHA256(DNSForm(domain)) where DNSForm converts from dotted form
  // ('www.google.com') to the form used in DNS: "\x03www\x06google\x03com"
  std::map<std::string, DomainState> enabled_hosts_;

  // These hosts are extra rules to treat as built-in, passed in the
  // constructor (typically originating from the command line).
  std::map<std::string, DomainState> forced_hosts_;

  // Our delegate who gets notified when we are dirtied, or NULL.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TransportSecurityState);
};

}  // namespace net

#endif  // NET_BASE_TRANSPORT_SECURITY_STATE_H_
