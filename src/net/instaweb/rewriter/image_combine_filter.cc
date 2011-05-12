/*
 * Copyright 2011 Google Inc.
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

// Author: abliss@google.com (Adam Bliss)

#include "net/instaweb/rewriter/public/image_combine_filter.h"

#include <map>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/image.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_combiner.h"
#include "net/instaweb/rewriter/public/resource_combiner_template.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/spriter/image_library_interface.h"
#include "net/instaweb/spriter/public/image_spriter.h"
#include "net/instaweb/spriter/public/image_spriter.pb.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/identifier.h"
#include "webutil/css/parser.h"
#include "webutil/css/property.h"
#include "webutil/css/value.h"

namespace net_instaweb {
class RequestHeaders;
class ResponseHeaders;
class Writer;

namespace {

// names for Statistics variables.
const char kImageFileCountReduction[] = "image_file_count_reduction";

}  // namespace

// Unfortunately SpriteFuture can't be inside an anonymous namespace due to
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=29365, which is relevant to
// some of our supported open-source platforms.
namespace spriter_binding {

// A SpriteFuture keeps track of a single image that is to be sprited.  When
// constructed, it is in an invalid state and merely serves as a token for the
// partnership.
class SpriteFuture {
 public:
  // old_url is the original URL which will be replaced with the sprite.  We
  // keep track of it so that we can avoid putting the same image in the sprite
  // twice.
  explicit SpriteFuture(const StringPiece& old_url)
      : url_value_(NULL), x_value_(NULL), y_value_(NULL),
        declarations_(NULL), declaration_to_push_(NULL) {
    old_url.CopyToString(&old_url_);
    x_offset_ = 0;
    y_offset_ = 0;
  }

  ~SpriteFuture() {}

  // Bind this Future to a particular image.  Owns nothing; the inputs must
  // outlive this future.  Returns true if this is a viable sprite-future.  If
  // we return false, Realize must not be called.
  bool Initialize(Css::Declarations* declarations, Css::Value* url_value) {
    url_value_ = url_value;
    return FindBackgroundPositionValues(declarations);
  }

  const GoogleString& old_url() { return old_url_; }

  // TODO(abliss): support other values like "10%" and "center"
  static bool GetPixelValue(Css::Value* value, int* out_value_px) {
    switch (value->GetLexicalUnitType()) {
      case Css::Value::NUMBER: {
        int int_value = value->GetIntegerValue();
        // If the offset is specified in pixels, or is 0, we can just use it.
        if ((value->GetDimension() == Css::Value::PX) || (int_value == 0)) {
          *out_value_px = int_value;
          return true;
        }
        // TODO(abliss): handle more fancy values
        return false;
      }
      case  Css::Value::IDENT:
        switch (value->GetIdentifier().ident()) {
          case Css::Identifier::LEFT:
          case Css::Identifier::TOP:
            *out_value_px = 0;
            return true;
          default:
            return false;
        }
      default:
        return false;
    }
  }
  // Attempts to read the x and y values of the background position.  *values
  // is a value array which includes the background-position at values_offset.
  // new_x and new_y are the coordinates of the image in the sprite.  Returns
  // true, and sets up {x,y}_{value,offset}_ if successful.
  bool ReadBackgroundPosition(Css::Values* values, int values_offset) {
    // Parsing these values is trickier than you might think.  If either
    // of the two values is a non-center identifier, it determines which
    // is x and which is y.  So for example, "5px left" means x=0, y=5 but
    // "5px top" means x=5, y=0.
    // See: http://www.w3.org/TR/CSS21/colors.html#propdef-background-position
    // TODO(abliss): actually this is too permissive; "5px left" is not
    // allowed by the spec.
    // TODO(abliss): move this to webutil/css?
    Css::Value* x_value = NULL;
    Css::Value* y_value = NULL;
    for (int i = 0; (i < 2) && (x_value == NULL); i++) {
      Css::Value* value = values->at(values_offset + i);
      Css::Value* other_value = values->at(values_offset + 1 - i);
      if (value->GetLexicalUnitType() == Css::Value::IDENT) {
        switch (value->GetIdentifier().ident()) {
          case Css::Identifier::LEFT:
          case Css::Identifier::RIGHT:
            x_value = value;
            y_value = other_value;
            break;
          case Css::Identifier::TOP:
          case Css::Identifier::BOTTOM:
            x_value = other_value;
            y_value = value;
            break;
          default:
            // We do not currently support CENTER
            return false;
        }
      }
    }
    // If there are two values and neither is an identifier, x comes
    // first: e.g. "5px 6px" means x=5, y=6.
    if (x_value == NULL) {
      x_value = values->at(values_offset);
      y_value = values->at(values_offset + 1);
    }
    // Now that we know which value is which dimension, we can extract the
    // values in px.
    int x_px, y_px;
    if (!GetPixelValue(x_value, &x_px) ||
        !GetPixelValue(y_value, &y_px)) {
      return false;
    }
    x_value_ = values->at(values_offset);
    x_offset_ = x_px;
    y_value_ = values->at(values_offset + 1);
    y_offset_ = y_px;
    return true;
  }
  // Tries to guess whether this value is an x- or y- position value in the
  // background shorthand value list.
  static bool IsPositionValue(const Css::Value& value) {
    if (value.GetLexicalUnitType() == Css::Value::NUMBER) {
      return true;
    } else if (value.GetLexicalUnitType() == Css::Value::IDENT) {
        switch (value.GetIdentifier().ident()) {
          case Css::Identifier::LEFT:
          case Css::Identifier::RIGHT:
          case Css::Identifier::TOP:
          case Css::Identifier::BOTTOM:
          case Css::Identifier::CENTER:
            return true;
          default:
            return false;
        }
    }
    return false;
  }
  // Attempt to actually perform the url substitution.  Initialize must have
  // been called first, and must have returned true.
  void Realize(const char* url, int x, int y) {
    CHECK(x_value_ != NULL);
    *url_value_ = Css::Value(Css::Value::URI, UTF8ToUnicodeText(url));
    *x_value_ = Css::Value(x_offset_ - x, Css::Value::PX);
    *y_value_ = Css::Value(y_offset_ - y, Css::Value::PX);
    if ((declarations_ != NULL) && (declaration_to_push_ != NULL)) {
      declarations_->push_back(declaration_to_push_);
    }
  }

 private:
  // Attempt to find the background position values, or create them if
  // necessary.  If we return true, we should be all set for a call to
  // Realize().  If we return false, Realize() must never be called.
  bool FindBackgroundPositionValues(Css::Declarations* declarations) {
    // Find the original background offsets (if any) so we can add to them.
    bool position_found = false;
    for (Css::Declarations::iterator decl_iter = declarations->begin();
         !position_found && (decl_iter != declarations->end());
         ++decl_iter) {
      Css::Declaration* decl = *decl_iter;
      switch (decl->prop()) {
        case Css::Property::BACKGROUND_POSITION: {
          Css::Values* decl_values = decl->mutable_values();
          if (decl_values->size() != 2) {
            // If only one of the coordinates is specified, the other is
            // "center", which we don't currently support.
            return false;
          }
          if (ReadBackgroundPosition(decl_values, 0)) {
            position_found = true;
          } else {
            // Upon failure here, we abort the sprite.
            return false;
          }
          break;
        }
        case Css::Property::BACKGROUND_POSITION_X:
        case Css::Property::BACKGROUND_POSITION_Y:
          // These are non-standard, though supported in IE and Chrome.
          // TODO(abliss): handle these.
          return false;
        case Css::Property::BACKGROUND: {
          Css::Values* decl_values = decl->mutable_values();
          // The background shorthand can include many values in any order.
          // We'll look for two consecutive position values.  (If only one
          // position value is present, the other is considered to be CENTER
          // which we don't support.)
          for (int i = 0, n = decl_values->size() - 1; i < n; ++i) {
            if (IsPositionValue(*(decl_values->at(i))) &&
                IsPositionValue(*(decl_values->at(i + 1)))) {
              if (ReadBackgroundPosition(decl_values, i)) {
                position_found = true;
                break;
              } else {
                // Upon failure here, we abort the sprite.
                return false;
              }
            }
          }
          break;
        }
        default:
          break;
      }
    }
    if (!position_found) {
      // If no position was specified, it defaults to "0% 0%", which is the same
      // as "0px 0px".
      Css::Values* values = new Css::Values();
      x_value_ = new Css::Value(0, Css::Value::PX);
      values->push_back(x_value_);
      y_value_ = new Css::Value(0, Css::Value::PX);
      values->push_back(y_value_);
      declarations_ = declarations;
      declaration_to_push_ = new Css::Declaration(
          Css::Property::BACKGROUND_POSITION, values, false);
    }

    // TODO(abliss): consider specifying width and height.  Currently we are
    // assuming the node is already sized correctly.
    return true;
  }

  GoogleString old_url_;
  // Pointer to the value where the url of the image is stored.
  Css::Value* url_value_;
  // Pointer to the value where the background position x coordinate is stored.
  Css::Value* x_value_;
  // Pointer to the value where the background position y coordinate is stored.
  Css::Value* y_value_;
  // Optional pointer to a declarations object where a new declaration will be
  // pushed.
  Css::Declarations* declarations_;
  // Optional pointer to a declaration object to be pushed onto declarations_.
  Css::Declaration* declaration_to_push_;
  int x_offset_;
  int y_offset_;
  DISALLOW_COPY_AND_ASSIGN(SpriteFuture);
};

// An implementation of the Spriter's ImageLibraryInterface on top of our own
// Image class.  Instead of using the filesystem, we keep an in-memory map,
// which owns pointers to images.
class Library : public spriter::ImageLibraryInterface {
 public:
  // A thin layer of glue around an Image as input to the Spriter.
  class SpriterImage : public spriter::ImageLibraryInterface::Image {
   public:
    // Owns nothing.  Image may not be null.  The library is expected to
    // maintain ownership of the image pointer.
    SpriterImage(net_instaweb::Image* image,
                 spriter::ImageLibraryInterface* lib) :
        Image(lib), image_(image) {
      DCHECK(image_ != NULL) << "null image not allowed.";
    }

    virtual ~SpriterImage() {}

    virtual bool GetDimensions(int* out_width, int* out_height) const {
      ImageDim dim;
      image_->Dimensions(&dim);
      *out_width = dim.width();
      *out_height = dim.height();
      return (dim.width() >= 0) && (dim.height() >= 0);
    }

    // TODO(abliss): This should really be returning const Image*.
    net_instaweb::Image* image() const { return image_; }

   private:
    net_instaweb::Image* image_;
    DISALLOW_COPY_AND_ASSIGN(SpriterImage);
  };

  // A thin layer of glue around an Image as output from the Spriter.
  // Owns its own mutable image.
  class Canvas : public spriter::ImageLibraryInterface::Canvas {
   public:
    Canvas(int width, int height, Library* lib,
           const StringPiece& tmp_dir, MessageHandler* handler) :
        spriter::ImageLibraryInterface::Canvas(lib),
        lib_(lib) {
      DCHECK(lib != NULL);
      image_.reset(net_instaweb::BlankImage(width, height,
                                            net_instaweb::Image::IMAGE_PNG,
                                            tmp_dir, handler));
    }

    virtual ~Canvas() {}

    virtual bool DrawImage(const Image* image, int x, int y) {
      const SpriterImage* spriter_image
          = static_cast<const SpriterImage*>(image);
      return image_->DrawImage(spriter_image->image(), x, y);
    }

    // On successfully writing, we release our image.
    virtual bool WriteToFile(
        const FilePath& write_path, spriter::ImageFormat format) {
      if (format != spriter::PNG) {
        return false;
      }
      lib_->Register(write_path, image_.release());
      return true;
    }

   private:
    scoped_ptr<net_instaweb::Image> image_;
    Library* lib_;

    DISALLOW_COPY_AND_ASSIGN(Canvas);
  };

  Library(Delegate* delegate, const StringPiece& tmp_dir,
          MessageHandler* handler)
      : spriter::ImageLibraryInterface(delegate), handler_(handler) {
    tmp_dir.CopyToString(&tmp_dir_);
  }

  ~Library() {
    STLDeleteValues(&fake_fs_);
  }

  // Read an image from disk.  Return NULL (after calling delegate method) on
  // error.  Caller owns the returned pointer, which must not outlive this
  // library.
  virtual SpriterImage* ReadFromFile(const FilePath& path) {
    net_instaweb::Image* image = fake_fs_[path];
    if (image == NULL) {
      return NULL;
    }
    return new SpriterImage(image, this);
  }

  virtual Canvas* CreateCanvas(int width, int height) {
    return new Canvas(width, height, this, tmp_dir_, handler_);
  }

  // Does not take ownership of the resource.  Returns true if the image could
  // be loaded, in which case we'll keep our own pointer to the image backed by
  // the resource, meaning that resource must not be destroyed before the next
  // call to Clear().
  bool Register(Resource* resource) {
    net_instaweb::Image* image = net_instaweb::NewImage(
        resource->contents(), resource->url(), tmp_dir_, handler_);
    if (image->EnsureLoaded()) {
      Register(resource->url(), image);
      return true;
    } else {
      delete image;
      return false;
    }
  }

  void Clear() {
    STLDeleteValues(&fake_fs_);
    fake_fs_.clear();
  }

 private:
  void Register(const StringPiece& key, net_instaweb::Image* image) {
    fake_fs_[key.as_string()] = image;
  }

  // The spriter expects a filesystem interface for accessing images, but we
  // don't want to hit the disk excessively.  We keep here an in-memory map from
  // a "pathname" to its Image (which contains both the encoded input and the
  // decoded raster) for quick access.
  std::map<const GoogleString, net_instaweb::Image*> fake_fs_;
  GoogleString tmp_dir_;
  MessageHandler* handler_;
};

}  // namespace spriter_binding

using spriter_binding::Library;
using spriter_binding::SpriteFuture;

// The Combiner does all the work of spriting.  Each combiner takes an image of
// a certain type (e.g. PNGs) and produces a single sprite as a combination.
class ImageCombineFilter::Combiner
    : public ResourceCombinerTemplate<SpriteFuture*> {
 public:
  Combiner(RewriteDriver* driver, const StringPiece& filter_prefix,
           const StringPiece& extension, ImageCombineFilter* filter)
      : ResourceCombinerTemplate<SpriteFuture*>(driver, filter_prefix,
                                                extension, filter),
        library_(NULL,
                 driver->resource_manager()->filename_prefix(),
                 driver->message_handler()),
        image_file_count_reduction_(NULL) {
    Statistics* stats = driver->resource_manager()->statistics();
    if (stats != NULL) {
      image_file_count_reduction_ =
          stats->GetVariable(kImageFileCountReduction);
    }
  }

  virtual ~Combiner() {
    // Note that the superclass's dtor will not call our overridden Clear.
    // Fortunately there's no harm in calling Clear() several times.
    Clear();
  }

  // Unlike other combiners (css and js) we want to uniquify incoming resources.
  virtual TimedBool AddResource(const StringPiece& url,
                                MessageHandler* handler) {
    if (added_urls_.find(url.as_string()) == added_urls_.end()) {
      last_added_ = true;
      TimedBool result = ResourceCombiner::AddResource(url, handler);
      if (result.value) {
        added_urls_.insert(url.as_string());
      }
      return result;
    } else {
      // If the url has been successfully added to the partnership already.
      // Since the image is already in the sprite, we do nothing and return
      // success.
      last_added_ = false;
      TimedBool ret = {kint64max, true};
      return ret;
    }
  }

  virtual void RemoveLastResource() {
    // We only want to actually remove the resource from the partnership if the
    // last call to AddResource actually added it.
    if (last_added_) {
      ResourceCombiner::RemoveLastResource();
    }
  }

  virtual bool ResourceCombinable(Resource* resource, MessageHandler* handler) {
    // TODO(abliss) We exhibit zero intelligence about which images files to
    // combine; we combine whatever is possible.  This can reduce cache
    // effectiveness by combining highly cacheable shared resources with
    // transient ones.

    // We only handle PNGs for now.
    if (resource->type() && (resource->type()->type() != ContentType::kPng)) {
      handler->Message(kInfo, "Cannot sprite: not PNG");
      return false;
    }
    // Need to make sure our image library can handle this image.
    if (!library_.Register(resource)) {
      handler->Message(kInfo, "Cannot sprite: not decodable (transparent?)");
      return false;
    }
    return true;
  }

  virtual bool WriteCombination(
      const ResourceVector& combine_resources,
      const OutputResourcePtr& combination,
      MessageHandler* handler) {
    spriter::ImageSpriter spriter(&library_);

    spriter::SpriterInput input;
    input.set_id(0);
    spriter::SpriteOptions* options = input.mutable_options();
    options->set_output_base_path("");
    options->set_output_image_path("sprite");
    options->set_placement_method(spriter::VERTICAL_STRIP);

    int64 min_origin_expiration_time_ms = 0;
    for (int i = 0, n = combine_resources.size(); i < n; ++i) {
      const ResourcePtr& resource = combine_resources[i];
      int64 expire_time_ms = resource->CacheExpirationTimeMs();
      if ((min_origin_expiration_time_ms == 0) ||
          (expire_time_ms < min_origin_expiration_time_ms)) {
        min_origin_expiration_time_ms = expire_time_ms;
      }
      input.add_input_image_set()->set_path(resource->url());
    }

    scoped_ptr<spriter::SpriterResult> result(spriter.Sprite(input));
    if (result.get() == NULL) {
      handler->Error(UrlSafeId().c_str(), 0,
                     "Could not sprite.");
      return false;
    }
    scoped_ptr<Library::SpriterImage> result_image(
        library_.ReadFromFile(result->output_image_path()));
    if (result_image.get() == NULL) {
      handler->Error(UrlSafeId().c_str(), 0,
                     "Could not read sprited image.");
      return false;
    }

    combination->EnsureCachedResultCreated()->mutable_spriter_result()->
        CopyFrom(*result);

    if (!resource_manager_->Write(HttpStatus::kOK,
                                  result_image->image()->Contents(),
                                  combination.get(),
                                  min_origin_expiration_time_ms, handler)) {
      handler->Error(UrlSafeId().c_str(), 0,
                     "Could not write sprited resource.");
      return false;
    }
    return true;
  }

  bool Realize(MessageHandler* handler) {
    // TODO(abliss): If we encounter the same combination in a different order,
    // we'll needlessly generate a new sprite.
    OutputResourcePtr combination(Combine(kContentTypePng, handler));
    if (combination.get() == NULL) {
      return false;
    }
    GoogleString result_buf;
    if (!combination->cached_result()->has_spriter_result()) {
      handler->Error(UrlSafeId().c_str(), 0,
                     "No remembered sprite result.");
      return false;
    }
    const spriter::SpriterResult& result =
        combination->cached_result()->spriter_result();
    // Now gather up the positions for each of the original urls.
    std::map<GoogleString, const spriter::Rect*> url_to_clip_rect;
    for (int i = result.image_position_size() - 1; i >= 0; i--) {
      const spriter::ImagePosition& image_position = result.image_position(i);
      // Where the spriter expects file paths, we are using urls.
      url_to_clip_rect[image_position.path()] = &image_position.clip_rect();
    }

    GoogleString new_url = combination->url();
    const char* new_url_cstr = new_url.c_str();
    StringSet replaced_urls;
    for (int i = num_elements() - 1; i >= 0; i--) {
      SpriteFuture* future = element(i);
      const spriter::Rect* clip_rect = url_to_clip_rect[future->old_url()];
      if (clip_rect != NULL) {
        future->Realize(new_url_cstr, clip_rect->x_pos(), clip_rect->y_pos());
        replaced_urls.insert(future->old_url());
      }
    }
    if (image_file_count_reduction_ != NULL) {
      int sprited = replaced_urls.size();
      handler->Message(kInfo, "Sprited %d images to %s!", sprited,
                       new_url_cstr);
      image_file_count_reduction_->Add(sprited - 1);
    }
    return true;
  }

  virtual void Clear() {
    STLDeleteElements(&elements_);
    ResourceCombinerTemplate<SpriteFuture*>::Clear();
    library_.Clear();
    added_urls_.clear();
  }

  // Returns true if the image at url has already been added to the collection
  // and is at least as large as the given dimensions.
  bool CheckMinImageDimensions(const GoogleString& url, int width, int height) {
    scoped_ptr<Library::SpriterImage> image(library_.ReadFromFile(url));
    if (image.get() == NULL) {
      return false;
    }
    int image_width, image_height;
    if (!image->GetDimensions(&image_width, &image_height)) {
      return false;
    }
    return (image_width >= width) && (image_height >= height);
  }

 private:
  StringSet added_urls_;
  Library library_;
  Variable* image_file_count_reduction_;
  // Whether the last call to AddResource actually called through to super.
  // TODO(abliss): this is pretty ugly.  Should replace RemoveLast* with a
  // better API.
  bool last_added_;
};

// TODO(jmaessen): The addition of 1 below avoids the leading ".";
// make this convention consistent and fix all code.
ImageCombineFilter::ImageCombineFilter(RewriteDriver* driver,
                                   const char* filter_prefix)
    : RewriteFilter(driver, filter_prefix) {
  combiner_.reset(new Combiner(driver, filter_prefix,
                               kContentTypePng.file_extension() + 1,
                               this));
}

ImageCombineFilter::~ImageCombineFilter() {
}

void ImageCombineFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kImageFileCountReduction);
}

bool ImageCombineFilter::Fetch(const OutputResourcePtr& resource,
                               Writer* writer,
                               const RequestHeaders& request_header,
                               ResponseHeaders* response_headers,
                               MessageHandler* message_handler,
                               UrlAsyncFetcher::Callback* callback) {
  return combiner_->Fetch(resource, writer, request_header, response_headers,
                          message_handler, callback);
}

// We must not modify *declarations in this method, but we may hold pointers
// through which we will modify it in DoCombine.
TimedBool ImageCombineFilter::AddCssBackground(const GoogleUrl& original_url,
                                             Css::Declarations* declarations,
                                             Css::Value* url_value,
                                             MessageHandler* handler) {
  handler->Message(kInfo, "Attempting to sprite css background.");
  // If the element is larger than the image, spriting will not work correctly.
  // TODO(abliss): support same-sized vertically-repeating backgrounds in a
  // horizontal sprite, and horizontal ones in a vertical sprite.
  TimedBool ret = {kint64max, false};
  int width = -1;
  int height = -1;
  for (Css::Declarations::iterator decl_iter = declarations->begin();
       decl_iter != declarations->end(); ++decl_iter) {
    Css::Declaration* decl = *decl_iter;
    switch (decl->prop()) {
      case Css::Property::WIDTH: {
        const Css::Values* decl_values = decl->values();
        for (Css::Values::const_iterator value_iter = decl_values->begin();
             value_iter != decl_values->end(); ++value_iter) {
          Css::Value* value = *value_iter;
          if ((value->GetLexicalUnitType() == Css::Value::NUMBER)
              && (value->GetDimension() == Css::Value::PX)) {
            width = value->GetIntegerValue();
          } else {
            return ret;
          }
        }
        break;
      }
      case Css::Property::HEIGHT: {
        const Css::Values* decl_values = decl->values();
        for (Css::Values::const_iterator value_iter = decl_values->begin();
             value_iter != decl_values->end(); ++value_iter) {
          Css::Value* value = *value_iter;
          if ((value->GetLexicalUnitType() == Css::Value::NUMBER)
              && (value->GetDimension() == Css::Value::PX)) {
            height = value->GetIntegerValue();
          } else {
            return ret;
          }
        }
        break;
      }
      default:
        break;
    }
  }
  if ((width == -1) || (height == -1)) {
    handler->Message(kInfo, "Cannot sprite: no explicit dimensions");
    return ret;
  }
  SpriteFuture* future = new SpriteFuture(original_url.Spec());
  ret = combiner_->AddElement(future, future->old_url(), handler);
  if (ret.value) {
    if (!combiner_->CheckMinImageDimensions(
            original_url.Spec().as_string(), width, height)
        || !future->Initialize(declarations, url_value)) {
      combiner_->RemoveLastElement();
      // TODO(abliss): consider the case of scaled BG images (can we resize
      // them)?
      handler->Message(kInfo, "Cannot sprite: failed init.");
      ret.value = false;
      delete future;
    }
  } else {
    handler->Message(kInfo, "Cannot sprite: combiner forbids.");
    delete future;
  }
  return ret;
}

bool ImageCombineFilter::DoCombine(MessageHandler* handler) {
  return combiner_->Realize(handler);
}

void ImageCombineFilter::Reset() {
  combiner_->Reset();
}

}  // namespace net_instaweb
