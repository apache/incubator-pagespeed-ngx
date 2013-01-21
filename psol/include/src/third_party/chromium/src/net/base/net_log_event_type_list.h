// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: No header guards are used, since this file is intended to be expanded
// directly into net_log.h. DO NOT include this file anywhere else.

// --------------------------------------------------------------------------
// General pseudo-events
// --------------------------------------------------------------------------

// Something got cancelled (we determine what is cancelled based on the
// log context around it.)
EVENT_TYPE(CANCELLED)

// Marks the creation/destruction of a request (net::URLRequest or
// SocketStream).
EVENT_TYPE(REQUEST_ALIVE)

// ------------------------------------------------------------------------
// HostResolverImpl
// ------------------------------------------------------------------------

// The start/end of waiting on a host resolve (DNS) request.
// The BEGIN phase contains the following parameters:
//
//   {
//     "source_dependency": <Source id of the request being waited on>,
//   }
EVENT_TYPE(HOST_RESOLVER_IMPL)

// The start/end of a host resolve (DNS) request.  Note that these events are
// logged for all DNS requests, though not all requests result in the creation
// of a HostResolvedImpl::Request object.
//
// The BEGIN phase contains the following parameters:
//
//   {
//     "host": <Hostname associated with the request>,
//     "source_dependency": <Source id, if any, of what created the request>,
//   }
//
// If an error occurred, the END phase will contain these parameters:
//   {
//     "net_error": <The net error code integer for the failure>,
//     "os_error": <The exact error code integer that getaddrinfo() returned>,
//   }

EVENT_TYPE(HOST_RESOLVER_IMPL_REQUEST)

// This event is logged when a request is handled by a cache entry.
EVENT_TYPE(HOST_RESOLVER_IMPL_CACHE_HIT)

// This event means a request was queued/dequeued for subsequent job creation,
// because there are already too many active HostResolverImpl::Jobs.
//
// The BEGIN phase contains the following parameters:
//
//   {
//     "priority": <Priority of the queued request>,
//   }
EVENT_TYPE(HOST_RESOLVER_IMPL_JOB_POOL_QUEUE)

// This event is created when a new HostResolverImpl::Request is evicted from
// JobPool without a Job being created, because the limit on number of queued
// Requests was reached.
EVENT_TYPE(HOST_RESOLVER_IMPL_JOB_POOL_QUEUE_EVICTED)

// This event is created when a new HostResolverImpl::Job is about to be created
// for a request.
EVENT_TYPE(HOST_RESOLVER_IMPL_CREATE_JOB)

// This event is created when HostResolverImpl::Job is about to start a new
// attempt to resolve the host.
//
// The ATTEMPT_STARTED event has the parameters:
//
//   {
//     "attempt_number": <the number of the attempt that is resolving the host>,
//   }
EVENT_TYPE(HOST_RESOLVER_IMPL_ATTEMPT_STARTED)

// This event is created when HostResolverImpl::Job has finished resolving the
// host.
//
// The ATTEMPT_FINISHED event has the parameters:
//
//   {
//     "attempt_number": <the number of the attempt that has resolved the host>,
//   }
// If an error occurred, the END phase will contain these additional parameters:
//   {
//     "net_error": <The net error code integer for the failure>,
//     "os_error": <The exact error code integer that getaddrinfo() returned>,
//   }
EVENT_TYPE(HOST_RESOLVER_IMPL_ATTEMPT_FINISHED)

// This is logged for a request when it's attached to a
// HostResolverImpl::Job.  When this occurs without a preceding
// HOST_RESOLVER_IMPL_CREATE_JOB entry, it means the request was attached to an
// existing HostResolverImpl::Job.
//
// If the BoundNetLog used to create the event has a valid source id, the BEGIN
// phase contains the following parameters:
//
//   {
//     "source_dependency": <Source identifier for the attached Job>,
//   }
EVENT_TYPE(HOST_RESOLVER_IMPL_JOB_ATTACH)

// The creation/completion of a host resolve (DNS) job.
// The BEGIN phase contains the following parameters:
//
//   {
//     "host": <Hostname associated with the request>,
//     "source_dependency": <Source id, if any, of what created the request>,
//   }
//
// On success, the END phase has these parameters:
//   {
//     "address_list": <The host name being resolved>,
//   }
// If an error occurred, the END phase will contain these parameters:
//   {
//     "net_error": <The net error code integer for the failure>,
//     "os_error": <The exact error code integer that getaddrinfo() returned>,
//   }
EVENT_TYPE(HOST_RESOLVER_IMPL_JOB)

// ------------------------------------------------------------------------
// InitProxyResolver
// ------------------------------------------------------------------------

// The start/end of auto-detect + custom PAC URL configuration.
EVENT_TYPE(INIT_PROXY_RESOLVER)

// The start/end of when proxy autoconfig was artificially paused following
// a network change event. (We wait some amount of time after being told of
// network changes to avoid hitting spurious errors during auto-detect).
EVENT_TYPE(INIT_PROXY_RESOLVER_WAIT)

// The start/end of download of a PAC script. This could be the well-known
// WPAD URL (if testing auto-detect), or a custom PAC URL.
//
// The START event has the parameters:
//   {
//     "source": <String describing where PAC script comes from>,
//   }
//
// If the fetch failed, then the END phase has these parameters:
//   {
//      "net_error": <Net error code integer>,
//   }
EVENT_TYPE(INIT_PROXY_RESOLVER_FETCH_PAC_SCRIPT)

// The start/end of the testing of a PAC script (trying to parse the fetched
// file as javascript).
//
// If the parsing of the script failed, the END phase will have parameters:
//   {
//      "net_error": <Net error code integer>,
//   }
EVENT_TYPE(INIT_PROXY_RESOLVER_SET_PAC_SCRIPT)

// This event means that initialization failed because there was no
// configured script fetcher. (This indicates a configuration error).
EVENT_TYPE(INIT_PROXY_RESOLVER_HAS_NO_FETCHER)

// This event is emitted after deciding to fall-back to the next source
// of PAC scripts in the list.
EVENT_TYPE(INIT_PROXY_RESOLVER_FALLING_BACK_TO_NEXT_PAC_SOURCE)

// ------------------------------------------------------------------------
// ProxyService
// ------------------------------------------------------------------------

// The start/end of a proxy resolve request.
EVENT_TYPE(PROXY_SERVICE)

// The time while a request is waiting on InitProxyResolver to configure
// against either WPAD or custom PAC URL. The specifics on this time
// are found from ProxyService::init_proxy_resolver_log().
EVENT_TYPE(PROXY_SERVICE_WAITING_FOR_INIT_PAC)

// This event is emitted to show what the PAC script returned. It can contain
// extra parameters that are either:
//   {
//      "pac_string": <List of valid proxy servers, in PAC format>,
//   }
//
//  Or if the the resolver failed:
//   {
//      "net_error": <Net error code that resolver failed with>,
//   }
EVENT_TYPE(PROXY_SERVICE_RESOLVED_PROXY_LIST)

// This event is emitted whenever the proxy settings used by ProxyService
// change.
//
// It contains these parameters:
//  {
//     "old_config": <Dump of the previous proxy settings>,
//     "new_config": <Dump of the new proxy settings>,
//  }
//
// Note that the "old_config" key will be omitted on the first fetch of the
// proxy settings (since there wasn't a previous value).
EVENT_TYPE(PROXY_CONFIG_CHANGED)

// ------------------------------------------------------------------------
// Proxy Resolver
// ------------------------------------------------------------------------

// Measures the time taken to execute the "myIpAddress()" javascript binding.
EVENT_TYPE(PAC_JAVASCRIPT_MY_IP_ADDRESS)

// Measures the time taken to execute the "myIpAddressEx()" javascript binding.
EVENT_TYPE(PAC_JAVASCRIPT_MY_IP_ADDRESS_EX)

// Measures the time taken to execute the "dnsResolve()" javascript binding.
EVENT_TYPE(PAC_JAVASCRIPT_DNS_RESOLVE)

// Measures the time taken to execute the "dnsResolveEx()" javascript binding.
EVENT_TYPE(PAC_JAVASCRIPT_DNS_RESOLVE_EX)

// This event is emitted when a javascript error has been triggered by a
// PAC script. It contains the following event parameters:
//   {
//      "line_number": <The line number in the PAC script
//                      (or -1 if not applicable)>,
//      "message": <The error message>,
//   }
EVENT_TYPE(PAC_JAVASCRIPT_ERROR)

// This event is emitted when a PAC script called alert(). It contains the
// following event parameters:
//   {
//      "message": <The string of the alert>,
//   }
EVENT_TYPE(PAC_JAVASCRIPT_ALERT)

// Measures the time that a proxy resolve request was stalled waiting for a
// proxy resolver thread to free-up.
EVENT_TYPE(WAITING_FOR_PROXY_RESOLVER_THREAD)

// This event is emitted just before a PAC request is bound to a thread. It
// contains these parameters:
//
//   {
//     "thread_number": <Identifier for the PAC thread that is going to
//                       run this request>,
//   }
EVENT_TYPE(SUBMITTED_TO_RESOLVER_THREAD)

// ------------------------------------------------------------------------
// StreamSocket
// ------------------------------------------------------------------------

// The start/end of a TCP connect(). This corresponds with a call to
// TCPClientSocket::Connect().
//
// The START event contains these parameters:
//
//   {
//     "address_list": <List of network address strings>,
//   }
//
// And the END event will contain the following parameters on failure:
//
//   {
//     "net_error": <Net integer error code>,
//   }
EVENT_TYPE(TCP_CONNECT)

// Nested within TCP_CONNECT, there may be multiple attempts to connect
// to the individual addresses. The START event will describe the
// address the attempt is for:
//
//   {
//     "address": <String of the network address>,
//   }
//
// And the END event will contain the system error code if it failed:
//
//   {
//     "os_error": <Integer error code the operating system returned>,
//   }
EVENT_TYPE(TCP_CONNECT_ATTEMPT)

// The start/end of a TCP connect(). This corresponds with a call to
// TCPServerSocket::Accept().
//
// The END event will contain the following parameters on success:
//   {
//     "address": <Remote address of the accepted connection>,
//   }
// On failure it contains the following parameters
//   {
//     "net_error": <Net integer error code>,
//   }
EVENT_TYPE(TCP_ACCEPT)

// Marks the begin/end of a socket (TCP/SOCKS/SSL).
EVENT_TYPE(SOCKET_ALIVE)

// This event is logged to the socket stream whenever the socket is
// acquired/released via a ClientSocketHandle.
//
// The BEGIN phase contains the following parameters:
//
//   {
//     "source_dependency": <Source identifier for the controlling entity>,
//   }
EVENT_TYPE(SOCKET_IN_USE)

// The start/end of a SOCKS connect().
EVENT_TYPE(SOCKS_CONNECT)

// The start/end of a SOCKS5 connect().
EVENT_TYPE(SOCKS5_CONNECT)

// This event is emitted when the SOCKS connect fails because the provided
// was longer than 255 characters.
EVENT_TYPE(SOCKS_HOSTNAME_TOO_BIG)

// These events are emitted when insufficient data was read while
// trying to establish a connection to the SOCKS proxy server
// (during the greeting phase or handshake phase, respectively).
EVENT_TYPE(SOCKS_UNEXPECTEDLY_CLOSED_DURING_GREETING)
EVENT_TYPE(SOCKS_UNEXPECTEDLY_CLOSED_DURING_HANDSHAKE)

// This event indicates that a bad version number was received in the
// proxy server's response. The extra parameters show its value:
//   {
//     "version": <Integer version number in the response>,
//   }
EVENT_TYPE(SOCKS_UNEXPECTED_VERSION)

// This event indicates that the SOCKS proxy server returned an error while
// trying to create a connection. The following parameters will be attached
// to the event:
//   {
//     "error_code": <Integer error code returned by the server>,
//   }
EVENT_TYPE(SOCKS_SERVER_ERROR)

// This event indicates that the SOCKS proxy server asked for an authentication
// method that we don't support. The following parameters are attached to the
// event:
//   {
//     "method": <Integer method code>,
//   }
EVENT_TYPE(SOCKS_UNEXPECTED_AUTH)

// This event indicates that the SOCKS proxy server's response indicated an
// address type which we are not prepared to handle.
// The following parameters are attached to the event:
//   {
//     "address_type": <Integer code for the address type>,
//   }
EVENT_TYPE(SOCKS_UNKNOWN_ADDRESS_TYPE)

// The start/end of an SSL "connect" (aka client handshake).
EVENT_TYPE(SSL_CONNECT)

// The start/end of an SSL server handshake (aka "accept").
EVENT_TYPE(SSL_SERVER_HANDSHAKE)

// An SSL error occurred while trying to do the indicated activity.
// The following parameters are attached to the event:
//   {
//     "net_error": <Integer code for the specific error type>,
//     "ssl_lib_error": <SSL library's integer code for the specific error type>
//   }
EVENT_TYPE(SSL_HANDSHAKE_ERROR)
EVENT_TYPE(SSL_READ_ERROR)
EVENT_TYPE(SSL_WRITE_ERROR)

// An SSL Snap Start was attempted
// The following parameters are attached to the event:
//   {
//     "type": <Integer code for the Snap Start result>,
//   }
EVENT_TYPE(SSL_SNAP_START)

// We found that our prediction of the server's certificates was correct and
// we merged the verification with the SSLHostInfo.
EVENT_TYPE(SSL_VERIFICATION_MERGED)

// An SSL error occurred while calling an NSS function not directly related to
// one of the above activities.  Can also be used when more information than
// is provided by just an error code is needed:
//   {
//     "function": <Name of the NSS function, as a string>,
//     "param": <Most relevant parameter, if any>,
//     "ssl_lib_error": <NSS library's integer code for the specific error type>
//   }
EVENT_TYPE(SSL_NSS_ERROR)

// The specified number of bytes were sent on the socket.
// The following parameters are attached:
//   {
//     "byte_count": <Number of bytes that were just sent>,
//     "hex_encoded_bytes": <The exact bytes sent, as a hexadecimal string.
//                           Only present when byte logging is enabled>,
//   }
EVENT_TYPE(SOCKET_BYTES_SENT)
EVENT_TYPE(SSL_SOCKET_BYTES_SENT)

// The specified number of bytes were received on the socket.
// The following parameters are attached:
//   {
//     "byte_count": <Number of bytes that were just received>,
//     "hex_encoded_bytes": <The exact bytes received, as a hexadecimal string.
//                           Only present when byte logging is enabled>,
//   }
EVENT_TYPE(SOCKET_BYTES_RECEIVED)
EVENT_TYPE(SSL_SOCKET_BYTES_RECEIVED)

// ------------------------------------------------------------------------
// ClientSocketPoolBase::ConnectJob
// ------------------------------------------------------------------------

// The start/end of a ConnectJob.
EVENT_TYPE(SOCKET_POOL_CONNECT_JOB)

// The start/end of the ConnectJob::Connect().
//
// The BEGIN phase has these parameters:
//
//   {
//     "group_name": <The group name for the socket request.>,
//   }
EVENT_TYPE(SOCKET_POOL_CONNECT_JOB_CONNECT)

// This event is logged whenever the ConnectJob gets a new socket
// association. The event parameters point to that socket:
//
//   {
//     "source_dependency": <The source identifier for the new socket.>,
//   }
EVENT_TYPE(CONNECT_JOB_SET_SOCKET)

// Whether the connect job timed out.
EVENT_TYPE(SOCKET_POOL_CONNECT_JOB_TIMED_OUT)

// ------------------------------------------------------------------------
// ClientSocketPoolBaseHelper
// ------------------------------------------------------------------------

// The start/end of a client socket pool request for a socket.
EVENT_TYPE(SOCKET_POOL)

// The request stalled because there are too many sockets in the pool.
EVENT_TYPE(SOCKET_POOL_STALLED_MAX_SOCKETS)

// The request stalled because there are too many sockets in the group.
EVENT_TYPE(SOCKET_POOL_STALLED_MAX_SOCKETS_PER_GROUP)

// Indicates that we reused an existing socket. Attached to the event are
// the parameters:
//   {
//     "idle_ms": <The number of milliseconds the socket was sitting idle for>,
//   }
EVENT_TYPE(SOCKET_POOL_REUSED_AN_EXISTING_SOCKET)

// This event simply describes the host:port that were requested from the
// socket pool. Its parameters are:
//   {
//     "host_and_port": <String encoding the host and port>,
//   }
EVENT_TYPE(TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKET)

// This event simply describes the host:port that were requested from the
// socket pool. Its parameters are:
//   {
//     "host_and_port": <String encoding the host and port>,
//   }
EVENT_TYPE(TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKETS)


// A backup socket is created due to slow connect
EVENT_TYPE(SOCKET_BACKUP_CREATED)

// This event is sent when a connect job is eventually bound to a request
// (because of late binding and socket backup jobs, we don't assign the job to
// a request until it has completed).
//
// The event parameters are:
//   {
//      "source_dependency": <Source identifer for the connect job we are
//                            bound to>,
//   }
EVENT_TYPE(SOCKET_POOL_BOUND_TO_CONNECT_JOB)

// Identifies the NetLog::Source() for the Socket assigned to the pending
// request. The event parameters are:
//   {
//      "source_dependency": <Source identifier for the socket we acquired>,
//   }
EVENT_TYPE(SOCKET_POOL_BOUND_TO_SOCKET)

// The start/end of a client socket pool request for multiple sockets.
// The event parameters are:
//   {
//      "num_sockets": <Number of sockets we're trying to ensure are connected>,
//   }
EVENT_TYPE(SOCKET_POOL_CONNECTING_N_SOCKETS)

// ------------------------------------------------------------------------
// URLRequest
// ------------------------------------------------------------------------

// Measures the time it took a net::URLRequestJob to start. For the most part
// this corresponds with the time between net::URLRequest::Start() and
// net::URLRequest::ResponseStarted(), however it is also repeated for every
// redirect, and every intercepted job that handles the request.
//
// For the BEGIN phase, the following parameters are attached:
//   {
//      "url": <String of URL being loaded>,
//      "method": <The method ("POST" or "GET" or "HEAD" etc..)>,
//      "load_flags": <Numeric value of the combined load flags>,
//   }
//
// For the END phase, if there was an error, the following parameters are
// attached:
//   {
//      "net_error": <Net error code of the failure>,
//   }
EVENT_TYPE(URL_REQUEST_START_JOB)

// This event is sent once a net::URLRequest receives a redirect. The parameters
// attached to the event are:
//   {
//     "location": <The URL that was redirected to>,
//   }
EVENT_TYPE(URL_REQUEST_REDIRECTED)

// Measures the time a net::URLRequest is blocked waiting for an extension to
// respond to the onBefoteRequest extension event.
EVENT_TYPE(URL_REQUEST_BLOCKED_ON_EXTENSION)

// The specified number of bytes were read from the net::URLRequest.
// The filtered event is used when the bytes were passed through a filter before
// being read.  This event is only present when byte logging is enabled.
// The following parameters are attached:
//   {
//     "byte_count": <Number of bytes that were just sent>,
//     "hex_encoded_bytes": <The exact bytes sent, as a hexadecimal string>,
//   }
EVENT_TYPE(URL_REQUEST_JOB_BYTES_READ)
EVENT_TYPE(URL_REQUEST_JOB_FILTERED_BYTES_READ)

// ------------------------------------------------------------------------
// HttpCache
// ------------------------------------------------------------------------

// Measures the time while getting a reference to the back end.
EVENT_TYPE(HTTP_CACHE_GET_BACKEND)

// Measures the time while opening a disk cache entry.
EVENT_TYPE(HTTP_CACHE_OPEN_ENTRY)

// Measures the time while creating a disk cache entry.
EVENT_TYPE(HTTP_CACHE_CREATE_ENTRY)

// Measures the time it takes to add a HttpCache::Transaction to an http cache
// entry's list of active Transactions.
EVENT_TYPE(HTTP_CACHE_ADD_TO_ENTRY)

// Measures the time while deleting a disk cache entry.
EVENT_TYPE(HTTP_CACHE_DOOM_ENTRY)

// Measures the time while reading/writing a disk cache entry's response headers
// or metadata.
EVENT_TYPE(HTTP_CACHE_READ_INFO)
EVENT_TYPE(HTTP_CACHE_WRITE_INFO)

// Measures the time while reading/writing a disk cache entry's body.
EVENT_TYPE(HTTP_CACHE_READ_DATA)
EVENT_TYPE(HTTP_CACHE_WRITE_DATA)

// ------------------------------------------------------------------------
// Disk Cache / Memory Cache
// ------------------------------------------------------------------------

// The creation/destruction of a disk_cache::EntryImpl object.  The "creation"
// is considered to be the point at which an Entry is first considered to be
// good and associated with a key.  Note that disk and memory cache entries
// share event types.
//
// For the BEGIN phase, the following parameters are attached:
//   {
//     "created": <true if the Entry was created, rather than being opened>,
//     "key": <The Entry's key>,
//   }
EVENT_TYPE(DISK_CACHE_ENTRY_IMPL)
EVENT_TYPE(DISK_CACHE_MEM_ENTRY_IMPL)

// Logs the time required to read/write data from/to a cache entry.
//
// For the BEGIN phase, the following parameters are attached:
//   {
//     "index": <Index being read/written>,
//     "offset": <Offset being read/written>,
//     "buf_len": <Length of buffer being read to/written from>,
//     "truncate": <If present for a write, the truncate flag is set to true.
//                  Not present in reads or writes where it is false>,
//   }
//
// For the END phase, the following parameters are attached:
//   {
//     "bytes_copied": <Number of bytes copied.  Not present on error>,
//     "net_error": <Network error code.  Only present on error>,
//   }
EVENT_TYPE(ENTRY_READ_DATA)
EVENT_TYPE(ENTRY_WRITE_DATA)

// Logged when sparse read/write operation starts/stops for an Entry.
//
// For the BEGIN phase, the following parameters are attached:
//   {
//     "offset": <Offset at which to start reading>,
//     "buff_len": <Bytes to read/write>,
//   }
EVENT_TYPE(SPARSE_READ)
EVENT_TYPE(SPARSE_WRITE)

// Logged when a parent Entry starts/stops reading/writing a child Entry's data.
//
// For the BEGIN phase, the following parameters are attached:
//   {
//     "source_dependency": <Source id of the child entry>,
//     "child_len": <Bytes to read/write from/to child>,
//   }
EVENT_TYPE(SPARSE_READ_CHILD_DATA)
EVENT_TYPE(SPARSE_WRITE_CHILD_DATA)

// Logged when sparse GetAvailableRange operation starts/stops for an Entry.
//
// For the BEGIN phase, the following parameters are attached:
//   {
//     "buff_len": <Bytes to read/write>,
//     "offset": <Offset at which to start reading>,
//   }
//
// For the END phase, the following parameters are attached.  No parameters are
// attached when cancelled:
//   {
//     "length": <Length of returned range. Only present on success>,
//     "start": <Position where returned range starts. Only present on success>,
//     "net_error": <Resulting error code. Only present on failure. This may be
//                   "OK" when there's no error, but no available bytes in the
//                   range>,
//   }
EVENT_TYPE(SPARSE_GET_RANGE)

// Indicates the children of a sparse EntryImpl are about to be deleted.
// Not logged for MemEntryImpls.
EVENT_TYPE(SPARSE_DELETE_CHILDREN)

// Logged when an EntryImpl is closed.  Not logged for MemEntryImpls.
EVENT_TYPE(ENTRY_CLOSE)

// Logged when an entry is doomed.
EVENT_TYPE(ENTRY_DOOM)

// ------------------------------------------------------------------------
// HttpStreamFactoryImpl
// ------------------------------------------------------------------------

// Measures the time taken to fulfill the HttpStreamRequest.
EVENT_TYPE(HTTP_STREAM_REQUEST)

// Measures the time taken to execute the HttpStreamFactoryImpl::Job
EVENT_TYPE(HTTP_STREAM_JOB)

// Identifies the NetLog::Source() for the Job that fulfilled the request.
// request. The event parameters are:
//   {
//      "source_dependency": <Source identifier for the job we acquired>,
//   }
EVENT_TYPE(HTTP_STREAM_REQUEST_BOUND_TO_JOB)

// ------------------------------------------------------------------------
// HttpNetworkTransaction
// ------------------------------------------------------------------------

// Measures the time taken to send the tunnel request to the server.
EVENT_TYPE(HTTP_TRANSACTION_TUNNEL_SEND_REQUEST)

// This event is sent for a tunnel request.
// The following parameters are attached:
//   {
//     "line": <The HTTP request line, CRLF terminated>,
//     "headers": <The list of header:value pairs>,
//   }
EVENT_TYPE(HTTP_TRANSACTION_SEND_TUNNEL_HEADERS)

// Measures the time to read the tunnel response headers from the server.
EVENT_TYPE(HTTP_TRANSACTION_TUNNEL_READ_HEADERS)

// This event is sent on receipt of the HTTP response headers to a tunnel
// request.
// The following parameters are attached:
//   {
//     "headers": <The list of header:value pairs>,
//   }
EVENT_TYPE(HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS)

// Measures the time taken to send the request to the server.
EVENT_TYPE(HTTP_TRANSACTION_SEND_REQUEST)

// This event is sent for a HTTP request.
// The following parameters are attached:
//   {
//     "line": <The HTTP request line, CRLF terminated>,
//     "headers": <The list of header:value pairs>,
//   }
EVENT_TYPE(HTTP_TRANSACTION_SEND_REQUEST_HEADERS)

// Measures the time to read HTTP response headers from the server.
EVENT_TYPE(HTTP_TRANSACTION_READ_HEADERS)

// This event is sent on receipt of the HTTP response headers.
// The following parameters are attached:
//   {
//     "headers": <The list of header:value pairs>,
//   }
EVENT_TYPE(HTTP_TRANSACTION_READ_RESPONSE_HEADERS)

// Measures the time to read the entity body from the server.
EVENT_TYPE(HTTP_TRANSACTION_READ_BODY)

// Measures the time taken to read the response out of the socket before
// restarting for authentication, on keep alive connections.
EVENT_TYPE(HTTP_TRANSACTION_DRAIN_BODY_FOR_AUTH_RESTART)

// ------------------------------------------------------------------------
// SpdySession
// ------------------------------------------------------------------------

// The start/end of a SpdySession.
//   {
//     "host": <The host-port string>,
//     "proxy": <The Proxy PAC string>,
//   }
EVENT_TYPE(SPDY_SESSION)

// This event is sent for a SPDY SYN_STREAM.
// The following parameters are attached:
//   {
//     "flags": <The control frame flags>,
//     "headers": <The list of header:value pairs>,
//     "id": <The stream id>,
//   }
EVENT_TYPE(SPDY_SESSION_SYN_STREAM)

// This event is sent for a SPDY SYN_STREAM pushed by the server, where a
// net::URLRequest is already waiting for the stream.
// The following parameters are attached:
//   {
//     "flags": <The control frame flags>,
//     "headers": <The list of header:value pairs>,
//     "id": <The stream id>,
//     "associated_stream": <The stream id>,
//   }
EVENT_TYPE(SPDY_SESSION_PUSHED_SYN_STREAM)

// This event is sent for a SPDY HEADERS frame.
// The following parameters are attached:
//   {
//     "flags": <The control frame flags>,
//     "headers": <The list of header:value pairs>,
//     "id": <The stream id>,
//   }
EVENT_TYPE(SPDY_SESSION_HEADERS)

// This event is sent for a SPDY SYN_REPLY.
// The following parameters are attached:
//   {
//     "flags": <The control frame flags>,
//     "headers": <The list of header:value pairs>,
//     "id": <The stream id>,
//   }
EVENT_TYPE(SPDY_SESSION_SYN_REPLY)

// On sending a SPDY SETTINGS frame.
// The following parameters are attached:
//   {
//     "settings": <The list of setting id:value pairs>,
//   }
EVENT_TYPE(SPDY_SESSION_SEND_SETTINGS)

// Receipt of a SPDY SETTINGS frame.
// The following parameters are attached:
//   {
//     "settings": <The list of setting id:value pairs>,
//   }
EVENT_TYPE(SPDY_SESSION_RECV_SETTINGS)

// The receipt of a RST_STREAM
// The following parameters are attached:
//   {
//     "stream_id": <The stream ID for the window update>,
//     "status": <The reason for the RST_STREAM>,
//   }
EVENT_TYPE(SPDY_SESSION_RST_STREAM)

// Sending of a RST_STREAM
// The following parameters are attached:
//   {
//     "stream_id": <The stream ID for the window update>,
//     "status": <The reason for the RST_STREAM>,
//   }
EVENT_TYPE(SPDY_SESSION_SEND_RST_STREAM)

// Receipt of a SPDY GOAWAY frame.
// The following parameters are attached:
//   {
//     "last_accepted_stream_id": <Last stream id accepted by the server, duh>,
//     "active_streams":          <Number of active streams>,
//     "unclaimed_streams":       <Number of unclaimed push streams>,
//   }
EVENT_TYPE(SPDY_SESSION_GOAWAY)

// Receipt of a SPDY WINDOW_UPDATE frame (which controls the send window).
//   {
//     "stream_id": <The stream ID for the window update>,
//     "delta"    : <The delta window size>,
//     "new_size" : <The new window size (computed)>,
//   }
EVENT_TYPE(SPDY_SESSION_SEND_WINDOW_UPDATE)

// Sending of a SPDY WINDOW_UPDATE frame (which controls the receive window).
//   {
//     "stream_id": <The stream ID for the window update>,
//     "delta"    : <The delta window size>,
//     "new_size" : <The new window size (computed)>,
//   }
EVENT_TYPE(SPDY_SESSION_RECV_WINDOW_UPDATE)

// Sending a data frame
//   {
//     "stream_id": <The stream ID for the window update>,
//     "length"   : <The size of data sent>,
//     "flags"    : <Send data flags>,
//   }
EVENT_TYPE(SPDY_SESSION_SEND_DATA)

// Receiving a data frame
//   {
//     "stream_id": <The stream ID for the window update>,
//     "length"   : <The size of data sent>,
//     "flags"    : <Send data flags>,
//   }
EVENT_TYPE(SPDY_SESSION_RECV_DATA)

// Logs that a stream is stalled on the send window being closed.
EVENT_TYPE(SPDY_SESSION_STALLED_ON_SEND_WINDOW)

// Session is closing
//   {
//     "status": <The error status of the closure>,
//   }
EVENT_TYPE(SPDY_SESSION_CLOSE)

// Event when the creation of a stream is stalled because we're at
// the maximum number of concurrent streams.
EVENT_TYPE(SPDY_SESSION_STALLED_MAX_STREAMS)

// ------------------------------------------------------------------------
// SpdySessionPool
// ------------------------------------------------------------------------

// This event indicates the pool is reusing an existing session
//   {
//     "id": <The session id>,
//   }
EVENT_TYPE(SPDY_SESSION_POOL_FOUND_EXISTING_SESSION)

// This event indicates the pool is reusing an existing session from an
// IP pooling match.
//   {
//     "id": <The session id>,
//   }
EVENT_TYPE(SPDY_SESSION_POOL_FOUND_EXISTING_SESSION_FROM_IP_POOL)

// This event indicates the pool created a new session
//   {
//     "id": <The session id>,
//   }
EVENT_TYPE(SPDY_SESSION_POOL_CREATED_NEW_SESSION)

// This event indicates that a SSL socket has been upgraded to a SPDY session.
//   {
//     "id": <The session id>,
//   }
EVENT_TYPE(SPDY_SESSION_POOL_IMPORTED_SESSION_FROM_SOCKET)

// This event indicates that the session has been removed.
//   {
//     "id": <The session id>,
//   }
EVENT_TYPE(SPDY_SESSION_POOL_REMOVE_SESSION)

// ------------------------------------------------------------------------
// SpdyStream
// ------------------------------------------------------------------------

// The begin and end of a SPDY STREAM.
EVENT_TYPE(SPDY_STREAM)

// Logs that a stream attached to a pushed stream.
EVENT_TYPE(SPDY_STREAM_ADOPTED_PUSH_STREAM)

// This event indicates that the send window has been updated
//   {
//     "id":         <The stream id>,
//     "delta":      <The window size delta>,
//     "new_window": <The new window size>,
//   }
EVENT_TYPE(SPDY_STREAM_SEND_WINDOW_UPDATE)

// This event indicates that the recv window has been updated
//   {
//     "id":         <The stream id>,
//     "delta":      <The window size delta>,
//     "new_window": <The new window size>,
//   }
EVENT_TYPE(SPDY_STREAM_RECV_WINDOW_UPDATE)

// ------------------------------------------------------------------------
// HttpStreamParser
// ------------------------------------------------------------------------

// Measures the time to read HTTP response headers from the server.
EVENT_TYPE(HTTP_STREAM_PARSER_READ_HEADERS)

// ------------------------------------------------------------------------
// SocketStream
// ------------------------------------------------------------------------

// Measures the time between SocketStream::Connect() and
// SocketStream::DidEstablishConnection()
//
// For the BEGIN phase, the following parameters are attached:
//   {
//      "url": <String of URL being loaded>,
//   }
//
// For the END phase, if there was an error, the following parameters are
// attached:
//   {
//      "net_error": <Net error code of the failure>,
//   }
EVENT_TYPE(SOCKET_STREAM_CONNECT)

// A message sent on the SocketStream.
EVENT_TYPE(SOCKET_STREAM_SENT)

// A message received on the SocketStream.
EVENT_TYPE(SOCKET_STREAM_RECEIVED)

// ------------------------------------------------------------------------
// WebSocketJob
// ------------------------------------------------------------------------

// This event is sent for a WebSocket handshake request.
// The following parameters are attached:
//   {
//     "headers": <handshake request message>,
//   }
EVENT_TYPE(WEB_SOCKET_SEND_REQUEST_HEADERS)

// This event is sent on receipt of the WebSocket handshake response headers.
// The following parameters are attached:
//   {
//     "headers": <handshake response message>,
//   }
EVENT_TYPE(WEB_SOCKET_READ_RESPONSE_HEADERS)

// ------------------------------------------------------------------------
// SOCKS5ClientSocket
// ------------------------------------------------------------------------

// The time spent sending the "greeting" to the SOCKS server.
EVENT_TYPE(SOCKS5_GREET_WRITE)

// The time spent waiting for the "greeting" response from the SOCKS server.
EVENT_TYPE(SOCKS5_GREET_READ)

// The time spent sending the CONNECT request to the SOCKS server.
EVENT_TYPE(SOCKS5_HANDSHAKE_WRITE)

// The time spent waiting for the response to the CONNECT request.
EVENT_TYPE(SOCKS5_HANDSHAKE_READ)

// ------------------------------------------------------------------------
// HTTP Authentication
// ------------------------------------------------------------------------

// The time spent authenticating to the proxy.
EVENT_TYPE(AUTH_PROXY)

// The time spent authentication to the server.
EVENT_TYPE(AUTH_SERVER)

// ------------------------------------------------------------------------
// HTML5 Application Cache
// ------------------------------------------------------------------------

// This event is emitted whenever a request is satistifed directly from
// the appache.
EVENT_TYPE(APPCACHE_DELIVERING_CACHED_RESPONSE)

// This event is emitted whenever the appcache uses a fallback response.
EVENT_TYPE(APPCACHE_DELIVERING_FALLBACK_RESPONSE)

// This event is emitted whenever the appcache generates an error response.
EVENT_TYPE(APPCACHE_DELIVERING_ERROR_RESPONSE)

// ------------------------------------------------------------------------
// Global events
// ------------------------------------------------------------------------
// These are events which are not grouped by source id, as they have no
// context.

// This event is emitted whenever NetworkChangeNotifier determines that the
// underlying network has changed.
EVENT_TYPE(NETWORK_IP_ADDRESSES_CHANGED)

// ------------------------------------------------------------------------
// Exponential back-off throttling events
// ------------------------------------------------------------------------

// Emitted when back-off is disabled for a given host, or the first time
// a localhost URL is used (back-off is always disabled for localhost).
//   {
//     "host": <The hostname back-off was disabled for>
//   }
EVENT_TYPE(THROTTLING_DISABLED_FOR_HOST)

// Emitted when a request is denied due to exponential back-off throttling.
//   {
//     "url":              <URL that was being requested>,
//     "num_failures":     <Failure count for the URL>,
//     "release_after_ms": <Number of milliseconds until URL will be unblocked>
//   }
EVENT_TYPE(THROTTLING_REJECTED_REQUEST)

// Emitted when throttling entry receives an X-Retry-After header.
//   {
//     "url":               <URL that was being requested>,
//     "retry_after_ms":    <Milliseconds until retry-after expires>
//   }
EVENT_TYPE(THROTTLING_GOT_CUSTOM_RETRY_AFTER)
