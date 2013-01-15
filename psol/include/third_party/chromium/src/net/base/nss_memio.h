// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Written in NSPR style to also be suitable for adding to the NSS demo suite

#ifndef __MEMIO_H
#define __MEMIO_H
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "prio.h"

/* Opaque structure.  Really just a more typesafe alias for PRFilePrivate. */
struct memio_Private;
typedef struct memio_Private memio_Private;

/*----------------------------------------------------------------------
 NSPR I/O layer that terminates in a pair of circular buffers
 rather than talking to the real network.
 To use this with NSS:
 1) call memio_CreateIOLayer to create a fake NSPR socket
 2) call SSL_ImportFD to ssl-ify the socket
 3) Do your own networking calls to set up a TCP connection
 4) call memio_SetPeerName to tell NSS about the other end of the connection
 5) While at the same time doing plaintext nonblocking NSPR I/O as
    usual to the nspr file descriptor returned by SSL_ImportFD,
    your app must shuttle encrypted data between
    the real network and memio's network buffers.
    memio_GetReadParams/memio_PutReadResult
    are the hooks you need to pump data into memio's input buffer,
    and memio_GetWriteParams/memio_PutWriteResult
    are the hooks you need to pump data out of memio's output buffer.
----------------------------------------------------------------------*/

/* Create the I/O layer and its two circular buffers. */
PRFileDesc *memio_CreateIOLayer(int bufsize);

/* Must call before trying to make an ssl connection */
void memio_SetPeerName(PRFileDesc *fd, const PRNetAddr *peername);

/* Return a private pointer needed by the following
 * four functions.  (We could have passed a PRFileDesc to
 * them, but that would be slower.  Better for the caller
 * to grab the pointer once and cache it.
 * This may be a premature optimization.)
 */
memio_Private *memio_GetSecret(PRFileDesc *fd);

/* Ask memio where to put bytes from the network, and how many it can handle.
 * Returns bytes available to write, or 0 if none available.
 * Puts current buffer position into *buf.
 */
int memio_GetReadParams(memio_Private *secret, char **buf);

/* Tell memio how many bytes were read from the network.
 * If bytes_read is 0, causes EOF to be reported to
 * NSS after it reads the last byte from the circular buffer.
 * If bytes_read is < 0, it is treated as an NSPR error code.
 * See nspr/pr/src/md/unix/unix_errors.c for how to
 * map from Unix errors to NSPR error codes.
 * On EWOULDBLOCK or the equivalent, don't call this function.
 */
void memio_PutReadResult(memio_Private *secret, int bytes_read);

/* Ask memio what data it has to send to the network.
 * Returns up to two buffers of data by writing the positions and lengths into
 * |buf1|, |len1| and |buf2|, |len2|.
 */
void memio_GetWriteParams(memio_Private *secret,
                          const char **buf1, unsigned int *len1,
                          const char **buf2, unsigned int *len2);

/* Tell memio how many bytes were sent to the network.
 * If bytes_written is < 0, it is treated as an NSPR error code.
 * See nspr/pr/src/md/unix/unix_errors.c for how to
 * map from Unix errors to NSPR error codes.
 * On EWOULDBLOCK or the equivalent, don't call this function.
 */
void memio_PutWriteResult(memio_Private *secret, int bytes_written);


#ifdef __cplusplus
}
#endif

#endif
