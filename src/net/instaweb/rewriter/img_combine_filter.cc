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

#include "net/instaweb/rewriter/public/img_combine_filter.h"

#include <map>

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/image.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_combiner_template.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/spriter/public/image_spriter.h"
#include "net/instaweb/spriter/public/image_spriter.pb.h"
#include "net/instaweb/spriter/image_library_interface.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include <string>
#include "net/instaweb/util/public/string_writer.h"
#include "webutil/css/parser.h"

namespace net_instaweb {

namespace {

// names for Statistics variables.
const char kImgFileCountReduction[] = "img_file_count_reduction";

// A SpriteFuture keeps track of a single image that is to be sprited.  When
// constructed, it is in an invalid state and merely serves as a token for the
// partnership.
class SpriteFuture {
 public:
  SpriteFuture() : declarations_(NULL), values_(NULL) {}

  // Bind this Future to a partictular image.  Owns nothing; the inputs must
  // outlive this future.
  void Initialize(Css::Declarations* declarations, Css::Values* values,
                  int value_index) {
    declarations_ = declarations;
    values_ = values;
    value_index_ = value_index;
  }

  // Actually perform the url substitution.  Initialize must have been called
  // first.
  void Realize(const char* url, int x, int y) {
    DCHECK(declarations_ != NULL);
    // Replace the old URL with the new one.
    delete (*values_)[value_index_];
    (*values_)[value_index_] = new Css::Value(
        Css::Value::URI, UTF8ToUnicodeText(url));
    Css::Values* values = new Css::Values();
    values->push_back(new Css::Value(-y, Css::Value::PX));
    // Add a new declaration for the background position.
    // TODO(abliss): This does not work correctly on firefox if the background
    // declaration included position values.
    declarations_->push_back(new Css::Declaration(
        Css::Property::BACKGROUND_POSITION_Y, values, true));
    values = new Css::Values();
    values->push_back(new Css::Value(-x, Css::Value::PX));
    declarations_->push_back(new Css::Declaration(
        Css::Property::BACKGROUND_POSITION_X, values, true));
    // TODO(abliss): consider specifying width and height.  Currently we are
    // assuming the node is already sized correctly.
  }
  Css::Declarations* declarations_;
  Css::Values* values_;
  // Index of the URL value in the Values array.
  int value_index_;

 private:
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

    virtual bool GetDimensions(int* out_width, int* out_height) {
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
      image_.reset(new net_instaweb::Image(width, height,
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
    net_instaweb::Image* image = new net_instaweb::Image(
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
  std::map<const std::string, net_instaweb::Image*> fake_fs_;
  std::string tmp_dir_;
  MessageHandler* handler_;
};

}  // namespace

// The Combiner does all the work of spriting.  Each combiner takes an image of
// a certain type (e.g. PNGs) and produces a single sprite as a combination.
class ImgCombineFilter::Combiner
    : public ResourceCombinerTemplate<SpriteFuture*> {
 public:
  Combiner(RewriteDriver* driver, const StringPiece& filter_prefix,
           const StringPiece& extension, ImgCombineFilter* filter)
      : ResourceCombinerTemplate<SpriteFuture*>(driver, filter_prefix,
                                                extension, filter),
        library_(NULL,
                 driver->resource_manager()->filename_prefix(),
                 driver->message_handler()),
        img_file_count_reduction_(NULL) {
    Statistics* stats = driver->resource_manager()->statistics();
    if (stats != NULL) {
      img_file_count_reduction_ = stats->GetVariable(kImgFileCountReduction);
    }
  }

  virtual bool ResourceCombinable(Resource* resource, MessageHandler* handler) {
    // TODO(abliss) We exhibit zero intelligence about which images files to
    // combine; we combine whatever is possible.  This can reduce cache
    // effectiveness by combining highly cacheable shared resources with
    // transient ones.

    // We only handle PNGs for now.
    if (resource->type() && (resource->type()->type() != ContentType::kPng)) {
      return false;
    }
    // Need to make sure our image library can handle this image.
    if (!library_.Register(resource)) {
      return false;
    }
    return true;
  }

  virtual bool WriteCombination(
      const ResourceVector& combine_resources,
      OutputResource* combination,
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
      Resource* resource = combine_resources[i];
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
            combination, min_origin_expiration_time_ms, handler)) {
      handler->Error(UrlSafeId().c_str(), 0,
                     "Could not write sprited resource.");
      return false;
    }
    return true;
  }

  bool Realize(MessageHandler* handler) {
    // TODO(abliss): If we encounter the same combination in a different order,
    // we'll needlessly generate a new sprite.
    scoped_ptr<OutputResource> combination(Combine(kContentTypePng, handler));
    if (combination.get() == NULL) {
      return false;
    }
    std::string result_buf;
    if (!combination->cached_result()->has_spriter_result()) {
      handler->Error(UrlSafeId().c_str(), 0,
                     "No remembered sprite result.");
      return false;
    }
    const spriter::SpriterResult& result =
        combination->cached_result()->spriter_result();
    int n = num_urls();
    if (n != result.image_position_size()) {
      handler->Error(UrlSafeId().c_str(), 0,
                     "Sprite result had %d images but we wanted %d",
                     result.image_position_size(), n);
      return false;
    }

    // TODO(abliss): If the same image is included multiple times, it may
    // show up multiple times in the sprite.
    std::string new_url = combination->url();
    const char* new_url_str = new_url.c_str();
    for (int i = n - 1; i >= 0; i--) {
      SpriteFuture* future = element(i);
      const spriter::ImagePosition& image_position = result.image_position(i);
      future->Realize(new_url_str,
                      image_position.clip_rect().x_pos(),
                      image_position.clip_rect().y_pos());
      delete future;
    }
    if (img_file_count_reduction_ != NULL) {
      img_file_count_reduction_->Add(n - 1);
    }
    return true;
  }

  virtual void Clear() {
    ResourceCombinerTemplate<SpriteFuture*>::Clear();
    library_.Clear();
  }

 private:
  Library library_;
  Variable* img_file_count_reduction_;
};

// TODO(jmaessen): The addition of 1 below avoids the leading ".";
// make this convention consistent and fix all code.
ImgCombineFilter::ImgCombineFilter(RewriteDriver* driver,
                                   const char* filter_prefix)
    : RewriteFilter(driver, filter_prefix) {
  combiner_.reset(new Combiner(driver, filter_prefix,
                               kContentTypePng.file_extension() + 1,
                               this));
}

ImgCombineFilter::~ImgCombineFilter() {
}

void ImgCombineFilter::Initialize(Statistics* statistics) {
  statistics->AddVariable(kImgFileCountReduction);
}

bool ImgCombineFilter::Fetch(OutputResource* resource,
                             Writer* writer,
                             const RequestHeaders& request_header,
                             ResponseHeaders* response_headers,
                             MessageHandler* message_handler,
                             UrlAsyncFetcher::Callback* callback) {
  return combiner_->Fetch(resource, writer, request_header, response_headers,
                          message_handler, callback);
}

bool ImgCombineFilter::AddCssBackground(const GoogleUrl& original_url,
                                        Css::Declarations* declarations,
                                        Css::Values* values,
                                        int value_index,
                                        MessageHandler* handler) {
  // We must rule out repeating backgrounds.  Since repeating is the default
  // behavior, we must find a no-repeat somewhere

  // TODO(abliss): support same-sized vertically-repeating backgrounds in a
  // horizontal sprite, and horizontal ones in a vertical sprite.
  // TODO(abliss): skip this check if the element is the same size as the image.
  bool repeat = true;
  for (Css::Declarations::iterator decl_iter = declarations->begin();
       decl_iter != declarations->end(); ++decl_iter) {
    Css::Declaration* decl = *decl_iter;
    // Only edit image declarations.
    switch (decl->prop()) {
      case Css::Property::BACKGROUND_REPEAT: {
        const Css::Values* decl_values = decl->values();
        for (Css::Values::const_iterator value_iter = decl_values->begin();
             value_iter != decl_values->end(); ++value_iter) {
          Css::Value* value = *value_iter;
          if (value->GetLexicalUnitType() == Css::Value::IDENT) {
            switch (value->GetIdentifier().ident()) {
              case Css::Identifier::REPEAT:
              case Css::Identifier::REPEAT_X:
              case Css::Identifier::REPEAT_Y:
                return false;
              case Css::Identifier::NO_REPEAT:
                repeat = false;
              default:
                break;
            }
          }
        }
      }
      case Css::Property::BACKGROUND: {
        const Css::Values* decl_values = decl->values();
        for (Css::Values::const_iterator value_iter = decl_values->begin();
             value_iter != decl_values->end(); ++value_iter) {
          Css::Value* value = *value_iter;
          if (value->GetLexicalUnitType() == Css::Value::IDENT) {
            switch (value->GetIdentifier().ident()) {
              case Css::Identifier::REPEAT:
              case Css::Identifier::REPEAT_X:
              case Css::Identifier::REPEAT_Y:
                return false;
              case Css::Identifier::NO_REPEAT:
                repeat = false;
              default:
                break;
            }
          }
        }
      }
      default:
        break;
    }
  }
  if (repeat) {
    return false;
  }
  SpriteFuture* future = new SpriteFuture();
  if (combiner_->AddElement(future, original_url.Spec(), handler)) {
    future->declarations_ = declarations;
    future->values_ = values;
    future->value_index_ = value_index;
    return true;
  } else {
    delete future;
    return false;
  }
}

bool ImgCombineFilter::DoCombine(MessageHandler* handler) {
  return combiner_->Realize(handler);
}

void ImgCombineFilter::Reset() {
  combiner_->Reset();
}

}  // namespace net_instaweb
