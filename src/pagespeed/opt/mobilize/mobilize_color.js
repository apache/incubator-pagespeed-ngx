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
goog.require('goog.dom.TagName');
goog.require('pagespeed.MobUtil');



/**
 * Creates a context for color analysis.
 * @constructor
 */
pagespeed.MobColor = function() {
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
 * Compute theme color from the logo image and background color.
 * @param {!Element} imageElement
 * @param {!goog.color.Rgb} backgroundColor
 * @private
 * @return {pagespeed.MobColor.ThemeColors}
 */
pagespeed.MobColor.prototype.computeThemeColor_ = function(imageElement,
                                                           backgroundColor) {
  var width = imageElement.naturalWidth;
  var height = imageElement.naturalHeight;
  var canvas = document.createElement(goog.dom.TagName.CANVAS);
  canvas.width = width;
  canvas.height = height;
  var context = canvas.getContext('2d');
  context.drawImage(imageElement, 0, 0);
  var foregroundData = context.getImageData(0, 0, width, height);
  var colors = this.computeColors_(
      /** @type {!Uint8ClampedArray} */ (foregroundData.data), backgroundColor,
      width, height);
  return colors;
};


/**
 * Compute theme color or return the default color.
 * @param {Element} imageElement
 * @param {goog.color.Rgb} backgroundColor
 * @return {pagespeed.MobColor.ThemeColors}
 */
pagespeed.MobColor.prototype.run = function(imageElement, backgroundColor) {
  if (imageElement) {
    if (!pagespeed.MobUtil.isCrossOrigin(imageElement.src)) {
      pagespeed.MobUtil.consoleLog('Found logo. Theme color will be computed ' +
                                   'from logo.');
      return this.computeThemeColor_(imageElement,
                                     backgroundColor || [255, 255, 255]);
    } else {
      pagespeed.MobUtil.consoleLog('Found logo but its origin is different ' +
                                   'from that of HTML. Using default color.');
    }
  } else {
    pagespeed.MobUtil.consoleLog('Did not find logo. Using default color.');
  }

  var foregroundColor = [0, 0, 0];
  if (backgroundColor) {
    // Use white foreground if the background is dark; or black foreground
    // otherwise.
    var hsv = goog.color.rgbArrayToHsv(backgroundColor);
    if (hsv[2] <= 0.7 * 255) {
      foregroundColor = [255, 255, 255];
    }
  } else {
    backgroundColor = [255, 255, 255];
  }

  return (new pagespeed.MobColor.ThemeColors(backgroundColor, foregroundColor));
};
