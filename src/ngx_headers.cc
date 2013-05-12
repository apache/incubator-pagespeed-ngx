
#include "ngx_pagespeed.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/request_headers.h"

namespace ngx_psol {
// modify from NgxBaseFetch::CopyHeadersFromTable()
namespace {
template<class Headers>
void copy_headers_from_table(const ngx_list_t &from, Headers* to) {
  // Standard nginx idiom for iterating over a list.  See ngx_list.h
  ngx_uint_t i;
  const ngx_list_part_t* part = &from.part;
  const ngx_table_elt_t* header = static_cast<ngx_table_elt_t*>(part->elts);

  for (i = 0 ; /* void */; i++) {
    if (i >= part->nelts) {
      if (part->next == NULL) {
        break;
      }

      part = part->next;
      header = static_cast<ngx_table_elt_t*>(part->elts);
      i = 0;
    }

    StringPiece key = ngx_psol::str_to_string_piece(header[i].key);
    StringPiece value = ngx_psol::str_to_string_piece(header[i].value);

    to->Add(key, value);
  }
}
}  // namespace

// modify from NgxBaseFetch::PopulateResponseHeaders()
void copy_response_headers_from_ngx(const ngx_http_request_t *r,
        net_instaweb::ResponseHeaders *headers) {
  headers->set_major_version(r->http_version / 1000);
  headers->set_minor_version(r->http_version % 1000);
  copy_headers_from_table(r->headers_out.headers, headers);

  headers->set_status_code(r->headers_out.status);

  // Manually copy over the content type because it's not included in
  // request_->headers_out.headers.
  headers->Add(net_instaweb::HttpAttributes::kContentType,
              ngx_psol::str_to_string_piece(r->headers_out.content_type));

  // TODO(oschaaf): ComputeCaching should be called in setupforhtml()?
  headers->ComputeCaching();
}

// modify from NgxBaseFetch::PopulateRequestHeaders()
void copy_request_headers_from_ngx(const ngx_http_request_t *r,
                                   net_instaweb::RequestHeaders *headers) {
  // TODO(chaizhenhua): only allow RewriteDriver::kPassThroughRequestAttributes?
  headers->set_major_version(r->http_version / 1000);
  headers->set_minor_version(r->http_version % 1000);
  copy_headers_from_table(r->headers_in.headers, headers);
}


ngx_int_t copy_response_headers_to_ngx(
    ngx_http_request_t* r,
    const net_instaweb::ResponseHeaders& pagespeed_headers) {
  ngx_http_headers_out_t* headers_out = &r->headers_out;
  headers_out->status = pagespeed_headers.status_code();

  ngx_int_t i;
  for (i = 0 ; i < pagespeed_headers.NumAttributes() ; i++) {
    const GoogleString& name_gs = pagespeed_headers.Name(i);
    const GoogleString& value_gs = pagespeed_headers.Value(i);

    ngx_str_t name, value;
    name.len = name_gs.length();
    name.data = reinterpret_cast<u_char*>(const_cast<char*>(name_gs.data()));
    value.len = value_gs.length();
    value.data = reinterpret_cast<u_char*>(const_cast<char*>(value_gs.data()));

    // TODO(jefftk): If we're setting a cache control header we'd like to
    // prevent any downstream code from changing it.  Specifically, if we're
    // serving a cache-extended resource the url will change if the resource
    // does and so we've given it a long lifetime.  If the site owner has done
    // something like set all css files to a 10-minute cache lifetime, that
    // shouldn't apply to our generated resources.  See Apache code in
    // net/instaweb/apache/header_util:AddResponseHeadersToRequest

    // Make copies of name and value to put into headers_out.

    u_char* value_s = ngx_pstrdup(r->pool, &value);
    if (value_s == NULL) {
      return NGX_ERROR;
    }

    if (STR_EQ_LITERAL(name, "Content-Type")) {
      // Unlike all the other headers, content_type is just a string.
      headers_out->content_type.data = value_s;
      headers_out->content_type.len = value.len;
      headers_out->content_type_len = value.len;
      // In ngx_http_test_content_type() nginx will allocate and calculate
      // content_type_lowcase if we leave it as null.
      headers_out->content_type_lowcase = NULL;
      continue;
    }

    u_char* name_s = ngx_pstrdup(r->pool, &name);
    if (name_s == NULL) {
      return NGX_ERROR;
    }

    ngx_table_elt_t* header = static_cast<ngx_table_elt_t*>(
        ngx_list_push(&headers_out->headers));
    if (header == NULL) {
      return NGX_ERROR;
    }

    header->hash = 1;  // Include this header in the output.
    header->key.len = name.len;
    header->key.data = name_s;
    header->value.len = value.len;
    header->value.data = value_s;

    // Populate the shortcuts to commonly used headers.
    if (STR_EQ_LITERAL(name, "Date")) {
      headers_out->date = header;
    } else if (STR_EQ_LITERAL(name, "Etag")) {
      headers_out->etag = header;
    } else if (STR_EQ_LITERAL(name, "Expires")) {
      headers_out->expires = header;
    } else if (STR_EQ_LITERAL(name, "Last-Modified")) {
      headers_out->last_modified = header;
    } else if (STR_EQ_LITERAL(name, "Location")) {
      headers_out->location = header;
    } else if (STR_EQ_LITERAL(name, "Server")) {
      headers_out->server = header;
    }
  }

  return NGX_OK;
}

}  // namespace ngx_psol
