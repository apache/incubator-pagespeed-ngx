/*
 * Copyright 2014 Google Inc.
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
 *
 * Author: huibao@google.com (Huibao Lin)
 */

goog.provide('pagespeed.MobColor');

goog.require('goog.color');
goog.require('pagespeed.MobUtil');



/**
 * Creates a context for color analysis.
 * @constructor
 */
pagespeed.MobColor = function() {
  /**
   * Callback to invoke when this object finishes its work.
   * @private {function(pagespeed.MobLogo.LogoRecord, !goog.color.Rgb,
   *                    !goog.color.Rgb)}
   */
  this.doneCallback_;

  /**
   * Number of images which we need to load and analyze.
   * @private {number}
   */
  this.numPendingImages_ = 0;

  /**
   * The logo, if there is any.
   * @private {pagespeed.MobLogo.LogoRecord}
   */
  this.logo_ = null;

  /**
   * Data of the logo foreground image.
   * @private {ImageData}
   */
  this.foregroundData_ = null;
};


/**
 * Machine epsilon (EPSILON) used in this file. This is the minimum value to be
 * considered non-zero.
 * @private
 * @const
 */
pagespeed.MobColor.prototype.EPSILON_ = 1e-10;


/**
 * Minimum contrast of theme colors. For 2 colors, contrast is defined as the
 * ratio between the larger brightness and the smaller brightness.
 * @private
 * @const
 */
pagespeed.MobColor.prototype.MIN_CONTRAST_ = 3;



/**
 * Creates a theme color object.
 * @param {!goog.color.Rgb} background
 * @param {!goog.color.Rgb} foreground
 * @struct
 * @constructor
 */
pagespeed.MobColor.ThemeColors = function(background, foreground) {
  /** @type {!goog.color.Rgb} */
  this.background = background;
  /** @type {!goog.color.Rgb} */
  this.foreground = foreground;
};


/**
 * Distance between two RGB colors.
 * @param {!goog.color.Rgb} rgb1
 * @param {!goog.color.Rgb} rgb2
 * @private
 * @return {number}
 */
pagespeed.MobColor.prototype.distance_ = function(rgb1, rgb2) {
  if (rgb1.length != 3 || rgb2.length != 3) {
    return Infinity;
  }
  var dif0 = (rgb1[0] - rgb2[0]);
  var dif1 = (rgb1[1] - rgb2[1]);
  var dif2 = (rgb1[2] - rgb2[2]);
  return Math.sqrt(dif0 * dif0 + dif1 * dif1 + dif2 * dif2);
};


/**
 * Convert a value from sRGB to RGB.
 * http://www.w3.org/TR/2008/REC-WCAG20-20081211/#relativeluminancedef
 * Input range [0, 255], output range [0, 1];
 * @param {number} v255
 * @private
 * @return {number}
 */
pagespeed.MobColor.prototype.srgbToRgb_ = function(v255) {
  var v = v255 / 255;
  if (v <= 0.03928) {
    v = v / 12.92;
  } else {
    v = Math.pow(((v + 0.055) / 1.055), 2.4);
  }
  return v;
};


/**
 * Extract luminance from RGB.
 * @param {!goog.color.Rgb} sRgb
 * @private
 * @return {number}
 */
pagespeed.MobColor.prototype.rgbToGray_ = function(sRgb) {
  var v = 0.2126 * this.srgbToRgb_(sRgb[0]) +
      0.7152 * this.srgbToRgb_(sRgb[1]) +
      0.0722 * this.srgbToRgb_(sRgb[2]);
  return v;
};


/**
 * Enhance colors if they don't have enough contrast. To enhance the contrast,
 * we increase the difference of luminance, but keep their colors.
 * @param {!pagespeed.MobColor.ThemeColors} themeColors
 * @private
 * @return {!pagespeed.MobColor.ThemeColors}
 * @this {pagespeed.MobColor}
 */
pagespeed.MobColor.prototype.enhanceColors_ = function(themeColors) {
  var bk = themeColors.background;
  var fr = themeColors.foreground;
  var bkGray = this.rgbToGray_(bk);
  var frGray = this.rgbToGray_(fr);

  // If both background and foreground are black, we can't enhance the colors.
  if (bkGray < this.EPSILON_ && frGray < this.EPSILON_) {
    return themeColors;
  }

  // If the colors already have enough contrast, we're all set.
  var contrast = frGray / bkGray;
  if (contrast < 1) {
    contrast = 1 / contrast;
  }
  if (contrast > this.MIN_CONTRAST_) {
    return themeColors;
  }

  // To enhance contrast, we convert the colors from RGB to HSV. We keep the
  // hue (H) and saturation (S) components, so the color will not be changed.
  // We increase the contrast by only modifying the luminance (V) component.
  var bkHsv = goog.color.rgbArrayToHsv(bk);
  var frHsv = goog.color.rgbArrayToHsv(fr);

  var minV = null;
  var maxV = null;
  if (bkHsv[2] < frHsv[2]) {
    minV = bkHsv[2];
    maxV = frHsv[2];
  } else {
    minV = frHsv[2];
    maxV = bkHsv[2];
  }

  var delta = ((this.MIN_CONTRAST_ * minV) - maxV) / (this.MIN_CONTRAST_ + 1);
  if (minV > delta) {
    minV = minV - delta;
  } else {
    minV = 0;
  }
  if (maxV < 1 - 2 * delta) {
    maxV += 2 * delta;
  } else {
    maxV = 255;
  }

  if (bkHsv[2] < frHsv[2]) {
    bkHsv[2] = minV;
    frHsv[2] = maxV;
  } else {
    frHsv[2] = minV;
    bkHsv[2] = maxV;
  }

  bk = goog.color.hsvArrayToRgb(bkHsv);
  fr = goog.color.hsvArrayToRgb(frHsv);
  return (new pagespeed.MobColor.ThemeColors(bk, fr));
};


/**
 * Compute the theme colors of an image. The theme colors have two components:
 *   - background: this is the color of the image border if it is not very
 *     transparent (average alpha is greater than 0.5); or the background color
 *     otherwise.
 *   - foreground: this is computed from the image center, by excluding colors
 *     which is close to the background.
 * @param {!Uint8ClampedArray} pixels Pixels in RGBA format
 * @param {!goog.color.Rgb} bkColor Background color in RGB format
 * @param {number} width
 * @param {number} height
 * @private
 * @return {!pagespeed.MobColor.ThemeColors}
 * @this {pagespeed.MobColor}
 */
pagespeed.MobColor.prototype.computeColors_ = function(pixels, bkColor,
                                                       width, height) {
  // Blend the background color into pixels.
  var rgb = [];
  var x, y, idx;
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      var idxIn = 4 * (y * width + x);
      var idxOut = 3 * (y * width + x);
      var rf = pixels[idxIn + 3] / 255;
      var rb = 1 - rf;
      rgb[idxOut] = rf * pixels[idxIn] + rb * bkColor[0];
      rgb[idxOut + 1] = rf * pixels[idxIn + 1] + rb * bkColor[1];
      rgb[idxOut + 2] = rf * pixels[idxIn + 2] + rb * bkColor[2];
    }
  }

  var bk = [0, 0, 0];
  var numBk = 0;
  var alpha = 0;
  for (x = 0; x < width; ++x) {
    var idxPixel = ((height - 1) * width + x);
    idx = 3 * idxPixel;
    bk[0] += rgb[idx];
    bk[1] += rgb[idx + 1];
    bk[2] += rgb[idx + 2];
    alpha += pixels[4 * idxPixel + 3];
    ++numBk;
  }
  // If the border is at least half transparent, we just use the background
  // color without considering the logo image.
  if (alpha > 0.5 * 255 * numBk) {
    for (var i = 0; i < 3; ++i) {
      bk[i] = Math.floor(bk[i] / numBk);
    }
  } else {
    bk = bkColor;
  }

  // Compute the color difference between pixels at image center and the
  // background.
  var xStart = Math.floor(0.25 * width);
  var xEnd = Math.floor(0.75 * width);
  var yStart = Math.floor(0.25 * height);
  var yEnd = Math.floor(0.75 * height);
  var j = 0;
  var disList = [];
  for (y = yStart; y <= yEnd; ++y) {
    for (x = xStart; x <= xEnd; ++x) {
      idx = 3 * (y * width + x);
      disList[j] = this.distance_(rgb.slice(idx, idx + 3), bk);
      ++j;
    }
  }

  // Find out the pixels whose color is most different from the background
  // (top 25%), and average their colors.
  var disSorted = disList.sort(function(a, b) { return a - b; });
  var threshold = Math.max(1, disSorted[Math.floor(j * 0.75)]);

  var numFr = 0;
  var fr = [0, 0, 0];
  for (y = yStart; y <= yEnd; ++y) {
    for (x = xStart; x <= xEnd; ++x) {
      idx = 3 * (y * width + x);
      var d = this.distance_(rgb.slice(idx, idx + 3), bk);
      if (d >= threshold) {
        fr[0] += rgb[idx];
        fr[1] += rgb[idx + 1];
        fr[2] += rgb[idx + 2];
        ++numFr;
      }
    }
  }
  if (numFr > 0) {
    for (var i = 0; i < 3; ++i) {
      fr[i] = Math.floor(fr[i] / numFr);
    }
  }

  // Ehance the colors, if they don't have enough contrast.
  return this.enhanceColors_(new pagespeed.MobColor.ThemeColors(bk, fr));
};


/**
 * Callback to use for synthesizing the logo and hamburger menu icon, and for
 * invoking the latter processing. This method must be called at the end of this
 * object.
 * @private
 * @this {pagespeed.MobColor}
 */
pagespeed.MobColor.prototype.synthesizeCallback_ = function() {
  --this.numPendingImages_;
  if (this.numPendingImages_ > 0) {
    return;
  }

  var backgroundColor = [255, 255, 255];
  var foregroundColor = [0, 0, 0];
  var logo = this.logo_;
  if (this.foregroundData_ && this.foregroundData_.data &&
      logo && logo.foregroundRect && logo.backgroundColor) {
    // TODO(huibao): If foreground is transparent and there is an image
    // behind it, use the image at the back for computing background color.
    backgroundColor = logo.backgroundColor;

    var colors = this.computeColors_(
        /** @type {!Uint8ClampedArray} */ (this.foregroundData_.data),
        logo.backgroundColor, logo.foregroundRect.width,
        logo.foregroundRect.height);

    backgroundColor = colors.background;
    foregroundColor = colors.foreground;
  } else {
    if (logo && logo.backgroundColor) {
      backgroundColor = logo.backgroundColor;
      // If the background color is bright, set the foreground to black;
      // otherwise, set to white.
      var hsv = goog.color.rgbArrayToHsv(backgroundColor);
      if (hsv[2] > 0.7 * 255) {
        foregroundColor = [0, 0, 0];
      } else {
        foregroundColor = [255, 255, 255];
      }
    }
  }

  console.log('Theme color. Background: ' + backgroundColor +
              ' foreground: ' + foregroundColor);

  this.doneCallback_(this.logo_, backgroundColor, foregroundColor);
};


/**
 * Decode an image into pixels.
 * @param {string} mode
 * @param {string} src
 * @param {pagespeed.MobUtil.Rect} rect
 * @private
 * @this {pagespeed.MobColor}
 */
pagespeed.MobColor.prototype.getImageDataAndSynthesize_ = function(mode, src,
                                                                   rect) {
  var imageElement = new Image();
  imageElement.onload = goog.bind(function() {
    var canvas = document.createElement('canvas');
    var width = null;
    var height = null;
    if (rect && rect.width > 0 && rect.height > 0) {
      width = rect.width;
      height = rect.height;
    } else {
      width = imageElement.naturalWidth;
      height = imageElement.naturalHeight;
    }
    canvas.width = width;
    canvas.height = height;
    var context = canvas.getContext('2d');
    context.drawImage(imageElement, 0, 0);
    if (mode == 'foreground') {
      this.foregroundData_ = context.getImageData(0, 0, width, height);
    }
    this.synthesizeCallback_();
  }, this);
  imageElement.onerror = goog.bind(this.synthesizeCallback_, this);
  imageElement.src = src;
};


/**
 * Compute color and synthesize logo span.
 * @param {pagespeed.MobLogo.LogoRecord} logo
 * @param {function(pagespeed.MobLogo.LogoRecord, !goog.color.Rgb,
 *                  !goog.color.Rgb)} doneCallback
 * @this {pagespeed.MobColor}
 */
pagespeed.MobColor.prototype.run = function(logo, doneCallback) {
  this.logo_ = logo;
  if (this.doneCallback_) {
    alert('A callback which was supposed to run after extracting theme color ' +
          ' was not executed.');
  }
  this.doneCallback_ = doneCallback;
  if (logo) {
    if (logo.foregroundImage &&
        !pagespeed.MobUtil.isCrossOrigin(logo.foregroundImage)) {
      this.numPendingImages_ = 1;
      this.getImageDataAndSynthesize_('foreground', logo.foregroundImage,
                                      logo.foregroundRect);
      // TODO(huibao): Report progress in theme extraction in the progress bar.
      console.log('Found logo. Theme color will be computed from logo.');
      return;
    }
  }

  if (logo && logo.foregroundImage) {
    console.log('Found logo but its origin is different that of HTML. ' +
                'Use default color.');
  } else {
    console.log('Could not find logo. Use default color.');
  }
  this.synthesizeCallback_();
};
