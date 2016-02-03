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
#include <algorithm>

#include "base/logging.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/image/scanline_utils.h"

namespace pagespeed {

namespace {

using net_instaweb::MessageHandler;

// Table for storing the resizing coefficients.
//
// Both the horizontal resizer and vertical resizer have their own resizing
// tables, but they are used in a similar way. The following example is for
// the horizontal resizer. The same example can be used for the vertical
// resizer, if "column" is replaced by "row".
//
// Each entry specifies an output column. The output columns are computed
// by weighting the column at first_index with first_weight, the column at
// last_index with last_weight, and the columns in between with 1.
// The output column is then normalized by the total weights.
//
// Range of first_weight and last_weight are (0, 1] and [0, 1], respectively.
// Note that first_weight cannot be 0 while last_weight can.
//
// The input image is uniquely divided into the entries as follows:
// if entry[i].last_weight is not 0 nor 1 then
//    entry[i+1].first_index = entry[i].last_index
//    entry[i+1].first_weight = 1 - entry[i].last_weight
// otherwise
//    entry[i+1].first_index = entry[i].last_index + 1
//    entry[i+1].first_weight = 1 (note that resize ratio >= 1)
//
// There are some differences between the tables. For the horizontal resizer,
// the indices refer to the left border of the image and its unit is color
// component. For the vertical resizer, the indices refer to the top border
// of the buffer (which is smaller than the image) and its unit is row.
struct ResizeTableEntry {
  int first_index;
  int last_index;
  float first_weight;
  float last_weight;
};

// Round to the nearest integer.
inline double Round(double val) {
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
// not have a noticeable difference because we quantize the output to integers
// of 0...255.
inline bool IsCloseToDouble(double val, double int_val) {
  // Threshold for determining whether a double-precision value is close enough
  // to an integer. A larger threshold increases the chance for the value to
  // be approximated by an integer and consequently reduces computation, but it
  // may also reduce accuracy. The value of 1.0E-10 was empirically chosen.
  const double kThreshold = 1.0E-10;
  double difference = fabs(val - int_val);
  bool is_integer = difference <= kThreshold;
  return is_integer;
}

inline bool IsApproximatelyInteger(double val) {
  return IsCloseToDouble(val, Round(val));
}

// Compute the interpolation coefficents for the "area" method.
// Reference for the "area" resizing method:
// http://opencv.willowgarage.com/documentation/cpp/
//     geometric_image_transformations.html
//
// The inputs, in_size and out_size, are 1-D sizes specified in pixels.
ResizeTableEntry* CreateTableForAreaMethod(int in_size,
                                           int out_size,
                                           double ratio,
                                           MessageHandler* handler) {
  if (in_size <= 0 || out_size <= 0 || ratio <= 0) {
    PS_LOG_DFATAL(handler, "The inputs must be positive values.");
    return NULL;
  }
  ResizeTableEntry* table = new ResizeTableEntry[out_size];
  if (table == NULL) {
    PS_LOG_DFATAL(handler, "Failed to allocate memory.");
    return NULL;
  }

  double end_pos = 0;
  for (int i = 0; i < out_size; ++i) {
    double start_pos = end_pos;
    double start_pos_floor = floor(start_pos);
    table[i].first_index = static_cast<int>(start_pos_floor);
    table[i].first_weight =
        static_cast<float>(1.0 + start_pos_floor - start_pos);

    end_pos = (i + 1) * ratio;
    if (IsApproximatelyInteger(end_pos)) {
      end_pos = Round(end_pos);
      table[i].last_index = static_cast<int>(end_pos) - 1;
    } else {
      table[i].last_index = static_cast<int>(end_pos);
    }

    // If the current dimension is set to have the same resizing ratio as the
    // other dimension, 'last_index' may be greater than in_size. This is
    // because out_size was computed as Round(in_size / ratio), so
    // last_index == out_size * ratio == Round(in_size / ratio) * ratio
    // might be greater than in_size by (0.5 * ratio), where ratio >= 1.
    if (table[i].last_index >= in_size) {
      table[i].last_index = in_size - 1;
    }

    if (table[i].first_index < table[i].last_index) {
      table[i].last_weight = static_cast<float>(end_pos - table[i].last_index);
    } else {
      table[i].last_weight = static_cast<float>(ratio - table[i].first_weight);
    }

    if (i > 0 && table[i - 1].first_index >= table[i].first_index) {
      LOG(DFATAL) << "Significant rounding error has been accumulated.";
      return NULL;
    }
  }

  return table;
}

// Compute the output size and resizing ratios. Either output_width or
// output_height, or both, must be positive values.
void ComputeResizedSizeRatio(int input_width,
                             int input_height,
                             int output_width,
                             int output_height,
                             int* width,
                             int* height,
                             double* ratio_x,
                             double* ratio_y,
                             MessageHandler* handler) {
  double original_width = input_width;
  double original_height = input_height;
  double resized_width = output_width;
  double resized_height = output_height;

  if (resized_width > 0.0 && resized_height > 0.0) {
    *ratio_x = original_width / resized_width;
    *ratio_y = original_height / resized_height;
  } else if (resized_width > 0.0) {
    *ratio_x = original_width / resized_width;
    *ratio_y = *ratio_x;
    resized_height = Round(original_height / *ratio_y);
  } else if (resized_height > 0.0) {
    *ratio_x = original_height / resized_height;
    *ratio_y = *ratio_x;
    resized_width = Round(original_width / *ratio_x);
  } else {
    PS_LOG_DFATAL(handler,
                  "Either output_width or output_height, or both must be " \
                  "positive.");
    *ratio_x = 0;
    *ratio_y = 0;
  }

  *width = static_cast<int>(resized_width);
  *height = static_cast<int>(resized_height);
}

// ResizeRowAreaGray, ResizeRowAreaRGB, and ResizeRowAreaRGBA resize a
// scanline of pixels of different formats. They process every pixel in the
// image and do the most expensive computation for resizing an image. To
// minimize conditional jumps and take advantage of cache prediction, so as to
// improve speed, these methods are implemented as independent function without
// reusing code.
void ResizeRowAreaGray(const ResizeTableEntry* table, int pixels_per_row,
                       const uint8_t* in_data, float* out_data) {
  for (int out_idx = 0; out_idx < pixels_per_row; ++out_idx) {
    const ResizeTableEntry& table_entry = table[out_idx];

    // Accumulate the first input pixel.
    float acc1 = in_data[table_entry.first_index] * table_entry.first_weight;

    // Accumulate the intermediate input pixels which contribute 100% to the
    // current output pixel.
    for (int in_idx = table_entry.first_index + 1;
         in_idx < table_entry.last_index;
         ++in_idx) {
      acc1 += in_data[in_idx];
    }

    // Accumulate the last input pixel.
    acc1 += in_data[table_entry.last_index] * table_entry.last_weight;
    out_data[out_idx] = acc1;
  }
}

void ResizeRowAreaRGB(const ResizeTableEntry* table, int pixels_per_row,
                      const uint8_t* in_data, float* out_data) {
  int out_idx = 0;
  for (int x = 0; x < pixels_per_row; ++x) {
    const ResizeTableEntry& table_entry = table[x];

    // Accumulate the first input pixel.
    int in_idx = table_entry.first_index;
    float weight = table_entry.first_weight;
    float acc1 = in_data[in_idx] * weight;
    float acc2 = in_data[in_idx + 1] * weight;
    float acc3 = in_data[in_idx + 2] * weight;

    // Accumulate the intermediate input pixels which contribute 100% to the
    // current output pixel.
    for (in_idx += 3; in_idx < table_entry.last_index; in_idx += 3) {
      acc1 += in_data[in_idx];
      acc2 += in_data[in_idx + 1];
      acc3 += in_data[in_idx + 2];
    }

    // Accumulate the last input pixel.
    weight = table_entry.last_weight;
    // In table, last_index may equal first_index, so we need to reset
    // in_idx to last_index.
    in_idx = table_entry.last_index;
    acc1 += in_data[in_idx] * weight;
    out_data[out_idx] = acc1;
    acc2 += in_data[in_idx + 1] * weight;
    out_data[out_idx + 1] = acc2;
    acc3 += in_data[in_idx + 2] * weight;
    out_data[out_idx + 2] = acc3;

    out_idx += 3;
  }
}

void ResizeRowAreaRGBA(const ResizeTableEntry* table, int pixels_per_row,
                       const uint8_t* in_data, float* out_data) {
  int out_idx = 0;
  for (int x = 0; x < pixels_per_row; ++x) {
    const ResizeTableEntry& table_entry = table[x];

    // Accumulate the first input pixel.
    int in_idx = table_entry.first_index;
    float weight = table_entry.first_weight;
    float acc1 = in_data[in_idx] * weight;
    float acc2 = in_data[in_idx + 1] * weight;
    float acc3 = in_data[in_idx + 2] * weight;
    float acc4 = in_data[in_idx + 3] * weight;

    // Accumulate the intermediate input pixels which contribute 100% to the
    // current output pixel.
    for (in_idx += 4; in_idx < table_entry.last_index; in_idx += 4) {
      acc1 += in_data[in_idx];
      acc2 += in_data[in_idx + 1];
      acc3 += in_data[in_idx + 2];
      acc4 += in_data[in_idx + 3];
    }

    // Accumulate the last input pixel.
    weight = table_entry.last_weight;
    // In table, last_index may equal first_index, so we need to reset
    // in_idx to last_index.
    in_idx = table_entry.last_index;
    acc1 += in_data[in_idx] * weight;
    out_data[out_idx] = acc1;
    acc2 += in_data[in_idx + 1] * weight;
    out_data[out_idx + 1] = acc2;
    acc3 += in_data[in_idx + 2] * weight;
    out_data[out_idx + 2] = acc3;
    acc4 += in_data[in_idx + 3] * weight;
    out_data[out_idx + 3] = acc4;

    out_idx += 4;
  }
}

}  // namespace

namespace image_compression {
// Resizing an image includes operations in orthogonal directions: resizing
// horizontally and vertically. These operations are independent. So
// ScanlineResizer delegates its work to two classes, ResizeRow and ResizeCol,
// which resize horizontally and vertically, respectively.
//
// To compute an output scanline, multiple input scanlines may be required.
// The following code shows an example.
//
// resizer_y_->InitializeResize();
// while (resizer_y_->NeedMoreScanlines()) {
//   ...
//   const void* buffer = resizer_x_->Resize(input_scanline);
//   *out_scanline = resizer_y_->Resize(buffer);
// }

// Base class for the horizontal resizer. If the object is not initialized,
// or if the object is initialized with 'output_buffer' set to 'NULL',
// Resize() will simply return 'in_data'. This class does not own
// 'output_buffer' nor the buffer which it returns.
class ResizeRow {
 public:
  virtual ~ResizeRow() {}

  virtual bool Initialize(int in_size, int out_size, double ratio,
                          float* output_buffer, MessageHandler* handler) = 0;

  // In order to process pixels stored in any data type, the base class,
  // ScanlineReaderInterface, uses "void*" for the pixel buffer. Consequently,
  // ScanlineResizer, which is derived from ScanlineReaderInterface, uses
  // "void*".
  //
  // The implementation in this file, i.e., ResizeRow, ResizeCol, and the
  // classes derived from them, only support pixels stored in "uint8_t" type.
  //
  // Therefore all internal methods and properties use "uint8_t*", while those
  // that connect directly to the interface of ScanlineResizer use "void*".
  virtual const void* Resize(const uint8_t* in_data) = 0;
};

// Base class for the vertical resizer. If the object is initialized with
// 'output_buffer' set to 'NULL', and resizing ratio set to '1',
// Resize() will simply return 'in_data_ptr'. This class does not own
// 'output_buffer' nor the buffer which it returns.
class ResizeCol {
 public:
  virtual ~ResizeCol() {}
  virtual bool Initialize(int in_size,
                          int out_size,
                          double ratio_x,
                          double ratio_y,
                          int elements_per_output_row,
                          uint8_t* output_buffer,
                          MessageHandler* handler) = 0;
  virtual const uint8_t* Resize(const void* in_data_ptr) = 0;
  virtual void InitializeResize() {}
  virtual bool NeedMoreScanlines() const = 0;
  virtual int out_row() const = 0;
};

// Base class for the horizontal resizer using the "area" method.
class ResizeRowArea : public ResizeRow {
 public:
  explicit ResizeRowArea(int num_channels)
      : num_channels_(num_channels), output_buffer_(NULL) {}

  virtual bool Initialize(int in_size, int out_size, double ratio,
                          float* output_buffer, MessageHandler* handler);
  virtual const void* Resize(const uint8_t* in_data);

 protected:
  const int num_channels_;
  int pixels_per_row_;
  float* output_buffer_;  // Not owned
  net_instaweb::scoped_array<ResizeTableEntry> table_;
};

bool ResizeRowArea::Initialize(int in_size,
    int out_size, double ratio, float* output_buffer, MessageHandler* handler) {
  if (num_channels_ != 1 && num_channels_ != 3 && num_channels_ != 4) {
    return false;
  }
  table_.reset(CreateTableForAreaMethod(in_size, out_size, ratio, handler));
  if (table_ == NULL) {
    return false;
  }

  // Modify the indices so they are based on bytes instead of pixels.
  for (int i = 0; i < out_size; ++i) {
    table_[i].first_index *= num_channels_;
    table_[i].last_index *= num_channels_;
  }
  pixels_per_row_ = out_size;
  output_buffer_ = output_buffer;
  return true;
}

const void* ResizeRowArea::Resize(const uint8_t* in_data) {
  if (output_buffer_ == NULL) {
    return in_data;
  }

  switch (num_channels_) {
    case 1:  // GRAY_8
      ResizeRowAreaGray(table_.get(), pixels_per_row_, in_data, output_buffer_);
      break;
    case 3:  // RGB_888
      ResizeRowAreaRGB(table_.get(), pixels_per_row_, in_data, output_buffer_);
      break;
    case 4:  // RGBA_8888
      ResizeRowAreaRGBA(table_.get(), pixels_per_row_, in_data, output_buffer_);
      break;
  }

  return output_buffer_;
}

// Vertical resizer for all pixel formats using the "area" method.
template<class BufferType>
class ResizeColArea : public ResizeCol {
 public:
  ResizeColArea() : output_buffer_(NULL) {}

  virtual bool Initialize(int in_size,
                          int out_size,
                          double ratio_x,
                          double ratio_y,
                          int elements_per_output_row,
                          uint8_t* output_buffer,
                          MessageHandler* handler);

  virtual const uint8_t* Resize(const void* in_data_ptr);

  virtual int out_row() const {
    return out_row_;
  }

  void InitializeResize() {
    need_more_scanlines_ = true;
  }

  bool NeedMoreScanlines() const {
    return need_more_scanlines_;
  }

 private:
  void AppendFirstRow(const BufferType* in_data, float weight);
  void AppendMiddleRow(const BufferType* in_data);
  void AppendLastRow(const BufferType* in_data, float weight);
  void ComputeOutput(const float* in_data, uint8_t* out_data);

  net_instaweb::scoped_array<ResizeTableEntry> table_;
  net_instaweb::scoped_array<float> buffer_;
  uint8_t* output_buffer_;  // Not owned
  int elements_per_row_;
  // elements_per_row_4_ is the largest multiple of 4 which is smaller than
  // elements_per_row_.
  int elements_per_row_4_;
  int in_row_;
  int out_row_;
  int num_out_rows_;
  bool need_more_scanlines_;
  float inv_grid_area_;
  float half_grid_area_;
  bool only_scale_outputs_;
};

template<class BufferType>
bool ResizeColArea<BufferType>::Initialize(
    int in_size,
    int out_size,
    double ratio_x,
    double ratio_y,
    int elements_per_output_row,
    uint8_t* output_buffer,
    MessageHandler* handler) {
  table_.reset(CreateTableForAreaMethod(in_size, out_size, ratio_y, handler));
  if (table_ == NULL) {
    return false;
  }

  only_scale_outputs_ = (ratio_y == 1.0);
  if (!only_scale_outputs_) {
    buffer_.reset(new float[elements_per_output_row]);
    if (buffer_ == NULL) {
      return false;
    }
  }
  output_buffer_ = output_buffer;

  float grid_area = static_cast<float>(ratio_x * ratio_y);
  inv_grid_area_ = 1.0f / grid_area;
  half_grid_area_ = 0.5f * grid_area;
  in_row_ = 0;
  out_row_ = 0;
  num_out_rows_ = out_size;
  need_more_scanlines_ = true;
  elements_per_row_ = elements_per_output_row;
  // elements_per_row_4_ is the largest multiplier of 4 which is smaller than
  // elements_per_row_.
  elements_per_row_4_ = (elements_per_output_row & ~3);
  return true;
}

// To speed up computation, loop unrolling is used in AppendFirstRow()
// AppendMiddleRow(), AppendLastRow(), and ComputeOutput().
template<class BufferType>
void ResizeColArea<BufferType>::AppendFirstRow(
    const BufferType* in_data, float weight) {
  int index = 0;
  for (; index < elements_per_row_4_; index += 4) {
    buffer_[index] = weight * in_data[index];
    buffer_[index + 1] = weight * in_data[index + 1];
    buffer_[index + 2] = weight * in_data[index + 2];
    buffer_[index + 3] = weight * in_data[index + 3];
  }
  for (; index < elements_per_row_; ++index) {
    buffer_[index] = weight * in_data[index];
  }
}

template<class BufferType>
void ResizeColArea<BufferType>::AppendMiddleRow(
    const BufferType* in_data) {
  int index = 0;
  for (; index < elements_per_row_4_; index += 4) {
    buffer_[index] += in_data[index];
    buffer_[index + 1] += in_data[index + 1];
    buffer_[index + 2] += in_data[index + 2];
    buffer_[index + 3] += in_data[index + 3];
  }
  for (; index < elements_per_row_; ++index) {
    buffer_[index] += in_data[index];
  }
}

template<class BufferType>
void ResizeColArea<BufferType>::AppendLastRow(
    const BufferType* in_data, float weight) {
  int index = 0;
  for (; index < elements_per_row_4_; index += 4) {
    buffer_[index] += weight * in_data[index];
    buffer_[index + 1] += weight * in_data[index + 1];
    buffer_[index + 2] += weight * in_data[index + 2];
    buffer_[index + 3] += weight * in_data[index + 3];
  }
  for (; index < elements_per_row_; ++index) {
    buffer_[index] += weight * in_data[index];
  }
}

template<class BufferType>
void ResizeColArea<BufferType>::ComputeOutput(const float* in_data,
                                              uint8_t* out_data) {
  int index = 0;
  // Make local copies of the data in order to speed up computation.
  const float half_grid_area = half_grid_area_;
  const float inv_grid_area = inv_grid_area_;
  for (; index < elements_per_row_4_; index += 4) {
    out_data[index] = static_cast<uint8_t>((
        in_data[index] + half_grid_area) * inv_grid_area);
    out_data[index + 1] = static_cast<uint8_t>((
        in_data[index + 1] + half_grid_area) * inv_grid_area);
    out_data[index + 2] = static_cast<uint8_t>((
        in_data[index + 2] + half_grid_area) * inv_grid_area);
    out_data[index + 3] = static_cast<uint8_t>((
        in_data[index + 3] + half_grid_area) * inv_grid_area);
  }
  for (; index < elements_per_row_; ++index) {
    out_data[index] = static_cast<uint8_t>((
        in_data[index] + half_grid_area) * inv_grid_area);
  }
}

// Resize the image vertically and output a row.
template<class BufferType>
const uint8_t* ResizeColArea<BufferType>::Resize(const void* in_data_ptr) {
  if (only_scale_outputs_) {
    need_more_scanlines_ = false;
    ++in_row_;
    ++out_row_;

    if (output_buffer_ == NULL) {
      return static_cast<const uint8_t*>(in_data_ptr);
    } else {
      const float* in_data = static_cast<const float*>(in_data_ptr);
      ComputeOutput(in_data, output_buffer_);
      return output_buffer_;
    }
  }

  const BufferType* in_data = reinterpret_cast<const BufferType*>(in_data_ptr);
  const ResizeTableEntry& table_entry = table_[out_row_];
  need_more_scanlines_ = (in_row_ < table_entry.last_index);

  if (in_row_ == table_entry.first_index) {
    AppendFirstRow(in_data, table_entry.first_weight);
  } else if (in_row_ < table_entry.last_index) {
    AppendMiddleRow(in_data);
  } else {
    float weight = table_entry.last_weight;
    if (weight > 0) {
      AppendLastRow(in_data, weight);
    }
  }

  // If we have enough input scanlines, we can compute the output scanline.
  if (!need_more_scanlines_) {
    ComputeOutput(buffer_.get(), output_buffer_);

    // If 'last_weight' is not 0 or 1, the current input scanline shall
    // be used for computing the next output scanline too.
    ++out_row_;
    if (out_row_ < num_out_rows_) {
      float weight = table_entry.last_weight;
      if (weight > 0 && weight < 1) {
        weight = table_[out_row_].first_weight;
        AppendFirstRow(in_data, weight);
      }
    }
  }
  ++in_row_;
  return output_buffer_;
}

// Instantiate the resizers. It is based on the pixel format as well as the
// resizing ratios.
template<class BufferType>
bool InstantiateResizers(pagespeed::image_compression::PixelFormat pixel_format,
                         net_instaweb::scoped_ptr<ResizeRow>* resizer_x,
                         net_instaweb::scoped_ptr<ResizeCol>* resizer_y,
                         MessageHandler* handler) {
  const int num_channels = GetNumChannelsFromPixelFormat(pixel_format, handler);
  resizer_x->reset(new ResizeRowArea(num_channels));
  resizer_y->reset(new ResizeColArea<BufferType>());
  return (resizer_x->get() != NULL && resizer_y->get() != NULL);
}

ScanlineResizer::ScanlineResizer(MessageHandler* handler)
  : reader_(NULL),
    width_(0),
    height_(0),
    elements_per_row_(0),
    bytes_per_buffer_row_(0),
    message_handler_(handler) {
}

ScanlineResizer::~ScanlineResizer() {
}

// Reset the scanline reader to its initial state.
bool ScanlineResizer::Reset() {
  reader_ = NULL;
  width_ = 0;
  height_ = 0;
  elements_per_row_ = 0;
  bytes_per_buffer_row_ = 0;
  return true;
}

bool ScanlineResizer::HasMoreScanLines() {
  return (resizer_y_->out_row() < height_);
}

ScanlineStatus ScanlineResizer::InitializeWithStatus(
    const void* /* image_buffer */,
    size_t /* buffer_length */) {
  return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler_,
                          SCANLINE_STATUS_INVOCATION_ERROR,
                          SCANLINE_RESIZER,
                          "unexpected call to InitializeWithStatus()");
}

// Reads the next available scanline.
ScanlineStatus ScanlineResizer::ReadNextScanlineWithStatus(
    void** out_scanline_bytes) {
  if (reader_ == NULL || !HasMoreScanLines()) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler_,
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            SCANLINE_RESIZER,
                            "null reader or no more scanlines");
  }

  // Fetch scanlines from the reader until we have enough input rows for
  // computing an output row.
  resizer_y_->InitializeResize();
  while (resizer_y_->NeedMoreScanlines()) {
    if (!reader_->HasMoreScanLines()) {
      return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                              SCANLINE_STATUS_INTERNAL_ERROR,
                              SCANLINE_RESIZER,
                              "HasMoreScanLines()");
    }
    void* in_scanline_bytes = NULL;
    ScanlineStatus status = reader_->ReadNextScanlineWithStatus(
        &in_scanline_bytes);
    if (!status.Success()) {
      Reset();
      return status;
    }

    // Resize the input scanline horizontally and put the results in buffer_.
    const void* buffer = resizer_x_->Resize(
        static_cast<uint8_t*>(in_scanline_bytes));
    *out_scanline_bytes = const_cast<uint8_t*>(resizer_y_->Resize(buffer));
  }

  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

// Initialize the resizer. For computational efficiency, we try to use
// integer for internal computation and buffer if it is possible. In particular,
// - If both ratio_x and ratio_y are integers, use integer for all computation;
// - If ratio_x is an integer but ratio_y is not, use integer for the
//   horizontal resizer and floating point for the vertical resizer;
// - Otherwise, use floating point for all computation.
bool ScanlineResizer::Initialize(ScanlineReaderInterface* reader,
                                 size_t request_width,
                                 size_t request_height) {
  if (reader == NULL ||
      reader->GetImageWidth() == 0 ||
      reader->GetImageHeight() == 0) {
    PS_LOG_DFATAL(message_handler_, "The input image cannot be empty.");
    return false;
  }

  if (request_width == kPreserveAspectRatio &&
      request_height == kPreserveAspectRatio) {
    PS_LOG_DFATAL(message_handler_, \
        "Output width and height cannot be kPreserveAspectRatio " \
        "at the same time.");
    return false;
  }

  const int input_width = static_cast<int>(reader->GetImageWidth());
  const int input_height = static_cast<int>(reader->GetImageHeight());

  // TODO(huibao): Truncate the requested image size if it is larger than the
  // input in 'image_rewrite_filter.cc'. Report an error and return 'false'
  // if it is larger than the input in this method.

  // If the request size for either dimension is greater than that of the input,
  // it will be truncated. In other words, the image will not be enlarged.
  if (static_cast<int>(request_width) > input_width ||
      static_cast<int>(request_height) > input_height) {
    PS_DLOG_INFO(message_handler_, \
                 "The requested output size will be truncated because it is " \
                 "larger than the input.");
  }

  const int output_width =
      std::min(static_cast<int>(request_width), input_width);
  const int output_height =
      std::min(static_cast<int>(request_height), input_height);

  int resized_width, resized_height;
  double ratio_x, ratio_y;

  ComputeResizedSizeRatio(input_width,
                          input_height,
                          output_width,
                          output_height,
                          &resized_width,
                          &resized_height,
                          &ratio_x,
                          &ratio_y,
                          message_handler_);

  reader_ = reader;
  height_ = resized_height;
  width_ = resized_width;
  const PixelFormat pixel_format = reader->GetPixelFormat();
  elements_per_row_ = resized_width *
    pagespeed::image_compression::GetNumChannelsFromPixelFormat(
        pixel_format, message_handler_);

  // Ratios           | X Resizer | X Buff | Y Input | Y Resizer      | Y Buff
  // x != 1 && y != 1 | Resize    | Valid  | float   | Resize & Scale | Valid
  // x != 1 && y == 1 | Resize    | Valid  | float   | Scale Only     | Valid
  // x == 1 && y != 1 | Shortcut  | NULL   | uint8   | Resize & Scale | Valid
  // x == 1 && y == 1 | Shortcut  | NULL   | uint8   | Shortcut       | NULL

  const bool need_resize_x = (ratio_x != 1.0);
  const bool need_resize_y = (ratio_y != 1.0);
  float* resizer_x_buffer = NULL;
  uint8_t* resizer_y_buffer = NULL;
  if (need_resize_x) {
    InstantiateResizers<float>(pixel_format, &resizer_x_, &resizer_y_,
                               message_handler_);
    buffer_.reset(new float[elements_per_row_]);
    resizer_x_buffer = buffer_.get();
    output_.reset(new uint8_t[elements_per_row_]);
    resizer_y_buffer = output_.get();
    if (resizer_x_buffer == NULL || resizer_y_buffer == NULL) {
      return false;
    }
  } else {
    InstantiateResizers<uint8_t>(pixel_format, &resizer_x_, &resizer_y_,
                                 message_handler_);
    if (need_resize_y) {
      output_.reset(new uint8_t[elements_per_row_]);
      resizer_y_buffer = output_.get();
      if (resizer_y_buffer == NULL) {
        return false;
      }
    }
  }

  if (!resizer_x_->Initialize(input_width, resized_width, ratio_x,
                              resizer_x_buffer, message_handler_)) {
    return false;
  }
  if (!resizer_y_->Initialize(input_height, resized_height, ratio_x, ratio_y,
                              elements_per_row_, resizer_y_buffer,
                              message_handler_)) {
    return false;
  }

  return true;
}

}  // namespace image_compression

}  // namespace pagespeed
