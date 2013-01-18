/*
 * Copyright 2012 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: morlovich@google.com (Maksim Orlovich)
//
// A fetcher that talks to mod_spdy for requests matching a certain
// domain (and passes the rest to fallthrough fetcher).
//
// Based in large part on mod_spdy's http_to_spdy_filter.cc and
// spdy_to_http_filter.cc

#include "net/instaweb/apache/mod_spdy_fetcher.h"

#include "util_filter.h"

#include <algorithm>
#include <cstddef>

#include "base/logging.h"
#include "net/instaweb/apache/interface_mod_spdy.h"
#include "net/instaweb/apache/mod_spdy_fetch_controller.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/http_response_parser.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

struct spdy_slave_connection;

namespace net_instaweb {

namespace {

ap_filter_rec_t* apache_to_mps_filter_handle = NULL;
ap_filter_rec_t* mps_to_apache_filter_handle = NULL;

struct MpsToApacheFilterContext {
  MpsToApacheFilterContext(const GoogleString url,
                           AsyncFetch* in_fetch,
                           MessageHandler* in_handler)
      : handler(in_handler),
        request_headers(in_fetch->request_headers()),
        message_body(request_headers->message_body()),
        pos(0),
        in_body(false) {
    StringWriter writer(&request_str);
    request_headers->WriteAsHttp(url, &writer, handler);
  }

  MessageHandler* handler;
  const RequestHeaders* request_headers;
  GoogleString request_str;  // request line + headers

  // We have to copy the message body since the AsyncFetch might actually
  // be deleted by ::Done while our filter is still active. This is not worth
  // optimizing since we do not actually use POST with our fetcher under
  // normal conditions with MPS.
  GoogleString message_body;
  size_t pos;
  bool in_body;
};

// This takes an HTTP request information (from an MpsToApacheFilterContext)
// and generates appropriate bucket brigade for it.
apr_status_t MpsToApacheFilter(ap_filter_t* filter,
                               apr_bucket_brigade* brigade,
                               ap_input_mode_t mode,
                               apr_read_type_e block,
                               apr_off_t readbytes) {
  const size_t max_bytes = std::max(static_cast<apr_off_t>(1), readbytes);

  if (filter->next != NULL) {
    LOG(WARNING) << "MpsToApacheFilter is not the last filter in the chain: "
                 << filter->next->frec->name;
  }

  MpsToApacheFilterContext* context =
      static_cast<MpsToApacheFilterContext*>(filter->ctx);

  if (!context) {
    return APR_EOF;
  }

  if (mode == AP_MODE_INIT) {
    return APR_SUCCESS;
  }

  // Switch over to body if we're done with headers.
  if (!context->in_body &&
      context->pos >= context->request_str.size()) {
    context->in_body = true;
    context->pos = 0;
  }

  // Which string are we going to be reading from? (We don't actually
  // attempt to read from more than one).
  const GoogleString& in = context->in_body ?
                               context->message_body :
                               context->request_str;


  // Detect EOF. This is enough due to the "switch to body" bit above.
  if (context->pos >= in.size()) {
    return APR_EOF;
  }

  size_t bytes_read = 0;
  size_t bytes_available = in.size() - context->pos;

  // Synthesize an EOS bucket if needed.
  if ((bytes_read == bytes_available) &&
      (context->in_body || context->request_headers->message_body().empty())) {
    APR_BRIGADE_INSERT_TAIL(brigade, apr_bucket_eos_create(
        brigade->bucket_alloc));
    filter->ctx = NULL;
    delete context;
    return APR_SUCCESS;
  }

  const char* bytes = in.data() + context->pos;
  // Byte read ops.
  if (mode == AP_MODE_READBYTES || mode == AP_MODE_SPECULATIVE ||
      mode == AP_MODE_EXHAUSTIVE) {
    bytes_read = (mode == AP_MODE_EXHAUSTIVE) ?
                     bytes_available :
                     std::min(bytes_available, max_bytes);
  } else if (mode == AP_MODE_GETLINE) {
    StringPiece remaining(bytes, bytes_available);
    size_t eol_pos = remaining.find('\n');
    if (eol_pos != StringPiece::npos) {
      bytes_read = eol_pos + 1;
    } else {
      bytes_read = bytes_available;
    }
  } else {
    // Not doing AP_MODE_EATCRLF. See mod_spdy http_to_spdy_filter.cc
    // for why.
    DCHECK(mode == AP_MODE_EATCRLF);
    LOG(WARNING) << "Unsupported read mode" << mode;
    return APR_ENOTIMPL;
  }

  // If we managed to read any data, put it into the brigade.  We use a
  // transient bucket (as opposed to a heap bucket) to avoid an extra string
  // copy.
  if (bytes_read > 0) {
    APR_BRIGADE_INSERT_TAIL(brigade, apr_bucket_transient_create(
        bytes, bytes_read, brigade->bucket_alloc));
  }

  // Advance position on non-speculative reads.
  if (mode != AP_MODE_SPECULATIVE) {
    context->pos += bytes_read;
  }
  return APR_SUCCESS;
}

struct ApacheToMpsFilterContext {
  ApacheToMpsFilterContext(AsyncFetch* in_fetch, MessageHandler* in_handler)
      : target_fetch(in_fetch), handler(in_handler),
        response_parser(in_fetch->response_headers(), in_fetch, handler),
        ok(true) {}

  AsyncFetch* target_fetch;
  MessageHandler* handler;
  HttpResponseParser response_parser;
  bool ok;
};

// TODO(sligocki): Perhaps we can merge this with instaweb_in_place_filter().
apr_status_t ApacheToMpsFilter(ap_filter_t* filter,
                               apr_bucket_brigade* input_brigade) {
  // mod_spdy fed us some bits through Apache --- direct them to our client.

  // This is a NETWORK-level filter, so there shouldn't be any filter after us.
  if (filter->next != NULL) {
    LOG(WARNING) << "ApacheToMpsFilter is not the last filter in the chain "
                 << "(it is followed by " << filter->next->frec->name << ")";
  }

  // According to the page at
  //   http://httpd.apache.org/docs/2.3/developer/output-filters.html
  // we should never pass an empty brigade down the chain, but to be safe, we
  // should be prepared to accept one and do nothing.
  if (APR_BRIGADE_EMPTY(input_brigade)) {
    LOG(INFO) << "ApacheToMpsFilter received an empty brigade.";
    return APR_SUCCESS;
  }

  ApacheToMpsFilterContext* context =
      static_cast<ApacheToMpsFilterContext*>(filter->ctx);
  AsyncFetch* target = (context != NULL) ? context->target_fetch : NULL;

  while (!APR_BRIGADE_EMPTY(input_brigade)) {
    apr_bucket* bucket = APR_BRIGADE_FIRST(input_brigade);

    if (APR_BUCKET_IS_METADATA(bucket)) {
      if (context == NULL) {
        // Ignore metadata buckets after EOS.
      } else if (APR_BUCKET_IS_EOS(bucket)) {
        bool ok = context->ok;

        // EOS bucket -- there should be no more data buckets in this stream.
        // We denote this by dropping the context and zeroing out the pointer
        // to it in the filter.
        filter->ctx = NULL;
        delete context;

        target->Done(ok);
      } else if (APR_BUCKET_IS_FLUSH(bucket)) {
        target->Flush(context->handler);
      } else {
        // Unknown metadata bucket.  This bucket has no meaning to us, and
        // there's no further filter to pass it to, so we just ignore it.
      }
    } else if (context == NULL) {
      // We shouldn't be getting any data buckets after an EOS (since this is a
      // connection-level filter, we do sometimes see other metadata buckets
      // after the EOS).  If we do get them, ignore them.
      LOG(INFO) << "ApacheToMpsFilter received " << bucket->type->name
                << " bucket after an EOS (and ignored it).";
    } else {
      // Data bucket -- get ready to read.
      const char* data = NULL;
      apr_size_t data_length = 0;

      // First, try a non-blocking read.
      apr_status_t status = apr_bucket_read(bucket, &data, &data_length,
                                            APR_NONBLOCK_READ);

      if (status == APR_SUCCESS) {
        // All OK! (will write below)
      } else if (APR_STATUS_IS_EAGAIN(status)) {
        // Non-blocking read failed with EAGAIN, so try again with a blocking
        // read.
        status = apr_bucket_read(bucket, &data, &data_length, APR_BLOCK_READ);
        if (status != APR_SUCCESS) {
          LOG(ERROR) << "Blocking read failed with status " << status;
        }
      }

      // Send bytes to our client, if we're successful.
      if (status == APR_SUCCESS) {
        StringPiece piece(data, data_length);
        HttpResponseParser& response_parser = context->response_parser;

        bool had_headers = response_parser.headers_complete();
        if (response_parser.ParseChunk(piece)) {
          if (!had_headers && response_parser.headers_complete()) {
            target->HeadersComplete();
          }
        } else {
          context->ok = false;
        }
      } else {
        context->ok = false;
        // Since we didn't successfully consume this bucket, don't delete it;
        // rather, leave it (and any remaining buckets) in the brigade.
        return status;  // failure
      }
    }

    // We consumed this bucket successfully, so delete it and move on to the
    // next.
    apr_bucket_delete(bucket);
  }
  return APR_SUCCESS;
}

}  // namespace

void ModSpdyFetcher::Initialize() {
  mps_to_apache_filter_handle = ap_register_input_filter(
      "MOD_PAGESPEED_TO_MOD_SPDY",  // name
      MpsToApacheFilter,            // filter function
      NULL,                         // init function (n/a in our case)
      AP_FTYPE_NETWORK);            // filter type

  apache_to_mps_filter_handle = ap_register_output_filter(
      "MOD_SPDY_TO_MOD_PAGESPEED",  // name
      ApacheToMpsFilter,            // filter function
      NULL,                         // init function (n/a in our case)
      AP_FTYPE_NETWORK);          // filter type
}

ModSpdyFetcher::ModSpdyFetcher(ModSpdyFetchController* controller,
                               StringPiece url,
                               RewriteDriver* driver,
                               spdy_slave_connection_factory* factory)
    : controller_(controller),
      fallback_fetcher_(driver->async_fetcher()),
      connection_factory_(factory) {
  GoogleUrl gurl(url);
  if (gurl.is_valid()) {
    gurl.Origin().CopyToString(&own_origin_);
  }
}

ModSpdyFetcher::~ModSpdyFetcher() {
}

bool ModSpdyFetcher::ShouldUseOn(request_rec* req) {
  // We want to get involved for all HTTPS resources, so that any resources
  // we generate for SPDY clients can also be served to HTTPS clients safely.
  //
  // It's not sufficient to just check is_https, however, since in case the
  // top-level connection is SPDY, this will be a slave connection that will
  // have mod_ssl off.
  return mod_ssl_is_https(req->connection) ||
         (mod_spdy_get_spdy_version(req->connection) != 0);
}

void ModSpdyFetcher::Fetch(const GoogleString& url,
                           MessageHandler* message_handler,
                           AsyncFetch* fetch) {
  // Only fetch from mod_spdy if the hostname matches that of outgoing
  // connection, and if we have access to appropriate mod_spdy exports.
  GoogleUrl parsed_url(url);
  if (connection_factory_ != NULL &&
      parsed_url.is_valid() && !own_origin_.empty() &&
      parsed_url.Origin() == own_origin_) {
    controller_->ScheduleBlockingFetch(this, url, message_handler, fetch);
  } else {
    fallback_fetcher_->Fetch(url, message_handler, fetch);
  }
}

void ModSpdyFetcher::BlockingFetch(
    const GoogleString& url, MessageHandler* message_handler,
    AsyncFetch* fetch) {
  // These will normally be deleted by their filter functions
  // (but we do cleanup if something went wrong)
  MpsToApacheFilterContext* in_context =
      new MpsToApacheFilterContext(
          url, fetch, message_handler);
  ApacheToMpsFilterContext* out_context =
      new ApacheToMpsFilterContext(fetch, message_handler);
  spdy_slave_connection* slave_connection =
      mod_spdy_create_slave_connection(
          connection_factory_,
          mps_to_apache_filter_handle, in_context,
          apache_to_mps_filter_handle, out_context);
  if (slave_connection != NULL) {
    mod_spdy_run_slave_connection(slave_connection);
    mod_spdy_destroy_slave_connection(slave_connection);
    return;
  } else {
    delete in_context;
    delete out_context;
    fallback_fetcher_->Fetch(url, message_handler, fetch);
  }
}

}  // namespace net_instaweb
