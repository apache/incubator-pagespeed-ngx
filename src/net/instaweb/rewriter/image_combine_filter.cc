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

#include <cstddef>                     // for size_t
#include <iterator>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/rewriter/public/css_resource_slot.h"
#include "net/instaweb/rewriter/public/image.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_combiner.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/spriter/image_library_interface.h"
#include "net/instaweb/spriter/public/image_spriter.h"
#include "net/instaweb/spriter/public/image_spriter.pb.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/md5_hasher.h"
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
class UrlSegmentEncoder;

typedef std::map<GoogleString, const spriter::Rect*> RectMap;
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
  // Initialize declarations for future use.
  // Used for async flow where we can't check image against decls
  // when the future is created.
  explicit SpriteFuture(const StringPiece& old_url, int width, int height,
                        Css::Declarations* decls)
      : url_value_(NULL),
        x_value_(NULL),
        y_value_(NULL),
        declarations_(decls),
        declaration_to_push_(NULL),
        div_width_(width),
        div_height_(height),
        has_position_(false) {
    old_url.CopyToString(&old_url_);
    x_offset_ = 0;
    y_offset_ = 0;
  }

  ~SpriteFuture() {}

  // Bind this Future to a particular image.  Owns nothing; the inputs must
  // outlive this future.
  void Initialize(Css::Value* url_value) {
    url_value_ = url_value;
  }

  const GoogleString& old_url() { return old_url_; }

  Css::Declarations* decls() { return declarations_; }

  // Set x_px and y_px to the alignment for this image/div combination
  // before spriting.
  bool SetAlignmentValues(Css::Value* x_value, Css::Value* y_value,
                          int image_width, int image_height,
                          int* x_px, int* y_px) {
    bool ret = true;
    if (x_value->GetLexicalUnitType() == Css::Value::NUMBER) {
      if (IsValidNumberPosition(*x_value)) {
        int int_value = x_value->GetIntegerValue();
        *x_px = int_value;
      } else {
        ret = false;
      }
    } else if (x_value->GetLexicalUnitType() == Css::Value::IDENT) {
      switch (x_value->GetIdentifier().ident()) {
        case Css::Identifier::LEFT:
          *x_px = 0;
          break;
        case Css::Identifier::RIGHT:
          *x_px = div_width_ - image_width;
          break;
        case Css::Identifier::CENTER:
          *x_px = (div_width_ - image_width) / 2;
          break;
        default:
          ret = false;
          break;
      }
    }
    if (y_value->GetLexicalUnitType() == Css::Value::NUMBER) {
      if (ret && IsValidNumberPosition(*y_value)) {
        int int_value = y_value->GetIntegerValue();
        *y_px = int_value;
      } else {
        ret = false;
      }
    } else if (ret && y_value->GetLexicalUnitType() == Css::Value::IDENT) {
      switch (y_value->GetIdentifier().ident()) {
        case Css::Identifier::TOP:
          *y_px = 0;
          break;
        case Css::Identifier::BOTTOM:
          *y_px = div_height_ - image_height;
          break;
        case Css::Identifier::CENTER:
          *y_px = (div_height_ - image_height) / 2;
          break;
        default:
          ret = false;
          break;
      }
    }
    return ret;
  }

  // (1) Figure out what position declaration we have.
  // (2) If we have x, create y, and vice versa.
  // (3) Insert the new value into the values vector.
  bool ReadSingleValue(Css::Values* values, int values_offset,
                       Css::Value** x_value, Css::Value** y_value) {
    Css::Value* extra_value = new Css::Value(Css::Identifier::CENTER);
    Css::Value* value = values->at(values_offset);
    if (value->GetLexicalUnitType() == Css::Value::IDENT) {
      switch (value->GetIdentifier().ident()) {
        case Css::Identifier::LEFT:
        case Css::Identifier::RIGHT:
        case Css::Identifier::CENTER:
          *x_value = value;
          *y_value = extra_value;
          break;
        case Css::Identifier::TOP:
        case Css::Identifier::BOTTOM:
          *y_value = value;
          *x_value = extra_value;
          break;
        default:
          delete extra_value;
          return false;
      }
    } else {
      return false;
    }
    values->insert(values->begin() + values_offset + 1, extra_value);
    return true;
  }

  // (1) Figure out what position declaration we have first.
  // (2) If horizontal, other is vertical, and vice versa.
  // (3) If first value is a number, second value is vertical,
  //     first is horizontal.
  bool ReadTwoValues(Css::Values* values, int values_offset,
                     Css::Value**x_value, Css::Value** y_value) {
    Css::Value* value = values->at(values_offset);
    Css::Value* other_value = values->at(values_offset + 1);
    if (value->GetLexicalUnitType() == Css::Value::IDENT) {
      switch (value->GetIdentifier().ident()) {
        case Css::Identifier::LEFT:
        case Css::Identifier::RIGHT:
          *x_value = value;
          *y_value = other_value;
          break;
        case Css::Identifier::TOP:
        case Css::Identifier::BOTTOM:
          *x_value = other_value;
          *y_value = value;
          break;
        case Css::Identifier::CENTER:
          if (other_value->GetLexicalUnitType() == Css::Value::IDENT) {
            switch (other_value->GetIdentifier().ident()) {
              case Css::Identifier::LEFT:
              case Css::Identifier::RIGHT:
                *x_value = other_value;
                *y_value = value;
                break;
              case Css::Identifier::TOP:
              case Css::Identifier::BOTTOM:
              case Css::Identifier::CENTER:
                *x_value = value;
                *y_value = other_value;
                break;
              default:
                return false;
            }
          } else {
            // TODO(nforman): Allow for mixing of alignment types,
            // i.e. left 2px.
            return false;
          }
          break;
        default:
          return false;
      }
    } else {
      // If there are two values and neither is an identifier, x comes
      // first: e.g. "5px 6px" means x=5, y=6.
      // TODO(nforman): support % values.
      for (int i = 0; i < 2; ++i) {
        Css::Value* val = values->at(values_offset + i);
        if (val->GetLexicalUnitType() == Css::Value::NUMBER &&
            IsValidNumberPosition(*val)) {
          continue;
        }
        return false;
      }
      *x_value = values->at(values_offset);
      *y_value = values->at(values_offset + 1);
    }
    return true;
  }

  // Attempts to read the x and y values of the background position.  *values
  // is a value array which includes the background-position at values_offset.
  // new_x and new_y are the coordinates of the image in the sprite.  Returns
  // true, and sets up {x,y}_{value,offset}_ if successful.
  bool ReadBackgroundPosition(Css::Values* values, int values_offset,
                              int image_width, int image_height) {
    // Parsing these values is trickier than you might think.  If either
    // of the two values is a non-center identifier, it determines which
    // is x and which is y.  So for example, "5px left" means x=0, y=5 but
    // "5px top" means x=5, y=0.
    // See: http://www.w3.org/TR/CSS21/colors.html#propdef-background-position
    // TODO(abliss): move this to webutil/css?
    Css::Value* x_value = NULL;
    Css::Value* y_value = NULL;
    if (((int)values->size() - values_offset == 1) ||
        !IsPositionValue(*(values->at(values_offset + 1)))) {
      if (!ReadSingleValue(values, values_offset, &x_value, &y_value)) {
        return false;
      }
    } else if (!ReadTwoValues(values, values_offset, &x_value, &y_value)) {
      return false;
    }
    // Now that we know which value is which dimension, we can extract the
    // values in px.
    int x_px = 0;
    int y_px = 0;
    if (!SetAlignmentValues(x_value, y_value, image_width, image_height,
                            &x_px, &y_px)) {
      return false;
    }
    // When sprited, these x_value_ and y_value_ will both be replaced
    // with absolute pixel values (i.e. not center or left), so they need
    // to be in x-first, y-second order.
    x_value_ = values->at(values_offset);
    x_offset_ = x_px;
    y_value_ = values->at(values_offset + 1);
    y_offset_ = y_px;
    return true;
  }

  // Returns whether or not this is a number value we can handle.
  static bool IsValidNumberPosition(const Css::Value& value) {
    CHECK(value.GetLexicalUnitType() == Css::Value::NUMBER);
    int int_value = value.GetIntegerValue();
    // If the aligment is specified in pixels, or is 0, we can just use it.
    if ((value.GetDimension() == Css::Value::PX) || (int_value == 0)) {
      return true;
    }
    return false;
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
    if (!has_position_) {
      // If no position was specified, it defaults to "0% 0%", which is the same
      // as "0px 0px".
      Css::Values* values = new Css::Values();
      x_value_ = new Css::Value(0, Css::Value::PX);
      values->push_back(x_value_);
      y_value_ = new Css::Value(0, Css::Value::PX);
      values->push_back(y_value_);
      declaration_to_push_ = new Css::Declaration(
          Css::Property::BACKGROUND_POSITION, values, false);
    }
    CHECK(x_value_ != NULL);
    *url_value_ = Css::Value(Css::Value::URI, UTF8ToUnicodeText(url));
    *x_value_ = Css::Value(x_offset_ - x, Css::Value::PX);
    *y_value_ = Css::Value(y_offset_ - y, Css::Value::PX);

    if ((declarations_ != NULL) && (declaration_to_push_ != NULL)) {
      declarations_->push_back(declaration_to_push_);
    }
  }

  int width() {
    return div_width_;
  }

  int height() {
    return div_height_;
  }

  // Attempt to find the background position values, or create them if
  // necessary.  If we return true, we should be all set for a call to
  // Realize().  If we return false, Realize() must never be called.
  // set has_position_ to true if there is already a position declaration.
  // If has_position_ is false, we will create a new declaration when
  // rendering.
  // Returns true if this is a viable sprite-future.  If
  // we return false, Realize must not be called.
  bool FindBackgroundPositionValues(int image_width, int image_height) {
    // Find the original background offsets (if any) so we can add to them.
    has_position_ = false;
    for (Css::Declarations::iterator decl_iter = declarations_->begin();
         !has_position_ && (decl_iter != declarations_->end());
         ++decl_iter) {
      Css::Declaration* decl = *decl_iter;
      switch (decl->prop()) {
        case Css::Property::BACKGROUND_POSITION: {
          Css::Values* decl_values = decl->mutable_values();
          if (decl_values->size() > 2 || decl_values->size() < 1) {
            return false;
          }
          if (ReadBackgroundPosition(decl_values, 0,
                                     image_width, image_height)) {
            has_position_ = true;
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
          for (int i = 0, n = decl_values->size(); i < n; ++i) {
            if (IsPositionValue(*(decl_values->at(i)))) {
              if (ReadBackgroundPosition(decl_values, i,
                                         image_width, image_height)) {
                has_position_ = true;
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
    // TODO(abliss): consider specifying width and height.  Currently we are
    // assuming the node is already sized correctly.
    return true;
  }

 private:
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
  // Width and height of original div.  We use these to check against
  // the image's dimensions once the image is loaded, so we can determine
  // if the image can be sprited in this context.
  int div_width_;
  int div_height_;
  bool has_position_;
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
      net_instaweb::Image::CompressionOptions* options =
          new net_instaweb::Image::CompressionOptions();
      options->recompress_png = true;
      image_.reset(BlankImageWithOptions(width, height,
                                         net_instaweb::Image::IMAGE_PNG,
                                         tmp_dir, handler, options));
    }

    virtual ~Canvas() { }

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
      lib_->RegisterImage(write_path, image_.release());
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
  // be detected as a valid format, in which case we'll keep our own pointer to
  // the image backed by the resource, meaning that resource must not be
  // destroyed before the next call to Clear().
  bool Register(Resource* resource, MessageHandler* handler) {
    net_instaweb::Image* prev_image = fake_fs_[resource->url()];
    if (prev_image != NULL) {
      // Already registered
      return true;
    }

    net_instaweb::Image::CompressionOptions* image_options =
        new net_instaweb::Image::CompressionOptions();
    image_options->webp_preferred = false;  // Not working with jpg/webp at all.
    // TODO(satyanarayana): Use appropriate quality param for spriting.
    image_options->jpeg_quality =
        RewriteOptions::kDefaultImageJpegRecompressQuality;
    // TODO(nikhilmadan): Use appropriate progressive setting for spriting.
    image_options->progressive_jpeg = false;
    image_options->convert_png_to_jpeg = false;

    scoped_ptr<net_instaweb::Image> image(net_instaweb::NewImage(
        resource->contents(), resource->url(), tmp_dir_, image_options,
        handler_));

    // We only handle PNGs and GIFs (which are converted to PNGs) for now.
    net_instaweb::Image::Type image_type = image->image_type();
    if ((image_type != net_instaweb::Image::IMAGE_PNG) &&
        (image_type != net_instaweb::Image::IMAGE_GIF)) {
      handler->Message(kInfo, "Cannot sprite: not PNG or GIF, %s",
                       resource->url().c_str());
      return false;
    }
    RegisterImage(resource->url(), image.release());
    return true;
  }

  void Clear() {
    STLDeleteValues(&fake_fs_);
    fake_fs_.clear();
  }

 private:
  typedef std::map<const GoogleString, net_instaweb::Image*> ImageMap;
  void RegisterImage(const StringPiece& key, net_instaweb::Image* image) {
    std::pair<ImageMap::iterator, bool> result(
        fake_fs_.insert(std::make_pair(key.as_string(), image)));
    if (!result.second) {
      // Already existed.
      ImageMap::iterator iter = result.first;
      if (iter->second != image) {
        delete iter->second;
        iter->second = image;
      }
    }
  }

  // The spriter expects a filesystem interface for accessing images, but we
  // don't want to hit the disk excessively.  We keep here an in-memory map from
  // a "pathname" to its Image (which contains both the encoded input and the
  // decoded raster) for quick access. Owns the Image objects.
  ImageMap fake_fs_;
  GoogleString tmp_dir_;
  MessageHandler* handler_;
};

}  // namespace spriter_binding

using spriter_binding::Library;
using spriter_binding::SpriteFuture;

// The Combiner does all the work of spriting.  Each combiner takes a set of
// images and produces a single sprite as a combination.
class ImageCombineFilter::Combiner : public ResourceCombiner {
 public:
  Combiner(ImageCombineFilter* filter, Library* library)
      : ResourceCombiner(filter->driver(), kContentTypePng.file_extension() + 1,
                         filter),
        library_(library) { }

  virtual ~Combiner() {
    // Note that the superclass's dtor will not call our overridden Clear.
    // Fortunately there's no harm in calling Clear() several times.
    Clear();
  }

  virtual bool WriteCombination(
      const ResourceVector& combine_resources,
      const OutputResourcePtr& combination,
      MessageHandler* handler) {
    spriter::ImageSpriter spriter(library_);

    spriter::SpriterInput input;
    input.set_id(0);
    spriter::SpriteOptions* options = input.mutable_options();
    options->set_output_base_path("");
    options->set_output_image_path("sprite");
    options->set_placement_method(spriter::VERTICAL_STRIP);

    for (int i = 0, n = combine_resources.size(); i < n; ++i) {
      const ResourcePtr& resource = combine_resources[i];
      input.add_input_image_set()->set_path(resource->url());
    }

    scoped_ptr<spriter::SpriterResult> result(spriter.Sprite(input));
    if (result.get() == NULL) {
      handler->Error(UrlSafeId().c_str(), 0,
                     "Could not sprite.");
      return false;
    }
    scoped_ptr<Library::SpriterImage> result_image(
        library_->ReadFromFile(result->output_image_path()));
    if (result_image.get() == NULL) {
      handler->Error(UrlSafeId().c_str(), 0,
                     "Could not read sprited image.");
      return false;
    }

    combination->EnsureCachedResultCreated()->mutable_spriter_result()->
        CopyFrom(*result);
    if (!resource_manager_->Write(combine_resources,
                                  result_image->image()->Contents(),
                                  &kContentTypePng,
                                  StringPiece(),  // no charset on images.
                                  combination.get(),
                                  handler)) {
      handler->Error(UrlSafeId().c_str(), 0,
                     "Could not write sprited resource.");
      return false;
    }
    return true;
  }

  OutputResourcePtr MakeOutput() {
    return Combine(rewrite_driver_->message_handler());
  }

  virtual void Clear() {
    ResourceCombiner::Clear();
    added_urls_.clear();
  }

  bool Write(const ResourceVector& in, const OutputResourcePtr& out) {
    return WriteCombination(in, out, rewrite_driver_->message_handler());
  }

 private:
  virtual const ContentType* CombinationContentType() {
    return &kContentTypePng;
  }

  StringSet added_urls_;
  Library* library_;
};

// Special resource slot that has a future_ pointer.
class SpriteFutureSlot : public CssResourceSlot {
 public:
  SpriteFutureSlot(const ResourcePtr& resource, Css::Values* values,
                   size_t value_index, SpriteFuture* future)
      : CssResourceSlot(resource, values, value_index),
        future_(future),
        may_sprite_(false) {
  }

  SpriteFuture* future() { return future_.get(); }

  virtual void Render() {
    // If we couldn't sprite this slot, try to apply other filters.
    if (!may_sprite_) {
      CssResourceSlot::Render();
    }
  }

  void set_may_sprite(bool x) { may_sprite_ = x; }

  bool may_sprite() const { return may_sprite_; }

 protected:
  REFCOUNT_FRIEND_DECLARATION(SpriteFutureSlot);
  virtual ~SpriteFutureSlot() {
  }

 private:
  scoped_ptr<SpriteFuture> future_;
  bool may_sprite_;
  DISALLOW_COPY_AND_ASSIGN(SpriteFutureSlot);
};

typedef RefCountedPtr<SpriteFutureSlot> SpriteFutureSlotPtr;

class ImageCombineFilter::Context : public RewriteContext {
 public:
  // TODO(jmaessen): The addition of 1 below avoids the leading ".";
  // make this convention consistent and fix all code.
  Context(ImageCombineFilter* filter, RewriteContext* parent,
          const GoogleUrl& css_url, const StringPiece& css_text)
      : RewriteContext(NULL, parent, NULL),
        library_(NULL,
                 filter->driver()->server_context()->filename_prefix(),
                 filter->driver()->message_handler()),
        filter_(filter) {
    MD5Hasher hasher;
    key_suffix_ = StrCat("css-key=", hasher.Hash(css_text),
                         "_", hasher.Hash(css_url.AllExceptLeaf()));
  }

  Context(RewriteDriver* driver, ImageCombineFilter* filter)
      : RewriteContext(driver, NULL, NULL),
        library_(NULL,
                 filter->driver()->server_context()->filename_prefix(),
                 filter->driver()->message_handler()),
        filter_(filter) {
  }

  virtual ~Context() {}

  // This cache key will no longer match the partition key generated for
  // fetches in RewriteContext.  This may or may not cause cache misses
  // when we could have had hits. It should not cause any functional
  // errors.
  // We hash the usual cache key, which is a list of the urls in the
  // combination, in order to keep it short so it doesn't run up against
  // filename length limits on apache.
  // TODO(nforman): Figure out a way to test cache keys in general.
  virtual GoogleString CacheKeySuffix() const {
    return key_suffix_;
  }

  bool AddFuture(CssResourceSlotPtr slot) {
    SpriteFutureSlot* future_slot = static_cast<SpriteFutureSlot*>(slot.get());
    StringPiece url(future_slot->future()->old_url());
    AddSlot(ResourceSlotPtr(slot));
    return true;
  }

  virtual const UrlSegmentEncoder* encoder() const {
    return filter_->encoder();
  }
  virtual const char* id() const { return filter_->id(); }
  virtual OutputResourceKind kind() const { return kRewrittenResource; }

  void Reset() {
    library_.Clear();
  }

 protected:
  // Write the combination out.
  virtual void Rewrite(int partition_index, CachedResult* partition,
                       const OutputResourcePtr& output) {
    RewriteResult result = kRewriteOk;
    if (!output->IsWritten()) {
      // Note that this method expects to do something for only the fetch path,
      // when only one partition should be in use --- in the rewrite path
      // we should have already written everything out in Partition().
      DCHECK_EQ(0, partition_index);
      ImageCombineFilter::Combiner combiner(filter_, &library_);

      ResourceVector resources;
      bool ok = true;
      for (int i = 0, n = num_slots(); (i < n) && ok; ++i) {
        ResourcePtr resource(slot(i)->resource());
        resources.push_back(resource);
        RegisterResource(resource.get());
        ok = EnsureLoaded(resource->url());
      }
      if (!ok || !combiner.Write(resources, output)) {
        result = kRewriteFailed;
      }
    }
    RewriteDone(result, partition_index);
  }

  // Finalize the declarations for the sprited slots.
  // TODO(nforman): be smarter about when to sprite and when not.
  // e.g. if it turns out all the divs are too big to use the sprite
  // except for one, don't use it.
  virtual void Render() {
    for (int p = 0, np = num_output_partitions(); p < np; ++p) {
      CachedResult* partition = output_partition(p);
      int num_inputs = partition->input_size();
      if (num_inputs > 1) {
        if (!partition->has_spriter_result()) {
          // TODO(nforman): some error handling here.
          DLOG(FATAL) << "spriting failed during Render";
          break;
        }
        const spriter::SpriterResult& spriter_result =
            partition->spriter_result();
        RectMap url_to_clip_rect;
        // Now gather up the positions for each of the original urls.
        for (int i = spriter_result.image_position_size() - 1; i >= 0; i--) {
          const spriter::ImagePosition& image_position =
              spriter_result.image_position(i);
          // Where the spriter expects file paths, we are using urls.
          url_to_clip_rect[image_position.path()] = &image_position.clip_rect();
        }

        GoogleString new_url = partition->url();
        const char* new_url_cstr = new_url.c_str();
        StringSet replaced_urls;  // for stats purposes
        for (int i = 0; i < num_inputs; ++i) {
          int slot_index = partition->input(i).index();
          SpriteFutureSlot* sprite_slot =
              static_cast<SpriteFutureSlot*>(slot(slot_index).get());
          SpriteFuture* future = sprite_slot->future();
          const spriter::Rect* clip_rect = url_to_clip_rect[future->old_url()];
          // Check against original image dimensions.
          // If these are smaller than the div we're putting the image
          // into then we can't sprite this declaraion
          if (clip_rect->width() < future->width() ||
              clip_rect->height() < future->height()) {
            continue;
          }
          if (clip_rect != NULL) {
            future->Realize(new_url_cstr, clip_rect->x_pos(),
                            clip_rect->y_pos());
            MessageHandler* handler = filter_->driver()->message_handler();
            handler->Message(kInfo, "Inserted sprite, url: %s\n",
                             new_url_cstr);
            replaced_urls.insert(future->old_url());
            sprite_slot->set_may_sprite(true);
          }
        }
        int sprited = replaced_urls.size();
        filter_->AddFilesReducedStat(sprited - 1);
      }
    }
    Reset();
  }

  // Partition the slots by what can get sprited and what can't.
  // Currently, we greedily combine everything that can be combined
  // in as few partitions as possible. We skip over slots that point to images
  // that are too small for the context they're in.
  // TODO(nforman): Consider separating by color map to group things smarter.
  virtual void PartitionAsync(OutputPartitions* partitions,
                              OutputResourceVector* outputs) {
    // Partitioning here requires image decompression, so we want to
    // move it to a different thread.
    Driver()->AddLowPriorityRewriteTask(MakeFunction(
        this, &Context::PartitionImpl, &Context::PartitionCancel,
        partitions, outputs));
  }

  void PartitionImpl(OutputPartitions* partitions,
                     OutputResourceVector* outputs) {
    StringSet no_sprite;
    FindUnspritable(&no_sprite);
    CollectSlots(partitions, outputs, &no_sprite);
    CrossThreadPartitionDone(partitions->partition_size() != 0);
  }

  void PartitionCancel(OutputPartitions* partitions,
                       OutputResourceVector* outputs) {
    CrossThreadPartitionDone(false);
  }

 private:
  // Class that associates a list of urls and a partition with a combiner.
  class ImageCombination : public ImageCombineFilter::Combiner {
   public:
    ImageCombination(ImageCombineFilter* filter, Library* library)
        : ImageCombineFilter::Combiner(filter, library),
          partition_(NULL) { }

    virtual ~ImageCombination() { }

    void AddResourceToPartition(Resource* resource, int index) {
      resource->AddInputInfoToPartition(
          Resource::kIncludeInputHash, index, partition_);
    }

    void set_partition(CachedResult* partition) { partition_ = partition; }

    CachedResult* partition() { return partition_; }

   private:
    CachedResult* partition_;  // Does not own memory.
    DISALLOW_COPY_AND_ASSIGN(ImageCombination);
  };

  typedef std::vector<ImageCombination*> ImageCombinationVector;

  // Put this resource in the library.
  bool RegisterResource(Resource* resource) {
    return library_.Register(resource, filter_->driver()->message_handler());
  }

  bool EnsureLoaded(const GoogleString& url) {
    scoped_ptr<Library::SpriterImage> spriter_image(library_.ReadFromFile(url));
    if (spriter_image.get() == NULL) {
      return false;
    }

    return spriter_image->image()->EnsureLoaded(false);
  }

  // Returns true if the image at url has already been added to the collection
  // and is at least as large as the given dimensions.
  bool GetImageDimensions(const GoogleString& url, int* width, int* height) {
    scoped_ptr<Library::SpriterImage> image(library_.ReadFromFile(url));
    if (image.get() == NULL) {
      return false;
    }
    return image->GetDimensions(width, height);
  }

  // Returns true iff declarations were setup properly, and the image
  // are smaller than the specified div dimensions.
  bool SetupSpriteDimensions(SpriteFuture* future) {
    int image_width, image_height;
    if (!GetImageDimensions(future->old_url(), &image_width, &image_height)) {
      return false;
    }
    if (image_width < future->width() || image_height < future->height()) {
      return false;
    }
    return future->FindBackgroundPositionValues(image_width, image_height);
  }

  // Walk through and find any resources that won't be able to be
  // sprited.  If we can't sprite them, add the url to the no-sprite
  // set.
  //
  // TODO(abliss) We exhibit zero intelligence about which image files to
  // combine; we combine whatever is possible.  This can reduce cache
  // effectiveness by combining highly cacheable shared resources with
  // transient ones.
  void FindUnspritable(StringSet* no_sprite) {
    StringSet seen_urls;
    for (int i = 0, n = num_slots(); i < n; ++i) {
      ResourcePtr resource(slot(i)->resource());
      SpriteFutureSlot* sprite_slot =
          static_cast<SpriteFutureSlot*>(slot(i).get());
      SpriteFuture* future = sprite_slot->future();
      GoogleString resource_url = resource->url();
      if (no_sprite->find(resource_url) == no_sprite->end()) {
        if (!resource->IsValidAndCacheable()) {
          no_sprite->insert(resource_url);
        } else {
          // Register the resource with the library and then check
          // its dimensions against those of the declaration to make
          // sure we can sprite here.
          // TODO(nforman): cheaper image dimension checking.
          if (seen_urls.find(resource_url) == seen_urls.end()) {
            RegisterResource(resource.get());
            seen_urls.insert(resource_url);
          }
          if (!SetupSpriteDimensions(future) || !EnsureLoaded(resource_url)) {
            no_sprite->insert(resource_url);
          }
        }
      }
    }
  }

  // For each slot, try to add its resource to the current partition.
  // If we can't, then finalize the last combination, and then
  // move on to the next slot.
  void CollectSlots(OutputPartitions* partitions,
                    OutputResourceVector* outputs,
                    StringSet* no_sprite) {
    ImageCombinationVector combinations;
    MessageHandler* handler = filter_->driver()->message_handler();
    std::map<GoogleString, ImageCombination*> urls_to_combos;

    for (int i = 0, n = num_slots(); i < n; ++i) {
      ResourcePtr resource(slot(i)->resource());
      SpriteFutureSlot* sprite_slot =
              static_cast<SpriteFutureSlot*>(slot(i).get());
      SpriteFuture* future = sprite_slot->future();
      GoogleString resource_url = future->old_url();
      if (no_sprite->find(resource_url) != no_sprite->end()) {
        continue;
      }
      bool added = false;
      // Don't add the same url to a combination twice.

      std::map<GoogleString, ImageCombination*>::iterator it;
      it = urls_to_combos.find(resource_url);
      if (it != urls_to_combos.end()) {
        ImageCombination* combo = it->second;
        DCHECK(combo != NULL) << "Combination points to NULL partition.";
        combo->AddResourceToPartition(resource.get(), i);
        added = true;
      }
      // If it wasn't already in the combination, see if we can add it.
      // Even if the image is spritable, it may not be able to be sprited
      // in a particular combination due to domain lawyer restrictions,
      // or (perhaps in the future) image type.
      if (!added) {
        for (int j = 0, m = combinations.size(); j < m; ++j) {
          ImageCombination* combo = combinations[j];
          if (combo->AddResourceNoFetch(resource, handler).value) {
            combo->AddResourceToPartition(resource.get(), i);
            urls_to_combos[resource_url] = combo;
            added = true;
            break;
          }
        }
        // If we couldn't add this resource to any of the existing partitions,
        // try making a new one and adding it there.  We may still not be able
        // to do that if the resource isn't spritable at all.
        if (!added) {
          scoped_ptr<ImageCombination> combo(new ImageCombination(
              filter_, &library_));
          if (combo->AddResourceNoFetch(resource, handler).value) {
            combo->set_partition(partitions->add_partition());
            combo->AddResourceToPartition(resource.get(), i);
            urls_to_combos[resource_url] = combo.get();
            combinations.push_back(combo.release());
          } else {
            no_sprite->insert(resource_url);
          }
        }
      }
    }
    FinalizePartitions(combinations, partitions, outputs);
    STLDeleteElements(&combinations);
    Reset();
  }

  // Write the output for the combinations.  If a combination can not be
  // written (e.g. it has only one element), then remove its partition.
  void FinalizePartitions(const ImageCombinationVector& combinations,
                          OutputPartitions* partitions,
                          OutputResourceVector* outputs) {
    std::set<int> remove_indices;
    for (int i = 0, n = combinations.size(); i < n; ++i) {
      ImageCombination* combination = combinations[i];
      CachedResult* partition = combination->partition();
      if (partition != NULL) {
        OutputResourcePtr combination_output(combination->MakeOutput());
        if (combination_output.get() == NULL) {
          remove_indices.insert(i);
        } else {
          combination_output->UpdateCachedResultPreservingInputInfo(partition);
          outputs->push_back(combination_output);
        }
      }
    }
    // We can re-arrange the partitions at this point only because we are
    // about to delete the ImageCombinations (and with them, their pointers
    // to those partitions).
    std::set<int>::reverse_iterator rit;
    for (rit = remove_indices.rbegin(); rit != remove_indices.rend(); rit++) {
      int last_partition = partitions->partition_size() - 1;
      if (*rit != last_partition) {
        partitions->mutable_partition()->SwapElements(*rit, last_partition);
      }
      partitions->mutable_partition()->RemoveLast();
    }
  }

  Library library_;
  ImageCombineFilter* filter_;
  GoogleString key_suffix_;
};

ImageCombineFilter::ImageCombineFilter(RewriteDriver* driver)
    : RewriteFilter(driver),
      context_(NULL) {
  Statistics* stats = driver->server_context()->statistics();
  image_file_count_reduction_ = stats->GetVariable(kImageFileCountReduction);
}

ImageCombineFilter::~ImageCombineFilter() {
}

void ImageCombineFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kImageFileCountReduction);
}

// Get the dimensions of the declaration.  This is tricky.
// If the element is larger than the image, spriting will not work correctly.
// TODO(abliss): support same-sized vertically-repeating backgrounds in a
// horizontal sprite, and horizontal ones in a vertical sprite.
bool ImageCombineFilter::GetDeclarationDimensions(
    Css::Declarations* declarations, int* width, int* height) {
  css_util::DimensionState state =
      css_util::GetDimensions(declarations, width, height);
  return (state == css_util::kHasBothDimensions);
}

// Must initialize context_ with appropriate parent before hand.
// parent passed here because it's private.
void ImageCombineFilter::AddCssBackgroundContext(
    const GoogleUrl& original_url, Css::Values* values, int value_index,
    CssFilter::Context* parent, Css::Declarations* decls,
    MessageHandler* handler) {
  CHECK(context_ != NULL);
  handler->Message(kInfo, "Attempting to sprite css background.");
  int width, height;
  if (!GetDeclarationDimensions(decls, &width, &height)) {
    handler->Message(kInfo, "Cannot sprite: no explicit dimensions");
    return;
  }
  StringPiece url_piece(original_url.Spec());
  SpriteFuture* future = new SpriteFuture(url_piece, width, height, decls);

  future->Initialize(values->at(value_index));

  ResourcePtr resource = CreateInputResource(url_piece);
  if (resource.get() != NULL) {
    // transfers ownership of future to slot_obj
    SpriteFutureSlot* slot_obj = new SpriteFutureSlot(
        resource, values, value_index, future);
    CssResourceSlotPtr slot(slot_obj);
    parent->slot_factory()->UniquifySlot(slot);
    // Spriting must run before all other filters so that the slot for the
    // resource a SpriteFutureSlot
    if (slot.get() != slot_obj) {
      return;
    }
    context_->AddFuture(slot);
  }
  return;
}

void ImageCombineFilter::Reset(RewriteContext* parent,
                               const GoogleUrl& css_url,
                               const StringPiece& css_text) {
  context_ = MakeNestedContext(parent, css_url, css_text);
}

void ImageCombineFilter::RegisterOrReleaseContext() {
  if ((context_ != NULL) && (context_->num_slots() != 0)) {
    context_->parent()->AddNestedContext(context_);
  } else {
    delete context_;
    context_ = NULL;
  }
}

// Make a new context that is nested under parent.
ImageCombineFilter::Context* ImageCombineFilter::MakeNestedContext(
    RewriteContext* parent, const GoogleUrl& css_url,
    const StringPiece& css_text) {
  Context* context = new Context(this, parent, css_url, css_text);
  return context;
}

RewriteContext* ImageCombineFilter::MakeRewriteContext() {
  return new Context(driver_, this);
}

void ImageCombineFilter::AddFilesReducedStat(int reduced) {
  image_file_count_reduction_->Add(reduced);
}

}  // namespace net_instaweb
