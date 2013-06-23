/*
 * Copyright 2013 Google Inc.
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

// Author: Huibao Lin

#include "pagespeed/kernel/image/image_resizer.h"

#include <math.h>

#include "base/logging.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/scanline_utils.h"

namespace pagespeed {

namespace {

// Table for storing the resizing coefficients.
//
// Both the horizontal resizer and vertical resizer have their own resizing
// tables, but they are used in the similar way. The following example is for
// the horizontal resizer, while similar applies to the vertical resizer.
//
// Each entry specifies an output column. The output column are computed
// by weighting the column at first_index_ with first_weight_, the column at
// last_index_ with last_weight_, and the columns in between with 1.
// The output column is then normalized by the total weights.
//
// Range of first_weight_ and last_weight_ are (0, 1] and [0, 1], respectively.
// Note that first_weight_ cannot be 0 while last_weight_ can.
//
// The input image is uniquely divided into the entries as follows:
// if entry[i].last_weight_ is not 0 nor 1 then
//    entry[i+1].first_index_ = entry[i].last_index_
//    entry[i+1].first_weight_ = 1 - entry[i].last_weight_
// otherwise
//    entry[i+1].first_index_ = entry[i].last_index_ + 1
//    entry[i+1].first_weight_ = 1 (note that resize ratio >= 1)
//
// There are some difference between the tables. For the horizontal resizer,
// the indices refer to the left border of the image and its unit is color
// component. For the vertical resizer, the indices refer to the top border
// of the buffer (which is smaller than the image) and its unit is row.
//
struct ResizeTableEntry {
  int first_index_;
  float first_weight_;
  int last_index_;
  float last_weight_;
};

// Round to the nearest integer.
inline float Round(float val) {
  return lrintf(val);
}

// Check if the value is very close to the specific integer.
// This function will be used to assist IsApproximatelyZero()
// and IsApproximatelyInteger(), which will be used to optimize
// interpolation coefficients for the "area" method.
//
// The "area" method basically divides the input image into grids.
// Each grid corresponds to an output pixel and the average value
// of the input pixels within the grid determines the value for the
// output pixel. When the grid does not align with the border of
// input pixels, some input pixels will be involved to compute
// multiple (2) output pixels. When the difference between the grid
// and the border of input pixel is small, we can ignore the difference.
// Therefore we can save computation because one input pixel will only
// be used to compute one output pixel. The numerical results shall
// not have a noticable difference because we quantize the ouput to intgers
// of 0...255.
//
inline bool IsCloseToFloat(float val, float int_val) {
  const float kThreshold = 1.0E-10;
  float difference = fabs(val - int_val);
  bool is_integer = difference <= kThreshold;
  return is_integer;
}

inline bool IsApproximatelyZero(float val) {
  return IsCloseToFloat(val, 0.0);
}

inline bool IsApproximatelyInteger(float val) {
  return IsCloseToFloat(val, Round(val));
}

// Compute the interpolation coefficents for the "area" method.
// Reference for the "area" resizing method:
// http://opencv.willowgarage.com/documentation/cpp/
//     geometric_image_transformations.html
//
// The inputs, in_size and out_size, are 1-D sizes specified in pixels.
//
ResizeTableEntry* CreateTableForAreaMethod(int in_size,
                                           int out_size,
                                           float ratio) {
  if (in_size <= 0 || out_size <= 0 || ratio <= 0) {
    LOG(DFATAL) << "The inputs must be positive values.";
    return NULL;
  }
  ResizeTableEntry* table = new ResizeTableEntry[out_size];
  if (table == NULL) {
    LOG(DFATAL) << "Failed to allocate memory.";
    return NULL;
  }

  memset(table, 0, sizeof(*table));
  float end_pos = 0;
  for (int i = 0; i < out_size; ++i) {
    float start_pos = end_pos;
    float start_pos_floor = floor(start_pos);
    table[i].first_index_ = static_cast<int>(start_pos_floor);
    table[i].first_weight_ = 1.0 + start_pos_floor - start_pos;

    end_pos += ratio;
    if (IsApproximatelyInteger(end_pos)) {
      end_pos = Round(end_pos);
      table[i].last_index_ = static_cast<int>(end_pos) - 1;
    } else {
      table[i].last_index_ = static_cast<int>(end_pos);
    }

    // If the current dimension is set to have the same resizing ratio as the
    // other dimension, 'last_index_' may be greater than in_size. This is
    // because out_size was computed as Round(in_size / ratio), so
    // last_index_ == out_size * ratio == Round(in_size / ratio) * ratio
    // might be greater than in_size by (0.5 * ratio), where ratio >= 1.
    //
    if (table[i].last_index_ >= in_size) {
      table[i].last_index_ = in_size - 1;
    }

    if (table[i].first_index_ < table[i].last_index_) {
      table[i].last_weight_ = end_pos - table[i].last_index_;
    } else {
      table[i].last_weight_ = ratio - table[i].first_weight_;
    }
  }

  return table;
}

// Compute the output size and resizing ratios. Either output_width or
// output_height, or both, must be postive values.
void ComputeResizedSizeRatio(int input_width,
                             int input_height,
                             int output_width,
                             int output_height,
                             int* width,
                             int* height,
                             float* ratio_x,
                             float* ratio_y) {
  float original_width = static_cast<float>(input_width);
  float original_height = static_cast<float>(input_height);
  float resized_width = static_cast<float>(output_width);
  float resized_height = static_cast<float>(output_height);

  if (resized_width > 0.0 && resized_height > 0.0) {
    *ratio_x = original_width / resized_width;
    *ratio_y = original_height / resized_height;
  } else if (resized_width > 0.0) {
    *ratio_x = original_width / resized_width;
    *ratio_y = *ratio_x;
    resized_height = Round(original_height / *ratio_y);
  } else if (resized_height > 0.0) {
    *ratio_y = original_height / resized_height;
    *ratio_x = *ratio_y;
    resized_width = Round(original_width / *ratio_x);
  }

  *width = static_cast<int>(resized_width);
  *height = static_cast<int>(resized_height);
}

}  // namespace

namespace image_compression {

// Base class for the horizontal resizer.
class ResizeRow {
 public:
  virtual ~ResizeRow() {}
  virtual void Resize(const void* in_data_ptr, void* out_data_ptr) = 0;
  virtual bool Initialize(int in_size, int out_size, float ratio) = 0;
};

// Base class for the vertical resizer.
class ResizeCol {
 public:
  virtual ~ResizeCol() {}

  virtual bool Initialize(int in_size,
                          int out_size,
                          float ratio_x,
                          float ratio_y,
                          int elements_per_output_row) = 0;

  // To compute an output scanline, multiple input scanlines may be required.
  // The following code shows an example.
  //
  // resizer_y_->InitializeResize();
  // while (resizer_y_->NeedMoreScanlines()) {
  //   ...
  //   resizer_x_->Resize(input_scanline, buffer);
  //   resizer_y_->Resize(buffer, output_scanline);
  // }
  virtual void InitializeResize() {}
  virtual bool NeedMoreScanlines() const = 0;
  virtual void Resize(const void* in_data_ptr, void* out_data_ptr) = 0;
};

// Base class for the horizontal resizer using the "area" method.
template<class OutputType, int num_channels>
class ResizeRowArea : public ResizeRow {
 public:
  virtual void Resize(const void* in_data_ptr, void* out_data_ptr);
  virtual bool Initialize(int in_size, int out_size, float ratio);

 protected:
  int pixels_per_row_;
  scoped_array<ResizeTableEntry> table_;
};

template<class OutputType, int num_channels>
bool ResizeRowArea<OutputType, num_channels>::Initialize(int in_size,
    int out_size, float ratio) {
  table_.reset(CreateTableForAreaMethod(in_size, out_size, ratio));
  if (table_ != NULL) {
    // Modify the indices so they are based on bytes instead of pixels.
    for (int i = 0; i < out_size; ++i) {
      table_[i].first_index_ *= num_channels;
      table_[i].last_index_ *= num_channels;
    }
    pixels_per_row_ = out_size;
    return true;
  }
  return false;
}

template<class OutputType, int num_channels>
void ResizeRowArea<OutputType, num_channels>::Resize(
    const void* in_data_ptr, void* out_data_ptr) {
  const uint8_t* in_data = reinterpret_cast<const uint8_t*>(in_data_ptr);
  OutputType* out_data = reinterpret_cast<OutputType*>(out_data_ptr);

  int out_idx = 0;
  for (int x = 0; x < pixels_per_row_; ++x) {
    int in_idx = table_[x].first_index_;
    OutputType weight = static_cast<OutputType>(table_[x].first_weight_);

    // Accumulate the first input pixel.
    switch (num_channels) {
      case 4:
        out_data[out_idx+3] = in_data[in_idx+3] * weight;
        FALLTHROUGH_INTENDED;
      case 3:
        out_data[out_idx+2] = in_data[in_idx+2] * weight;
        out_data[out_idx+1] = in_data[in_idx+1] * weight;
        FALLTHROUGH_INTENDED;
      case 1:
        out_data[out_idx] = in_data[in_idx] * weight;
        break;
    }
    in_idx += num_channels;

    while (in_idx < table_[x].last_index_) {
      // Accumulate the input pixels which contribute 100% to the current output
      // pixel.
      switch (num_channels) {
        case 4:
          out_data[out_idx+3] += in_data[in_idx+3];
          FALLTHROUGH_INTENDED;
        case 3:
          out_data[out_idx+2] += in_data[in_idx+2];
          out_data[out_idx+1] += in_data[in_idx+1];
          FALLTHROUGH_INTENDED;
        case 1:
          out_data[out_idx] += in_data[in_idx];
          break;
      }
      in_idx += num_channels;
    }

    // Accumulate the last input pixel.
    weight = static_cast<OutputType>(table_[x].last_weight_);
    // In table_, last_index_ may equal to first_index_, so we need to reset
    // in_idx to last_index_.
    in_idx = table_[x].last_index_;
    switch (num_channels) {
      case 4:
        out_data[out_idx+3] += in_data[in_idx+3] * weight;
        FALLTHROUGH_INTENDED;
      case 3:
        out_data[out_idx+2] += in_data[in_idx+2] * weight;
        out_data[out_idx+1] += in_data[in_idx+1] * weight;
        FALLTHROUGH_INTENDED;
      case 1:
        out_data[out_idx] += in_data[in_idx] * weight;
        break;
    }
    out_idx += num_channels;
  }
}

// Vertical resizer for all pixel formats using the "area" method.
template<class InputType, class BufferType>
class ResizeColArea : public ResizeCol {
 public:
  virtual void Resize(const void* in_data_ptr, void* out_data_ptr);

  virtual bool Initialize(int in_size,
                          int out_size,
                          float ratio_x,
                          float ratio_y,
                          int elements_per_output_row);

  void InitializeResize() {
    need_more_scanlines_ = true;
  }

  bool NeedMoreScanlines() const {
    return need_more_scanlines_;
  }

 private:
  void AppendFirstRow(const InputType* in_data, BufferType weight);
  void AppendMiddleRow(const InputType* in_data);
  void AppendLastRow(const InputType* in_data, BufferType weight);
  void ComputeOutput(uint8_t* out_data);

 private:
  scoped_array<ResizeTableEntry> table_;
  scoped_array<BufferType> buffer_;
  int elements_per_row_;
  // elements_per_row_4_ is the largest multiple of 4 which is smaller than
  // elements_per_row_.
  int elements_per_row_4_;
  int in_row_;
  int out_row_;
  bool need_more_scanlines_;
  BufferType grid_area_;
  BufferType half_grid_area_;
};

template<class InputType, class BufferType>
bool ResizeColArea<InputType, BufferType>::Initialize(
    int in_size,
    int out_size,
    float ratio_x,
    float ratio_y,
    int elements_per_output_row) {
  table_.reset(CreateTableForAreaMethod(in_size, out_size, ratio_y));
  if (table_ == NULL) {
    return false;
  }

  buffer_.reset(new BufferType[elements_per_output_row]);
  if (buffer_ == NULL) {
    return false;
  }

  grid_area_ = static_cast<BufferType>(ratio_x)
      * static_cast<BufferType>(ratio_y);
  half_grid_area_ = grid_area_ / 2;
  in_row_ = 0;
  out_row_ = 0;
  need_more_scanlines_ = true;
  elements_per_row_ = elements_per_output_row;
  // elements_per_row_4_ is the largest multiplier of 4 which is smaller than
  // elements_per_row_.
  elements_per_row_4_ = ((elements_per_output_row >> 2) << 2);
  return true;
}

// To speed up computation, loop unrolling is used in AppendFirstRow()
// AppendMiddleRow(), AppendLastRow(), and ComputeOutput().
//
template<class InputType, class BufferType>
void ResizeColArea<InputType, BufferType>::AppendFirstRow(
    const InputType* in_data, BufferType weight) {
  int index = 0;
  for (; index < elements_per_row_4_; index+=4) {
    buffer_[index]   = weight * static_cast<BufferType>(in_data[index]);
    buffer_[index+1] = weight * static_cast<BufferType>(in_data[index+1]);
    buffer_[index+2] = weight * static_cast<BufferType>(in_data[index+2]);
    buffer_[index+3] = weight * static_cast<BufferType>(in_data[index+3]);
  }
  for (; index < elements_per_row_; ++index) {
    buffer_[index] = weight * static_cast<BufferType>(in_data[index]);
  }
}

template<class InputType, class BufferType>
void ResizeColArea<InputType, BufferType>::AppendMiddleRow(
    const InputType* in_data) {
  int index = 0;
  for (; index < elements_per_row_4_; index+=4) {
    buffer_[index]   += static_cast<BufferType>(in_data[index]);
    buffer_[index+1] += static_cast<BufferType>(in_data[index+1]);
    buffer_[index+2] += static_cast<BufferType>(in_data[index+2]);
    buffer_[index+3] += static_cast<BufferType>(in_data[index+3]);
  }
  for (; index < elements_per_row_; ++index) {
    buffer_[index] += static_cast<BufferType>(in_data[index]);
  }
}

template<class InputType, class BufferType>
void ResizeColArea<InputType, BufferType>::AppendLastRow(
    const InputType* in_data, BufferType weight) {
  int index = 0;
  for (; index < elements_per_row_4_; index+=4) {
    buffer_[index]   += weight * static_cast<BufferType>(in_data[index]);
    buffer_[index+1] += weight * static_cast<BufferType>(in_data[index+1]);
    buffer_[index+2] += weight * static_cast<BufferType>(in_data[index+2]);
    buffer_[index+3] += weight * static_cast<BufferType>(in_data[index+3]);
  }
  for (; index < elements_per_row_; ++index) {
    buffer_[index] += weight * static_cast<BufferType>(in_data[index]);
  }
}

template<class InputType, class BufferType>
void ResizeColArea<InputType, BufferType>::ComputeOutput(uint8_t* out_data) {
  int index = 0;
  for (; index < elements_per_row_4_; index+=4) {
    out_data[index] = static_cast<uint8_t>((
        buffer_[index] + half_grid_area_) / grid_area_);
    out_data[index+1] = static_cast<uint8_t>((
        buffer_[index+1] + half_grid_area_) / grid_area_);
    out_data[index+2] = static_cast<uint8_t>((
        buffer_[index+2] + half_grid_area_) / grid_area_);
    out_data[index+3] = static_cast<uint8_t>((
        buffer_[index+3] + half_grid_area_) / grid_area_);
  }
  for (; index < elements_per_row_; ++index) {
    out_data[index] = static_cast<uint8_t>((
        buffer_[index] + half_grid_area_) / grid_area_);
  }
}

// Resize the image vertically and output a row.
//
template<class InputType, class BufferType>
void ResizeColArea<InputType, BufferType>::Resize(const void* in_data_ptr,
                                                  void* out_data_ptr) {
  const InputType* in_data = reinterpret_cast<const InputType*>(in_data_ptr);
  const ResizeTableEntry& table = table_[out_row_];
  need_more_scanlines_ = (in_row_ < table.last_index_);

  if (in_row_ == table.first_index_) {
    BufferType weight = static_cast<BufferType>(table.first_weight_);
    AppendFirstRow(in_data, weight);
  } else if (in_row_ < table.last_index_) {
    AppendMiddleRow(in_data);
  } else {
    BufferType weight = static_cast<BufferType>(table.last_weight_);
    if (weight > 0) {
      AppendLastRow(in_data, weight);
    }
  }

  // If we have enough input scanlines, we can compute the output scanline.
  if (!need_more_scanlines_) {
    uint8_t* out_data = reinterpret_cast<uint8_t*>(out_data_ptr);
    ComputeOutput(out_data);

    // If 'last_weight_' is not 0 or 1, the current input scanline shall
    // be used for computing the next output scanline too.
    ++out_row_;
    BufferType weight = static_cast<BufferType>(table.last_weight_);
    if (weight > 0 && weight < 1) {
      weight = static_cast<BufferType>(table_[out_row_].first_weight_);
      AppendFirstRow(in_data, weight);
    }
  }
  ++in_row_;
}

// Instantiate the resizers. It is based on the pixel format as well as the
// resizing ratios.
template<class ResizeXType, class ResizeYType>
bool InstantiateResizers(pagespeed::image_compression::PixelFormat pixel_format,
                         scoped_ptr<ResizeRow>* resizer_x,
                         scoped_ptr<ResizeCol>* resizer_y) {
  bool is_ok = true;
  switch (pixel_format) {
    case GRAY_8:
      resizer_x->reset(new ResizeRowArea<ResizeXType, 1>());
      break;
    case RGB_888:
      resizer_x->reset(new ResizeRowArea<ResizeXType, 3>());
      break;
    case RGBA_8888:
      resizer_x->reset(new ResizeRowArea<ResizeXType, 4>());
      break;
    default:
      LOG(DFATAL) << "Invalid pixel format.";
      is_ok = false;
  }
  resizer_y->reset(new ResizeColArea<ResizeXType, ResizeYType>());
  is_ok = (is_ok && resizer_x != NULL && resizer_y != NULL);
  return is_ok;
}

ScanlineResizer::ScanlineResizer()
  : reader_(NULL),
    width_(0),
    height_(0),
    elements_per_row_(0),
    row_(0),
    bytes_per_buffer_row_(0) {
}

ScanlineResizer::~ScanlineResizer() {
}

// Reset the scanline reader to its initial state.
bool ScanlineResizer::Reset() {
  reader_ = NULL;
  width_ = 0;
  height_ = 0;
  elements_per_row_ = 0;
  row_ = 0;
  bytes_per_buffer_row_ = 0;
  return true;
}

// Reads the next available scanline.
bool ScanlineResizer::ReadNextScanline(void** out_scanline_bytes) {
  if (reader_ == NULL || !HasMoreScanLines()) {
    return false;
  }

  // Fetch scanlines from the reader until we have enough input rows for
  // computing an output row.
  resizer_y_->InitializeResize();
  while (resizer_y_->NeedMoreScanlines()) {
    if (!reader_->HasMoreScanLines()) {
      return false;
    }
    void* in_scanline_bytes = NULL;
    if (!reader_->ReadNextScanline(&in_scanline_bytes)) {
      Reset();
      return false;
    }

    // Resize the input scanline horizontally and put the results in buffer_.
    resizer_x_->Resize(in_scanline_bytes, buffer_.get());
    resizer_y_->Resize(buffer_.get(), output_.get());
  }

  *out_scanline_bytes = output_.get();
  ++row_;

  return true;
}

// Initialize the resizer. For computational efficiency, we try to use
// integer for internal compuation and buffer if it is possible. In particular,
// - If both ratio_x and ratio_y are integers, use integer for all computation;
// - If ratio_x is an integer but ratio_y is not, use integer for the
//   horizontal resizer and floating point for the vertical resizer;
// - Otherwise, use floating point for all compuation.
//
bool ScanlineResizer::Initialize(ScanlineReaderInterface* reader,
                                 size_t output_width,
                                 size_t output_height) {
  if (reader == NULL ||
      reader->GetImageWidth() == 0 ||
      reader->GetImageHeight() == 0) {
    LOG(DFATAL) << "The input image cannot be empty.";
    return false;
  }

  if (output_width == kPreserveAspectRatio &&
      output_height == kPreserveAspectRatio) {
    LOG(DFATAL) << "Output width and height cannot be kPreserveAspectRatio "
                << "at the same time.";
    return false;
  }

  const int input_width = static_cast<int>(reader->GetImageWidth());
  const int input_height = static_cast<int>(reader->GetImageHeight());
  int resized_width, resized_height;
  float ratio_x, ratio_y;

  ComputeResizedSizeRatio(input_width,
                          input_height,
                          static_cast<int>(output_width),
                          static_cast<int>(output_height),
                          &resized_width,
                          &resized_height,
                          &ratio_x,
                          &ratio_y);

  if (ratio_x < 1 || ratio_y < 1) {
    // We are using the "area" method for resizing image. This method is good
    // for shrinking, but not enlarging.
    LOG(DFATAL) << "Enlarging image is not supported";
    return false;
  }

  const bool is_ratio_x_integer = IsApproximatelyInteger(ratio_x);
  const bool is_ratio_y_integer = IsApproximatelyInteger(ratio_y);

  reader_ = reader;
  height_ = resized_height;
  width_ = resized_width;
  row_ = 0;
  const PixelFormat pixel_format = reader->GetPixelFormat();
  elements_per_row_ = resized_width *
      pagespeed::image_compression::GetNumChannelsFromPixelFormat(pixel_format);

  if (is_ratio_x_integer && is_ratio_y_integer) {
    // Use uint32_t for buffer and intermediate computation.
    InstantiateResizers<uint32_t, uint32_t>(pixel_format, &resizer_x_,
                                            &resizer_y_);
    bytes_per_buffer_row_ = elements_per_row_ * sizeof(uint32_t);
  } else if (is_ratio_x_integer) {
    // Use uint32_t for horizontal resizer and float for vertical resizer.
    InstantiateResizers<uint32_t, float>(pixel_format, &resizer_x_,
                                         &resizer_y_);
    bytes_per_buffer_row_ = elements_per_row_ * sizeof(uint32_t);
  } else {
    // Use float for buffer and intermediate computation.
    InstantiateResizers<float, float>(pixel_format, &resizer_x_, &resizer_y_);
    bytes_per_buffer_row_ = elements_per_row_ * sizeof(float);
  }
  buffer_.reset(new uint8_t[bytes_per_buffer_row_]);
  output_.reset(new uint8_t[elements_per_row_]);

  if (buffer_ == NULL || output_ == NULL) {
    return false;
  }

  if (!resizer_x_->Initialize(input_width, resized_width, ratio_x)) {
    return false;
  }
  if (!resizer_y_->Initialize(input_height, resized_height,
                              ratio_x, ratio_y, elements_per_row_)) {
    return false;
  }

  return true;
}

}  // namespace image_compression

}  // namespace pagespeed
