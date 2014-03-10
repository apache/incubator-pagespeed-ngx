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
// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_URL_ENCODER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_URL_ENCODER_H_

#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest_prod.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_segment_encoder.h"

namespace net_instaweb {

class GoogleUrl;
class RequestProperties;
class RewriteDriver;
class RewriteOptions;
class MessageHandler;

// This class implements the encoding of image urls with optional additional
// dimension metadata.  It basically prepends characters indicating image
// dimensions on the page, webp eligibility, and mobile user agent eligibility
// (this information is conveyed in the ResourceContext).
//   http://...path.../50x75xurl...  No webp, image is 50x75 on page
//   http://...path.../50x75wurl...  Webp requested, image is 50x75 on page
//   http://...path.../50x75mxurl...
//       No webp, for mobile user agent, image is 50x75 on page
//   http://...path.../50x75mwurl...
//       Webp requested, for mobile user agent, image is 50x75 on page
//   http://...path.../50xNxurl..    No webp, image is 50 wide, no height given
//   http://...path.../50xNwurl...   Webp, image is 50 wide, no height given
//   http://...path.../Nx75xurl...   No webp, image is 75 high, no width given
//   http://...path.../Nx75wurl...   Webp, image is 75 high, no width given.
//   http://...path.../50xNmxurl..   No webp, image is 50 wide, mobile
//   http://...path.../50xNmwurl...  Webp, image is 50 wide, mobile
//   http://...path.../Nx75mxurl...  No webp, image is 75 high, mobile
//   http://...path.../Nx75mwurl...  Webp, image is 75 high, mobile
//   http://...path.../xurl...  Page does not specify both dimensions.  No webp.
//   http://...path.../wurl...  Webp requested, page missing dimensions.
//   http://...path.../xurl...  Page does not specify any dimension.  No webp.
//   http://...path.../wurl...  Webp requested, page missing either dimension.
//   http://...path.../mxurl...
//       No webp, for mobile user agent, page does not specify dimensions.
//   http://...path.../mwurl...
//       Webp requested, for mobile user agent, page missing dimensions.
class ImageUrlEncoder : public UrlSegmentEncoder {
 public:
  // Area threshold that determines whether we use
  // Image*RecompressionQualityForSmallScreens to set WebP/Jpeg quality.
  // The intent is for phones and small tablets (e.g., Nexus 7) to be in.
  static const int kSmallScreenSizeThresholdArea;

  ImageUrlEncoder() {}
  virtual ~ImageUrlEncoder();

  virtual void Encode(const StringVector& urls,
                      const ResourceContext* dim,
                      GoogleString* rewritten_url) const;

  virtual bool Decode(const StringPiece& url_segment,
                      StringVector* urls,
                      ResourceContext* dim,
                      MessageHandler* handler) const;

  // Set LibWebp level according to the user agent.
  // TODO(poojatandon): Pass a user agent object with its webp-cabaple bits
  // pre-analyzed (not just the string from the request headers), since
  // checking webp level related code doesn't belong here.
  static void SetLibWebpLevel(const RewriteOptions& options,
                              const RequestProperties& request_properties,
                              ResourceContext* resource_context);

  // Sets webp and mobile capability in resource context.
  //
  // The parameters to this method are urls, rewrite options & resource context.
  // Since rewrite options are not changed, we have passed const reference and
  // resource context is modified and can be NULL, hence we pass as a pointer.
  static void SetWebpAndMobileUserAgent(const RewriteDriver& driver,
                                        ResourceContext* context);

  // Determines whether the given URL is a pagespeed-rewritten webp URL.
  static bool IsWebpRewrittenUrl(const GoogleUrl& gurl);

  // Flag whether this device has a small screen, which determines what
  // Jpeg/WebP quality to use.
  static void SetSmallScreen(const RewriteDriver& driver,
                             ResourceContext* context);

  // Set context for screen resolution.
  static void SetUserAgentScreenResolution(
      RewriteDriver* driver, ResourceContext* context);

  // Helper function to generate Metadata cache key from ResourceContext.
  static GoogleString CacheKeyFromResourceContext(
      const ResourceContext& resource_context);

  static bool HasDimensions(const ResourceContext& data) {
    return (data.has_desired_image_dims() &&
            HasValidDimensions(data.desired_image_dims()));
  }

  static bool HasValidDimensions(const ImageDim& dims) {
    return (dims.has_width() && dims.has_height());
  }

  static bool HasDimension(const ResourceContext& data) {
    return (data.has_desired_image_dims() &&
            HasValidDimension(data.desired_image_dims()));
  }

  static bool HasValidDimension(const ImageDim& dims) {
    return (dims.has_width() || dims.has_height());
  }

 private:
  FRIEND_TEST(ImageRewriteTest, SquashImagesForMobileScreen);
  FRIEND_TEST(ImageUrlEncoderTest, UserAgentScreenResolution);

  // Returns true if screen width and height are normalized according to a
  // predefined list of screen resolutions (see implementation header document
  // for more details).
  static bool GetNormalizedScreenResolution(
      int screen_width, int screen_height, int* normalized_width,
      int* normalized_height);

  DISALLOW_COPY_AND_ASSIGN(ImageUrlEncoder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_URL_ENCODER_H_
