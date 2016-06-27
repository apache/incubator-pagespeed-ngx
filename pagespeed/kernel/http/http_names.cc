/*
 * Copyright 2010 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "pagespeed/kernel/http/http_names.h"


#include "base/at_exit.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

const char HttpAttributes::kAccept[] = "Accept";
const char HttpAttributes::kAcceptEncoding[] = "Accept-Encoding";
const char HttpAttributes::kAcceptRanges[] = "Accept-Ranges";
const char HttpAttributes::kAccessControlAllowOrigin[] =
    "Access-Control-Allow-Origin";
const char HttpAttributes::kAccessControlAllowCredentials[] =
    "Access-Control-Allow-Credentials";
const char HttpAttributes::kAge[] = "Age";
const char HttpAttributes::kAllow[] = "Allow";
const char HttpAttributes::kAltSvc[] = "Alt-Svc";
const char HttpAttributes::kAlternateProtocol[] = "Alternate-Protocol";
const char HttpAttributes::kAttachment[] = "attachment";
const char HttpAttributes::kAuthorization[] = "Authorization";
const char HttpAttributes::kCacheControl[] = "Cache-Control";
const char HttpAttributes::kConnection[] = "Connection";
const char HttpAttributes::kContentDisposition[] = "Content-Disposition";
const char HttpAttributes::kContentEncoding[] = "Content-Encoding";
const char HttpAttributes::kContentLanguage[] = "Content-Language";
const char HttpAttributes::kContentLength[] = "Content-Length";
const char HttpAttributes::kContentType[] = "Content-Type";
const char HttpAttributes::kCookie[] = "Cookie";
const char HttpAttributes::kCookie2[] = "Cookie2";
const char HttpAttributes::kDate[] = "Date";
const char HttpAttributes::kDeflate[] = "deflate";
const char HttpAttributes::kDnt[] = "DNT";
const char HttpAttributes::kEtag[] = "Etag";
const char HttpAttributes::kExpires[] = "Expires";
const char HttpAttributes::kGzip[] = "gzip";
const char HttpAttributes::kHost[] = "Host";
const char HttpAttributes::kIfModifiedSince[] = "If-Modified-Since";
const char HttpAttributes::kIfNoneMatch[] = "If-None-Match";
const char HttpAttributes::kKeepAlive[] = "Keep-Alive";
const char HttpAttributes::kLastModified[] = "Last-Modified";
const char HttpAttributes::kLink[] = "Link";
const char HttpAttributes::kLocation[] = "Location";
const char HttpAttributes::kMaxAge[] = "max-age";
const char HttpAttributes::kNoCache[] = "no-cache";
const char HttpAttributes::kNoCacheMaxAge0[] = "max-age=0, no-cache";
const char HttpAttributes::kNoStore[] = "no-store";
const char HttpAttributes::kNosniff[] = "nosniff";
const char HttpAttributes::kOrigin[] = "Origin";
const char HttpAttributes::kPragma[] = "Pragma";
const char HttpAttributes::kPrivate[] = "private";
const char HttpAttributes::kProxyAuthenticate[] = "Proxy-Authenticate";
const char HttpAttributes::kProxyAuthorization[] = "Proxy-Authorization";
const char HttpAttributes::kPublic[] = "public";
const char HttpAttributes::kPurpose[] = "Purpose";
const char HttpAttributes::kReferer[] = "Referer";  // sic
const char HttpAttributes::kRefresh[] = "Refresh";
const char HttpAttributes::kSaveData[] = "Save-Data";
const char HttpAttributes::kServer[] = "Server";
const char HttpAttributes::kSetCookie2[] = "Set-Cookie2";
const char HttpAttributes::kSetCookie[] = "Set-Cookie";
const char HttpAttributes::kTE[] = "TE";
const char HttpAttributes::kTrailers[] = "Trailers";
const char HttpAttributes::kTransferEncoding[] = "Transfer-Encoding";
const char HttpAttributes::kUpgrade[] = "Upgrade";
const char HttpAttributes::kUserAgent[] = "User-Agent";
const char HttpAttributes::kVary[] = "Vary";
const char HttpAttributes::kVia[] = "Via";
const char HttpAttributes::kWarning[] = "Warning";
const char HttpAttributes::kXAssociatedContent[] = "X-Associated-Content";
const char HttpAttributes::kXContentTypeOptions[] = "X-Content-Type-Options";
const char HttpAttributes::kXForwardedFor[] = "X-Forwarded-For";
const char HttpAttributes::kXForwardedProto[] = "X-Forwarded-Proto";
const char HttpAttributes::kXmlHttpRequest[] = "XMLHttpRequest";
const char HttpAttributes::kXGooglePagespeedClientId[] =
    "X-Google-Pagespeed-Client-Id";
const char HttpAttributes::kXGoogleRequestEventId[] =
    "X-Google-Request-Event-Id";
const char HttpAttributes::kXOriginalContentLength[] =
    "X-Original-Content-Length";
const char HttpAttributes::kXPsaBlockingRewrite[] = "X-PSA-Blocking-Rewrite";
const char HttpAttributes::kXPsaBlockingRewriteMode[] =
    "X-PSA-Blocking-Rewrite-Mode";
const char HttpAttributes::kXPsaBlockingRewriteModeSlow[] = "slow";
// Warning: The header name is subject to change.
const char HttpAttributes::kXPsaClientOptions[] = "X-PSA-Client-Options";
const char HttpAttributes::kXPsaDistributedRewriteBlock[] =
    "X-PSA-Distributed-Rewrite-Block";
const char HttpAttributes::kXPsaDistributedRewriteFetch[] =
    "X-PSA-Distributed-Rewrite-Fetch";
const char HttpAttributes::kXPsaDistributedRewriteForHtml[] =
    "X-PSA-Distributed-Rewrite-Html";
const char HttpAttributes::kXPsaLoadShed[] = "X-Psa-Load-Shed";
const char HttpAttributes::kXPsaRequestMetadata[] =
    "X-PSA-Request-Metadata";
// TODO(jkarlin): Make sure (with tests) that this header does not leak beyond
// pagespeed servers.
const char HttpAttributes::kXPsaResponseMetadata[] =
    "X-PSA-Response-Metadata";
const char HttpAttributes::kXSplit[] = "x_split";
const char HttpAttributes::kXSplitAboveTheFold[] = "atf";
const char HttpAttributes::kXSplitBelowTheFold[] = "btf";
const char HttpAttributes::kXRequestedWith[] = "X-Requested-With";
const char HttpAttributes::kXUACompatible[] = "X-UA-Compatible";
const char HttpAttributes::kXPsaSplitConfig[] = "X-PSA-Split-Config";
const char HttpAttributes::kXSendfile[] = "X-Sendfile";
const char HttpAttributes::kXAccelRedirect[] = "X-Accel-Redirect";

const char* HttpStatus::GetReasonPhrase(HttpStatus::Code rc) {
  switch (rc) {
    case HttpStatus::kContinue                : return "Continue";
    case HttpStatus::kSwitchingProtocols      : return "Switching Protocols";

    case HttpStatus::kOK                      : return "OK";
    case HttpStatus::kCreated                 : return "Created";
    case HttpStatus::kAccepted                : return "Accepted";
    case HttpStatus::kNonAuthoritative        :
      return "Non-Authoritative Information";
    case HttpStatus::kNoContent               : return "No Content";
    case HttpStatus::kResetContent            : return "Reset Content";
    case HttpStatus::kPartialContent          : return "Partial Content";

      // 300 range: redirects
    case HttpStatus::kMultipleChoices         : return "Multiple Choices";
    case HttpStatus::kMovedPermanently        : return "Moved Permanently";
    case HttpStatus::kFound                   : return "Found";
    case HttpStatus::kSeeOther                : return "See Other";
    case HttpStatus::kNotModified             : return "Not Modified";
    case HttpStatus::kUseProxy                : return "Use Proxy";
    case HttpStatus::kTemporaryRedirect       : return "OK";

      // 400 range: client errors
    case HttpStatus::kBadRequest              : return "Bad Request";
    case HttpStatus::kUnauthorized            : return "Unauthorized";
    case HttpStatus::kPaymentRequired         : return "Payment Required";
    case HttpStatus::kForbidden               : return "Forbidden";
    case HttpStatus::kNotFound                : return "Not Found";
    case HttpStatus::kMethodNotAllowed        : return "Method Not Allowed";
    case HttpStatus::kNotAcceptable           : return "Not Acceptable";
    case HttpStatus::kProxyAuthRequired       :
      return "Proxy Authentication Required";
    case HttpStatus::kRequestTimeout          : return "Request Time-out";
    case HttpStatus::kConflict                : return "Conflict";
    case HttpStatus::kGone                    : return "Gone";
    case HttpStatus::kLengthRequired          : return "Length Required";
    case HttpStatus::kPreconditionFailed      : return "Precondition Failed";
    case HttpStatus::kEntityTooLarge          :
      return "Request Entity Too Large";
    case HttpStatus::kUriTooLong              : return "Request-URI Too Large";
    case HttpStatus::kUnsupportedMediaType    : return "Unsupported Media Type";
    case HttpStatus::kRangeNotSatisfiable     :
      return "Requested range not satisfiable";
    case HttpStatus::kExpectationFailed       : return "Expectation Failed";

      // 500 range: server errors
    case HttpStatus::kInternalServerError     : return "Internal Server Error";
    case HttpStatus::kNotImplemented          : return "Not Implemented";
    case HttpStatus::kBadGateway              : return "Bad Gateway";
    case HttpStatus::kUnavailable             : return "Service Unavailable";
    case HttpStatus::kGatewayTimeout          : return "Gateway Time-out";

     // Instaweb proxy failures
    case HttpStatus::kProxyPublisherFailure  : return "Proxy Publisher Failure";
    case HttpStatus::kProxyFailure             : return "Proxy Failure";
    case HttpStatus::kProxyConfigurationFailure: return "Proxy Config Failure";
    case HttpStatus::kProxyDeclinedRequest    : return "Proxy Declined Request";

    default:
      // We don't have a name for this response code, so we'll just
      // take the blame
      return "Internal Server Error";
  }
  return "";
}

namespace {

class EndToEndHeadersContainer {
 public:
  EndToEndHeadersContainer() {
    BuildEndToEnd();
    BuildHopByHop();
    BuildCachingHeadersToBeRemoved();
  }

  void BuildEndToEnd() {
    // See http://www.w3.org/Protocols/rfc2616/rfc2616-sec13.html#sec13.5.1
    const int kReserveSize = 36;
    int index = 0;
    StringPieceVector names(kReserveSize);

    names[index++] = StringPiece(HttpAttributes::kAccept);
    names[index++] = StringPiece(HttpAttributes::kAcceptEncoding);
    names[index++] = StringPiece(HttpAttributes::kAccessControlAllowOrigin);
    names[index++] =
        StringPiece(HttpAttributes::kAccessControlAllowCredentials);
    names[index++] = StringPiece(HttpAttributes::kAge);
    names[index++] = StringPiece(HttpAttributes::kAllow);
    names[index++] = StringPiece(HttpAttributes::kAuthorization);
    names[index++] = StringPiece(HttpAttributes::kCacheControl);
    names[index++] = StringPiece(HttpAttributes::kContentDisposition);
    names[index++] = StringPiece(HttpAttributes::kContentEncoding);
    names[index++] = StringPiece(HttpAttributes::kContentLanguage);
    names[index++] = StringPiece(HttpAttributes::kContentLength);
    names[index++] = StringPiece(HttpAttributes::kContentType);
    names[index++] = StringPiece(HttpAttributes::kCookie);
    names[index++] = StringPiece(HttpAttributes::kCookie2);
    names[index++] = StringPiece(HttpAttributes::kDate);
    names[index++] = StringPiece(HttpAttributes::kEtag);
    names[index++] = StringPiece(HttpAttributes::kExpires);
    names[index++] = StringPiece(HttpAttributes::kHost);
    names[index++] = StringPiece(HttpAttributes::kIfModifiedSince);
    names[index++] = StringPiece(HttpAttributes::kIfNoneMatch);
    names[index++] = StringPiece(HttpAttributes::kLastModified);
    names[index++] = StringPiece(HttpAttributes::kLink);
    names[index++] = StringPiece(HttpAttributes::kLocation);
    names[index++] = StringPiece(HttpAttributes::kOrigin);
    names[index++] = StringPiece(HttpAttributes::kPragma);
    names[index++] = StringPiece(HttpAttributes::kPurpose);
    names[index++] = StringPiece(HttpAttributes::kReferer);
    names[index++] = StringPiece(HttpAttributes::kRefresh);
    names[index++] = StringPiece(HttpAttributes::kServer);
    names[index++] = StringPiece(HttpAttributes::kSetCookie2);
    names[index++] = StringPiece(HttpAttributes::kSetCookie);
    names[index++] = StringPiece(HttpAttributes::kUserAgent);
    names[index++] = StringPiece(HttpAttributes::kVary);
    names[index++] = StringPiece(HttpAttributes::kVia);
    names[index++] = StringPiece(HttpAttributes::kWarning);

    DCHECK_EQ(kReserveSize, index);
    end_to_end_ = names;
  }

  void BuildHopByHop() {
    // http://www.w3.org/Protocols/rfc2616/rfc2616-sec13.html
    const int kReserveSize = 12;
    int index = 0;
    StringPieceVector names(kReserveSize);

    // This exact mechanism of initializing the names is used because it allows
    // populating a StringPieceVector from locally defined string constants
    // without having runtime calls to strlen to find the length.
    names[index++] = StringPiece(HttpAttributes::kAltSvc);
    names[index++] = StringPiece(HttpAttributes::kAlternateProtocol);
    names[index++] = StringPiece(HttpAttributes::kConnection);
    names[index++] = StringPiece(HttpAttributes::kKeepAlive);
    names[index++] = StringPiece(HttpAttributes::kProxyAuthenticate);
    names[index++] = StringPiece(HttpAttributes::kProxyAuthorization);
    names[index++] = StringPiece(HttpAttributes::kSetCookie);
    names[index++] = StringPiece(HttpAttributes::kSetCookie2);
    names[index++] = StringPiece(HttpAttributes::kTE);
    names[index++] = StringPiece(HttpAttributes::kTrailers);
    names[index++] = StringPiece(HttpAttributes::kTransferEncoding);
    names[index++] = StringPiece(HttpAttributes::kUpgrade);
    DCHECK_EQ(kReserveSize, index);
    hop_by_hop_ = names;
  }

  void BuildCachingHeadersToBeRemoved() {
    // http://www.w3.org/Protocols/rfc2616/rfc2616-sec13.html
    const int kReserveSize = 3;
    int index = 0;
    StringPieceVector names(kReserveSize);

    // This exact mechanism of initializing the names is used because it allows
    // populating a StringPieceVector from locally defined string constants
    // without having runtime calls to strlen to find the length.
    names[index++] = StringPiece(HttpAttributes::kLastModified);
    names[index++] = StringPiece(HttpAttributes::kExpires);
    names[index++] = StringPiece(HttpAttributes::kEtag);
    DCHECK_EQ(kReserveSize, index);
    caching_headers_to_be_removed_ = names;
  }

  const StringPieceVector& end_to_end() const {
    return end_to_end_;
  }

  const StringPieceVector& hop_by_hop() const {
    return hop_by_hop_;
  }

  const StringPieceVector& caching_headers_to_be_removed() const {
    return caching_headers_to_be_removed_;
  }

 private:
  StringPieceVector end_to_end_;
  StringPieceVector hop_by_hop_;
  StringPieceVector caching_headers_to_be_removed_;
};


base::LazyInstance<EndToEndHeadersContainer>
    headers_container = LAZY_INSTANCE_INITIALIZER;

const EndToEndHeadersContainer& get_headers_container() {
  return headers_container.Get();
}

}  // namespace


const StringPieceVector& HttpAttributes::SortedEndToEndHeaders() {
  return get_headers_container().end_to_end();
}

const StringPieceVector& HttpAttributes::SortedHopByHopHeaders() {
  return get_headers_container().hop_by_hop();
}

const StringPieceVector& HttpAttributes::CachingHeadersToBeRemoved() {
  return get_headers_container().caching_headers_to_be_removed();
}

}  // namespace net_instaweb
