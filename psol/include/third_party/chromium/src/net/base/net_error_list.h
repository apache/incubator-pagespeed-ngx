// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate enum.

// This file contains the list of network errors.

//
// Ranges:
//     0- 99 System related errors
//   100-199 Connection related errors
//   200-299 Certificate errors
//   300-399 HTTP errors
//   400-499 Cache errors
//   500-599 ?
//   600-699 FTP errors
//   700-799 Certificate manager errors
//   800-899 DNS resolver errors

// An asynchronous IO operation is not yet complete.  This usually does not
// indicate a fatal error.  Typically this error will be generated as a
// notification to wait for some external notification that the IO operation
// finally completed.
NET_ERROR(IO_PENDING, -1)

// A generic failure occurred.
NET_ERROR(FAILED, -2)

// An operation was aborted (due to user action).
NET_ERROR(ABORTED, -3)

// An argument to the function is incorrect.
NET_ERROR(INVALID_ARGUMENT, -4)

// The handle or file descriptor is invalid.
NET_ERROR(INVALID_HANDLE, -5)

// The file or directory cannot be found.
NET_ERROR(FILE_NOT_FOUND, -6)

// An operation timed out.
NET_ERROR(TIMED_OUT, -7)

// The file is too large.
NET_ERROR(FILE_TOO_BIG, -8)

// An unexpected error.  This may be caused by a programming mistake or an
// invalid assumption.
NET_ERROR(UNEXPECTED, -9)

// Permission to access a resource, other than the network, was denied.
NET_ERROR(ACCESS_DENIED, -10)

// The operation failed because of unimplemented functionality.
NET_ERROR(NOT_IMPLEMENTED, -11)

// There were not enough resources to complete the operation.
NET_ERROR(INSUFFICIENT_RESOURCES, -12)

// Memory allocation failed.
NET_ERROR(OUT_OF_MEMORY, -13)

// The file upload failed because the file's modification time was different
// from the expectation.
NET_ERROR(UPLOAD_FILE_CHANGED, -14)

// The socket is not connected.
NET_ERROR(SOCKET_NOT_CONNECTED, -15)

// The file already exists.
NET_ERROR(FILE_EXISTS, -16)

// The path or file name is too long.
NET_ERROR(FILE_PATH_TOO_LONG, -17)

// Not enough room left on the disk.
NET_ERROR(FILE_NO_SPACE, -18)

// The file has a virus.
NET_ERROR(FILE_VIRUS_INFECTED, -19)

// The client chose to block the request.
NET_ERROR(BLOCKED_BY_CLIENT, -20)

// A connection was closed (corresponding to a TCP FIN).
NET_ERROR(CONNECTION_CLOSED, -100)

// A connection was reset (corresponding to a TCP RST).
NET_ERROR(CONNECTION_RESET, -101)

// A connection attempt was refused.
NET_ERROR(CONNECTION_REFUSED, -102)

// A connection timed out as a result of not receiving an ACK for data sent.
// This can include a FIN packet that did not get ACK'd.
NET_ERROR(CONNECTION_ABORTED, -103)

// A connection attempt failed.
NET_ERROR(CONNECTION_FAILED, -104)

// The host name could not be resolved.
NET_ERROR(NAME_NOT_RESOLVED, -105)

// The Internet connection has been lost.
NET_ERROR(INTERNET_DISCONNECTED, -106)

// An SSL protocol error occurred.
NET_ERROR(SSL_PROTOCOL_ERROR, -107)

// The IP address or port number is invalid (e.g., cannot connect to the IP
// address 0 or the port 0).
NET_ERROR(ADDRESS_INVALID, -108)

// The IP address is unreachable.  This usually means that there is no route to
// the specified host or network.
NET_ERROR(ADDRESS_UNREACHABLE, -109)

// The server requested a client certificate for SSL client authentication.
NET_ERROR(SSL_CLIENT_AUTH_CERT_NEEDED, -110)

// A tunnel connection through the proxy could not be established.
NET_ERROR(TUNNEL_CONNECTION_FAILED, -111)

// No SSL protocol versions are enabled.
NET_ERROR(NO_SSL_VERSIONS_ENABLED, -112)

// The client and server don't support a common SSL protocol version or
// cipher suite.
NET_ERROR(SSL_VERSION_OR_CIPHER_MISMATCH, -113)

// The server requested a renegotiation (rehandshake).
NET_ERROR(SSL_RENEGOTIATION_REQUESTED, -114)

// The proxy requested authentication (for tunnel establishment) with an
// unsupported method.
NET_ERROR(PROXY_AUTH_UNSUPPORTED, -115)

// During SSL renegotiation (rehandshake), the server sent a certificate with
// an error.
//
// Note: this error is not in the -2xx range so that it won't be handled as a
// certificate error.
NET_ERROR(CERT_ERROR_IN_SSL_RENEGOTIATION, -116)

// The SSL handshake failed because of a bad or missing client certificate.
NET_ERROR(BAD_SSL_CLIENT_AUTH_CERT, -117)

// A connection attempt timed out.
NET_ERROR(CONNECTION_TIMED_OUT, -118)

// There are too many pending DNS resolves, so a request in the queue was
// aborted.
NET_ERROR(HOST_RESOLVER_QUEUE_TOO_LARGE, -119)

// Failed establishing a connection to the SOCKS proxy server for a target host.
NET_ERROR(SOCKS_CONNECTION_FAILED, -120)

// The SOCKS proxy server failed establishing connection to the target host
// because that host is unreachable.
NET_ERROR(SOCKS_CONNECTION_HOST_UNREACHABLE, -121)

// The request to negotiate an alternate protocol failed.
NET_ERROR(NPN_NEGOTIATION_FAILED, -122)

// The peer sent an SSL no_renegotiation alert message.
NET_ERROR(SSL_NO_RENEGOTIATION, -123)

// Winsock sometimes reports more data written than passed.  This is probably
// due to a broken LSP.
NET_ERROR(WINSOCK_UNEXPECTED_WRITTEN_BYTES, -124)

// An SSL peer sent us a fatal decompression_failure alert. This typically
// occurs when a peer selects DEFLATE compression in the mistaken belief that
// it supports it.
NET_ERROR(SSL_DECOMPRESSION_FAILURE_ALERT, -125)

// An SSL peer sent us a fatal bad_record_mac alert. This has been observed
// from servers with buggy DEFLATE support.
NET_ERROR(SSL_BAD_RECORD_MAC_ALERT, -126)

// The proxy requested authentication (for tunnel establishment).
NET_ERROR(PROXY_AUTH_REQUESTED, -127)

// A known TLS strict server didn't offer the renegotiation extension.
NET_ERROR(SSL_UNSAFE_NEGOTIATION, -128)

// The SSL server attempted to use a weak ephemeral Diffie-Hellman key.
NET_ERROR(SSL_WEAK_SERVER_EPHEMERAL_DH_KEY, -129)

// Could not create a connection to the proxy server. An error occurred
// either in resolving its name, or in connecting a socket to it.
// Note that this does NOT include failures during the actual "CONNECT" method
// of an HTTP proxy.
NET_ERROR(PROXY_CONNECTION_FAILED, -130)

// A mandatory proxy configuration could not be used. Currently this means
// that a mandatory PAC script could not be fetched, parsed or executed.
NET_ERROR(MANDATORY_PROXY_CONFIGURATION_FAILED, -131)

// -132 was formerly ERR_ESET_ANTI_VIRUS_SSL_INTERCEPTION

// We've hit the max socket limit for the socket pool while preconnecting.  We
// don't bother trying to preconnect more sockets.
NET_ERROR(PRECONNECT_MAX_SOCKET_LIMIT, -133)

// The permission to use the SSL client certificate's private key was denied.
NET_ERROR(SSL_CLIENT_AUTH_PRIVATE_KEY_ACCESS_DENIED, -134)

// The SSL client certificate has no private key.
NET_ERROR(SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY, -135)

// The certificate presented by the HTTPS Proxy was invalid.
NET_ERROR(PROXY_CERTIFICATE_INVALID, -136)

// An error occurred when trying to do a name resolution (DNS).
NET_ERROR(NAME_RESOLUTION_FAILED, -137)

// Permission to access the network was denied. This is used to distinguish
// errors that were most likely caused by a firewall from other access denied
// errors. See also ERR_ACCESS_DENIED.
NET_ERROR(NETWORK_ACCESS_DENIED, -138)

// The request throttler module cancelled this request to avoid DDOS.
NET_ERROR(TEMPORARILY_THROTTLED, -139)

// A request to create an SSL tunnel connection through the HTTPS proxy
// received a non-200 (OK) and non-407 (Proxy Auth) response.  The response
// body might include a description of why the request failed.
NET_ERROR(HTTPS_PROXY_TUNNEL_RESPONSE, -140)

// We were unable to sign the CertificateVerify data of an SSL client auth
// handshake with the client certificate's private key.
//
// Possible causes for this include the user implicitly or explicitly
// denying access to the private key, the private key may not be valid for
// signing, the key may be relying on a cached handle which is no longer
// valid, or the CSP won't allow arbitrary data to be signed.
NET_ERROR(SSL_CLIENT_AUTH_SIGNATURE_FAILED, -141)

// The message was too large for the transport.  (for example a UDP message
// which exceeds size threshold).
NET_ERROR(MSG_TOO_BIG, -142)

// A SPDY session already exists, and should be used instead of this connection.
NET_ERROR(SPDY_SESSION_ALREADY_EXISTS, -143)

// Error -144 was removed (LIMIT_VIOLATION).

// Error -145 was removed (WS_PROTOCOL_ERROR).

// Connection was aborted for switching to another ptotocol.
// WebSocket abort SocketStream connection when alternate protocol is found.
NET_ERROR(PROTOCOL_SWITCHED, -146)

// Returned when attempting to bind an address that is already in use.
NET_ERROR(ADDRESS_IN_USE, -147)

// An operation failed because the SSL handshake has not completed.
NET_ERROR(SSL_HANDSHAKE_NOT_COMPLETED, -148)

// SSL peer's public key is invalid.
NET_ERROR(SSL_BAD_PEER_PUBLIC_KEY, -149)

// The certificate didn't match the built-in public key pins for the host name.
// The pins are set in net/base/transport_security_state.cc and require that
// one of a set of public keys exist on the path from the leaf to the root.
NET_ERROR(SSL_PINNED_KEY_NOT_IN_CERT_CHAIN, -150)

// Server request for client certificate did not contain any types we support.
NET_ERROR(CLIENT_AUTH_CERT_TYPE_UNSUPPORTED, -151)

// Server requested one type of cert, then requested a different type while the
// first was still being generated.
NET_ERROR(ORIGIN_BOUND_CERT_GENERATION_TYPE_MISMATCH, -152)

// Certificate error codes
//
// The values of certificate error codes must be consecutive.

// The server responded with a certificate whose common name did not match
// the host name.  This could mean:
//
// 1. An attacker has redirected our traffic to his server and is
//    presenting a certificate for which he knows the private key.
//
// 2. The server is misconfigured and responding with the wrong cert.
//
// 3. The user is on a wireless network and is being redirected to the
//    network's login page.
//
// 4. The OS has used a DNS search suffix and the server doesn't have
//    a certificate for the abbreviated name in the address bar.
//
NET_ERROR(CERT_COMMON_NAME_INVALID, -200)

// The server responded with a certificate that, by our clock, appears to
// either not yet be valid or to have expired.  This could mean:
//
// 1. An attacker is presenting an old certificate for which he has
//    managed to obtain the private key.
//
// 2. The server is misconfigured and is not presenting a valid cert.
//
// 3. Our clock is wrong.
//
NET_ERROR(CERT_DATE_INVALID, -201)

// The server responded with a certificate that is signed by an authority
// we don't trust.  The could mean:
//
// 1. An attacker has substituted the real certificate for a cert that
//    contains his public key and is signed by his cousin.
//
// 2. The server operator has a legitimate certificate from a CA we don't
//    know about, but should trust.
//
// 3. The server is presenting a self-signed certificate, providing no
//    defense against active attackers (but foiling passive attackers).
//
NET_ERROR(CERT_AUTHORITY_INVALID, -202)

// The server responded with a certificate that contains errors.
// This error is not recoverable.
//
// MSDN describes this error as follows:
//   "The SSL certificate contains errors."
// NOTE: It's unclear how this differs from ERR_CERT_INVALID. For consistency,
// use that code instead of this one from now on.
//
NET_ERROR(CERT_CONTAINS_ERRORS, -203)

// The certificate has no mechanism for determining if it is revoked.  In
// effect, this certificate cannot be revoked.
NET_ERROR(CERT_NO_REVOCATION_MECHANISM, -204)

// Revocation information for the security certificate for this site is not
// available.  This could mean:
//
// 1. An attacker has compromised the private key in the certificate and is
//    blocking our attempt to find out that the cert was revoked.
//
// 2. The certificate is unrevoked, but the revocation server is busy or
//    unavailable.
//
NET_ERROR(CERT_UNABLE_TO_CHECK_REVOCATION, -205)

// The server responded with a certificate has been revoked.
// We have the capability to ignore this error, but it is probably not the
// thing to do.
NET_ERROR(CERT_REVOKED, -206)

// The server responded with a certificate that is invalid.
// This error is not recoverable.
//
// MSDN describes this error as follows:
//   "The SSL certificate is invalid."
//
NET_ERROR(CERT_INVALID, -207)

// The server responded with a certificate that is signed using a weak
// signature algorithm.
NET_ERROR(CERT_WEAK_SIGNATURE_ALGORITHM, -208)

// -209 is availible: was CERT_NOT_IN_DNS.

// The host name specified in the certificate is not unique.
NET_ERROR(CERT_NON_UNIQUE_NAME, -210)

// The server responded with a certificate that contains a weak key (e.g.
// a too-small RSA key).
NET_ERROR(CERT_WEAK_KEY, -211)

// Add new certificate error codes here.
//
// Update the value of CERT_END whenever you add a new certificate error
// code.

// The value immediately past the last certificate error code.
NET_ERROR(CERT_END, -212)

// The URL is invalid.
NET_ERROR(INVALID_URL, -300)

// The scheme of the URL is disallowed.
NET_ERROR(DISALLOWED_URL_SCHEME, -301)

// The scheme of the URL is unknown.
NET_ERROR(UNKNOWN_URL_SCHEME, -302)

// Attempting to load an URL resulted in too many redirects.
NET_ERROR(TOO_MANY_REDIRECTS, -310)

// Attempting to load an URL resulted in an unsafe redirect (e.g., a redirect
// to file:// is considered unsafe).
NET_ERROR(UNSAFE_REDIRECT, -311)

// Attempting to load an URL with an unsafe port number.  These are port
// numbers that correspond to services, which are not robust to spurious input
// that may be constructed as a result of an allowed web construct (e.g., HTTP
// looks a lot like SMTP, so form submission to port 25 is denied).
NET_ERROR(UNSAFE_PORT, -312)

// The server's response was invalid.
NET_ERROR(INVALID_RESPONSE, -320)

// Error in chunked transfer encoding.
NET_ERROR(INVALID_CHUNKED_ENCODING, -321)

// The server did not support the request method.
NET_ERROR(METHOD_NOT_SUPPORTED, -322)

// The response was 407 (Proxy Authentication Required), yet we did not send
// the request to a proxy.
NET_ERROR(UNEXPECTED_PROXY_AUTH, -323)

// The server closed the connection without sending any data.
NET_ERROR(EMPTY_RESPONSE, -324)

// The headers section of the response is too large.
NET_ERROR(RESPONSE_HEADERS_TOO_BIG, -325)

// The PAC requested by HTTP did not have a valid status code (non-200).
NET_ERROR(PAC_STATUS_NOT_OK, -326)

// The evaluation of the PAC script failed.
NET_ERROR(PAC_SCRIPT_FAILED, -327)

// The response was 416 (Requested range not satisfiable) and the server cannot
// satisfy the range requested.
NET_ERROR(REQUEST_RANGE_NOT_SATISFIABLE, -328)

// The identity used for authentication is invalid.
NET_ERROR(MALFORMED_IDENTITY, -329)

// Content decoding of the response body failed.
NET_ERROR(CONTENT_DECODING_FAILED, -330)

// An operation could not be completed because all network IO
// is suspended.
NET_ERROR(NETWORK_IO_SUSPENDED, -331)

// FLIP data received without receiving a SYN_REPLY on the stream.
NET_ERROR(SYN_REPLY_NOT_RECEIVED, -332)

// Converting the response to target encoding failed.
NET_ERROR(ENCODING_CONVERSION_FAILED, -333)

// The server sent an FTP directory listing in a format we do not understand.
NET_ERROR(UNRECOGNIZED_FTP_DIRECTORY_LISTING_FORMAT, -334)

// Attempted use of an unknown SPDY stream id.
NET_ERROR(INVALID_SPDY_STREAM, -335)

// There are no supported proxies in the provided list.
NET_ERROR(NO_SUPPORTED_PROXIES, -336)

// There is a SPDY protocol framing error.
NET_ERROR(SPDY_PROTOCOL_ERROR, -337)

// Credentials could not be established during HTTP Authentication.
NET_ERROR(INVALID_AUTH_CREDENTIALS, -338)

// An HTTP Authentication scheme was tried which is not supported on this
// machine.
NET_ERROR(UNSUPPORTED_AUTH_SCHEME, -339)

// Detecting the encoding of the response failed.
NET_ERROR(ENCODING_DETECTION_FAILED, -340)

// (GSSAPI) No Kerberos credentials were available during HTTP Authentication.
NET_ERROR(MISSING_AUTH_CREDENTIALS, -341)

// An unexpected, but documented, SSPI or GSSAPI status code was returned.
NET_ERROR(UNEXPECTED_SECURITY_LIBRARY_STATUS, -342)

// The environment was not set up correctly for authentication (for
// example, no KDC could be found or the principal is unknown.
NET_ERROR(MISCONFIGURED_AUTH_ENVIRONMENT, -343)

// An undocumented SSPI or GSSAPI status code was returned.
NET_ERROR(UNDOCUMENTED_SECURITY_LIBRARY_STATUS, -344)

// The HTTP response was too big to drain.
NET_ERROR(RESPONSE_BODY_TOO_BIG_TO_DRAIN, -345)

// The HTTP response contained multiple distinct Content-Length headers.
NET_ERROR(RESPONSE_HEADERS_MULTIPLE_CONTENT_LENGTH, -346)

// SPDY Headers have been received, but not all of them - status or version
// headers are missing, so we're expecting additional frames to complete them.
NET_ERROR(INCOMPLETE_SPDY_HEADERS, -347)

// No PAC URL configuration could be retrieved from DHCP. This can indicate
// either a failure to retrieve the DHCP configuration, or that there was no
// PAC URL configured in DHCP.
NET_ERROR(PAC_NOT_IN_DHCP, -348)

// The HTTP response contained multiple Content-Disposition headers.
NET_ERROR(RESPONSE_HEADERS_MULTIPLE_CONTENT_DISPOSITION, -349)

// The HTTP response contained multiple Location headers.
NET_ERROR(RESPONSE_HEADERS_MULTIPLE_LOCATION, -350)

// SPDY server refused the stream. Client should retry. This should never be a
// user-visible error.
NET_ERROR(SPDY_SERVER_REFUSED_STREAM, -351)

// SPDY server didn't respond to the PING message.
NET_ERROR(SPDY_PING_FAILED, -352)

// The request couldn't be completed on an HTTP pipeline. Client should retry.
NET_ERROR(PIPELINE_EVICTION, -353)

// The HTTP response body transferred fewer bytes than were advertised by the
// Content-Length header when the connection is closed.
NET_ERROR(CONTENT_LENGTH_MISMATCH, -354)

// The HTTP response body is transferred with Chunked-Encoding, but the
// terminating zero-length chunk was never sent when the connection is closed.
NET_ERROR(INCOMPLETE_CHUNKED_ENCODING, -355)

// The cache does not have the requested entry.
NET_ERROR(CACHE_MISS, -400)

// Unable to read from the disk cache.
NET_ERROR(CACHE_READ_FAILURE, -401)

// Unable to write to the disk cache.
NET_ERROR(CACHE_WRITE_FAILURE, -402)

// The operation is not supported for this entry.
NET_ERROR(CACHE_OPERATION_NOT_SUPPORTED, -403)

// The disk cache is unable to open this entry.
NET_ERROR(CACHE_OPEN_FAILURE, -404)

// The disk cache is unable to create this entry.
NET_ERROR(CACHE_CREATE_FAILURE, -405)

// Multiple transactions are racing to create disk cache entries. This is an
// internal error returned from the HttpCache to the HttpCacheTransaction that
// tells the transaction to restart the entry-creation logic because the state
// of the cache has changed.
NET_ERROR(CACHE_RACE, -406)

// The server's response was insecure (e.g. there was a cert error).
NET_ERROR(INSECURE_RESPONSE, -501)

// The server responded to a <keygen> with a generated client cert that we
// don't have the matching private key for.
NET_ERROR(NO_PRIVATE_KEY_FOR_CERT, -502)

// An error adding to the OS certificate database (e.g. OS X Keychain).
NET_ERROR(ADD_USER_CERT_FAILED, -503)

// *** Code -600 is reserved (was FTP_PASV_COMMAND_FAILED). ***

// A generic error for failed FTP control connection command.
// If possible, please use or add a more specific error code.
NET_ERROR(FTP_FAILED, -601)

// The server cannot fulfill the request at this point. This is a temporary
// error.
// FTP response code 421.
NET_ERROR(FTP_SERVICE_UNAVAILABLE, -602)

// The server has aborted the transfer.
// FTP response code 426.
NET_ERROR(FTP_TRANSFER_ABORTED, -603)

// The file is busy, or some other temporary error condition on opening
// the file.
// FTP response code 450.
NET_ERROR(FTP_FILE_BUSY, -604)

// Server rejected our command because of syntax errors.
// FTP response codes 500, 501.
NET_ERROR(FTP_SYNTAX_ERROR, -605)

// Server does not support the command we issued.
// FTP response codes 502, 504.
NET_ERROR(FTP_COMMAND_NOT_SUPPORTED, -606)

// Server rejected our command because we didn't issue the commands in right
// order.
// FTP response code 503.
NET_ERROR(FTP_BAD_COMMAND_SEQUENCE, -607)

// PKCS #12 import failed due to incorrect password.
NET_ERROR(PKCS12_IMPORT_BAD_PASSWORD, -701)

// PKCS #12 import failed due to other error.
NET_ERROR(PKCS12_IMPORT_FAILED, -702)

// CA import failed - not a CA cert.
NET_ERROR(IMPORT_CA_CERT_NOT_CA, -703)

// Import failed - certificate already exists in database.
// Note it's a little weird this is an error but reimporting a PKCS12 is ok
// (no-op).  That's how Mozilla does it, though.
NET_ERROR(IMPORT_CERT_ALREADY_EXISTS, -704)

// CA import failed due to some other error.
NET_ERROR(IMPORT_CA_CERT_FAILED, -705)

// Server certificate import failed due to some internal error.
NET_ERROR(IMPORT_SERVER_CERT_FAILED, -706)

// PKCS #12 import failed due to invalid MAC.
NET_ERROR(PKCS12_IMPORT_INVALID_MAC, -707)

// PKCS #12 import failed due to invalid/corrupt file.
NET_ERROR(PKCS12_IMPORT_INVALID_FILE, -708)

// PKCS #12 import failed due to unsupported features.
NET_ERROR(PKCS12_IMPORT_UNSUPPORTED, -709)

// Key generation failed.
NET_ERROR(KEY_GENERATION_FAILED, -710)

// Server-bound certificate generation failed.
NET_ERROR(ORIGIN_BOUND_CERT_GENERATION_FAILED, -711)

// Failure to export private key.
NET_ERROR(PRIVATE_KEY_EXPORT_FAILED, -712)

// DNS error codes.

// DNS resolver received a malformed response.
NET_ERROR(DNS_MALFORMED_RESPONSE, -800)

// DNS server requires TCP
NET_ERROR(DNS_SERVER_REQUIRES_TCP, -801)

// DNS server failed.  This error is returned for all of the following
// error conditions:
// 1 - Format error - The name server was unable to interpret the query.
// 2 - Server failure - The name server was unable to process this query
//     due to a problem with the name server.
// 4 - Not Implemented - The name server does not support the requested
//     kind of query.
// 5 - Refused - The name server refuses to perform the specified
//     operation for policy reasons.
NET_ERROR(DNS_SERVER_FAILED, -802)

// DNS transaction timed out.
NET_ERROR(DNS_TIMED_OUT, -803)

// The entry was not found in cache, for cache-only lookups.
NET_ERROR(DNS_CACHE_MISS, -804)

// Suffix search list rules prevent resolution of the given host name.
NET_ERROR(DNS_SEARCH_EMPTY, -805)

// Failed to sort addresses according to RFC3484.
NET_ERROR(DNS_SORT_ERROR, -806)
