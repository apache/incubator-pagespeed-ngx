// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_AUTH_H__
#define NET_BASE_AUTH_H__

#include <string>

#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"

namespace net {

// Holds info about an authentication challenge that we may want to display
// to the user.
class NET_EXPORT AuthChallengeInfo :
    public base::RefCountedThreadSafe<AuthChallengeInfo> {
 public:
  AuthChallengeInfo();

  // Determines whether two AuthChallengeInfo's are equivalent.
  bool Equals(const AuthChallengeInfo& other) const;

  // Whether this came from a server or a proxy.
  bool is_proxy;

  // The service issuing the challenge.
  HostPortPair challenger;

  // The authentication scheme used, such as "basic" or "digest". If the
  // |source| is FTP_SERVER, this is an empty string. The encoding is ASCII.
  std::string scheme;

  // The realm of the challenge. May be empty. The encoding is UTF-8.
  std::string realm;

 private:
  friend class base::RefCountedThreadSafe<AuthChallengeInfo>;
  ~AuthChallengeInfo();
};

// Authentication Credentials for an authentication credentials.
class NET_EXPORT AuthCredentials {
 public:
  AuthCredentials();
  AuthCredentials(const string16& username, const string16& password);
  ~AuthCredentials();

  // Set the |username| and |password|.
  void Set(const string16& username, const string16& password);

  // Determines if |this| is equivalent to |other|.
  bool Equals(const AuthCredentials& other) const;

  // Returns true if all credentials are empty.
  bool Empty() const;

  // Overwrites the password memory to prevent it from being read if
  // it's paged out to disk.
  void Zap();

  const string16& username() const { return username_; }
  const string16& password() const { return password_; }

 private:
  // The username to provide, possibly empty. This should be ASCII only to
  // minimize compatibility problems, but arbitrary UTF-16 strings are allowed
  // and will be attempted.
  string16 username_;

  // The password to provide, possibly empty. This should be ASCII only to
  // minimize compatibility problems, but arbitrary UTF-16 strings are allowed
  // and will be attempted.
  string16 password_;

  // Intentionally allowing the implicit copy constructor and assignment
  // operators.
};

// Authentication structures
enum AuthState {
  AUTH_STATE_DONT_NEED_AUTH,
  AUTH_STATE_NEED_AUTH,
  AUTH_STATE_HAVE_AUTH,
  AUTH_STATE_CANCELED
};

class AuthData : public base::RefCountedThreadSafe<AuthData> {
 public:
  AuthState state;  // whether we need, have, or gave up on authentication.
  AuthCredentials credentials; // The credentials to use for auth.

  // We wouldn't instantiate this class if we didn't need authentication.
  AuthData();

 private:
  friend class base::RefCountedThreadSafe<AuthData>;
  ~AuthData();
};

}  // namespace net

#endif  // NET_BASE_AUTH_H__
