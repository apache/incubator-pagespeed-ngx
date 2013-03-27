// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UNIX_DOMAIN_SOCKET_POSIX_H_
#define NET_BASE_UNIX_DOMAIN_SOCKET_POSIX_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/base/stream_listen_socket.h"

#if defined(OS_ANDROID) || defined(OS_LINUX)
// Feature only supported on Linux currently. This lets the Unix Domain Socket
// not be backed by the file system.
#define SOCKET_ABSTRACT_NAMESPACE_SUPPORTED
#endif

namespace net {

// Unix Domain Socket Implementation. Supports abstract namespaces on Linux.
class NET_EXPORT UnixDomainSocket : public StreamListenSocket {
 public:
  // Callback that returns whether the already connected client, identified by
  // its process |user_id| and |group_id|, is allowed to keep the connection
  // open. Note that the socket is closed immediately in case the callback
  // returns false.
  typedef base::Callback<bool (uid_t user_id, gid_t group_id)> AuthCallback;

  // Returns an authentication callback that always grants access for
  // convenience in case you don't want to use authentication.
  static AuthCallback NoAuthentication();

  // Note that the returned UnixDomainSocket instance does not take ownership of
  // |del|.
  static scoped_refptr<UnixDomainSocket> CreateAndListen(
      const std::string& path,
      StreamListenSocket::Delegate* del,
      const AuthCallback& auth_callback);

#if defined(SOCKET_ABSTRACT_NAMESPACE_SUPPORTED)
  // Same as above except that the created socket uses the abstract namespace
  // which is a Linux-only feature.
  static scoped_refptr<UnixDomainSocket> CreateAndListenWithAbstractNamespace(
      const std::string& path,
      StreamListenSocket::Delegate* del,
      const AuthCallback& auth_callback);
#endif

 private:
  UnixDomainSocket(SocketDescriptor s,
                   StreamListenSocket::Delegate* del,
                   const AuthCallback& auth_callback);
  virtual ~UnixDomainSocket();

  static UnixDomainSocket* CreateAndListenInternal(
      const std::string& path,
      StreamListenSocket::Delegate* del,
      const AuthCallback& auth_callback,
      bool use_abstract_namespace);

  static SocketDescriptor CreateAndBind(const std::string& path,
                                        bool use_abstract_namespace);

  // StreamListenSocket:
  virtual void Accept() OVERRIDE;

  AuthCallback auth_callback_;

  DISALLOW_COPY_AND_ASSIGN(UnixDomainSocket);
};

// Factory that can be used to instantiate UnixDomainSocket.
class NET_EXPORT UnixDomainSocketFactory : public StreamListenSocketFactory {
 public:
  // Note that this class does not take ownership of the provided delegate.
  UnixDomainSocketFactory(const std::string& path,
                          const UnixDomainSocket::AuthCallback& auth_callback);
  virtual ~UnixDomainSocketFactory();

  // StreamListenSocketFactory:
  virtual scoped_refptr<StreamListenSocket> CreateAndListen(
      StreamListenSocket::Delegate* delegate) const OVERRIDE;

 protected:
  const std::string path_;
  const UnixDomainSocket::AuthCallback auth_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UnixDomainSocketFactory);
};

#if defined(SOCKET_ABSTRACT_NAMESPACE_SUPPORTED)
// Use this factory to instantiate UnixDomainSocket using the abstract
// namespace feature (only supported on Linux).
class NET_EXPORT UnixDomainSocketWithAbstractNamespaceFactory
    : public UnixDomainSocketFactory {
 public:
  UnixDomainSocketWithAbstractNamespaceFactory(
      const std::string& path,
      const UnixDomainSocket::AuthCallback& auth_callback);
  virtual ~UnixDomainSocketWithAbstractNamespaceFactory();

  // UnixDomainSocketFactory:
  virtual scoped_refptr<StreamListenSocket> CreateAndListen(
      StreamListenSocket::Delegate* delegate) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(UnixDomainSocketWithAbstractNamespaceFactory);
};
#endif

}  // namespace net

#endif  // NET_BASE_UNIX_DOMAIN_SOCKET_POSIX_H_
