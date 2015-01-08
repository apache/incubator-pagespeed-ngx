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

goog.provide('pagespeed.MobTheme');

goog.require('pagespeed.MobColor');
goog.require('pagespeed.MobLogo');
goog.require('pagespeed.MobUtil');



/**
 * Creates a context for Pagespeed theme extraction.
 * @constructor
 */
pagespeed.MobTheme = function() {
  /**
   * Callback to invoke when this object finishes its work.
   * @type {function(!pagespeed.MobUtil.ThemeData)}
   */
  this.doneCallback;

  /**
   * The best logo, if there is any.
   * @type {?pagespeed.MobLogo.LogoRecord}
   */
  this.logo = null;
};


/**
 * Return an SVG image for the hamburger icon.
 * @param {number} width
 * @param {number} height
 * @param {string} foregroundColorStr
 * @return {string}
 * @private
 */
pagespeed.MobTheme.menuIconString_ = function(width, height,
                                              foregroundColorStr) {
  // TODO(huibao): Use div and CSS to contruct the hamburger icon for more
  // browser support.
  var y1 = String(Math.floor(0.25 * height));
  var y2 = String(Math.floor(0.5 * height));
  var y3 = String(Math.floor(0.75 * height));
  var strokeWidth = String(Math.floor(0.15 * height));

  var icon = '<svg height="' + height + 'px" ' + 'width="' + width + 'px" ' +
      'style="stroke:' + foregroundColorStr +
      ';stroke-width:' + strokeWidth + 'px" >' +
      '<line x1="4px" y1="' + y1 + 'px" ' +
      'x2="' + (width - 4) + 'px" ' + 'y2="' + y1 + 'px"/>' +
      '<line x1="4px" y1="' + y2 + 'px" ' +
      'x2="' + (width - 4) + 'px" ' + 'y2="' + y2 + 'px"/>' +
      '<line x1="4px" y1="' + y3 + 'px" ' +
      'x2="' + (width - 4) + 'px" ' + 'y2="' + y3 + 'px"/>' +
      '</svg>';
  return icon;
};


/**
 * Return the string for a button.
 * @param {string} foregroundColorStr
 * @return {string}
 * @private
 */
pagespeed.MobTheme.synthesizeHamburgerIcon_ = function(foregroundColorStr) {
  return '<button class="psmob-menu-button">' +
         pagespeed.MobTheme.menuIconString_(28, 28, foregroundColorStr) +
         '</button>';
};


/**
 * Add a logo span to window.document.
 * @param {pagespeed.MobLogo.LogoRecord} logo
 * @param {!goog.color.Rgb} backgroundColor
 * @param {!goog.color.Rgb} foregroundColor
 * @return {!pagespeed.MobUtil.ThemeData}
 * @private
 */
pagespeed.MobTheme.synthesizeLogoSpan_ = function(logo, backgroundColor,
                                                  foregroundColor) {
  var logoSpan = document.createElement('span');
  logoSpan.classList.add('psmob-logo-span');

  if (logo && logo.foregroundImage) {
    var newImage = document.createElement('IMG');
    newImage.src = logo.foregroundImage;
    newImage.style.backgroundColor =
        pagespeed.MobUtil.colorNumbersToString(backgroundColor);
    newImage.setAttribute('data-mobile-role', 'logo');
    logoSpan.appendChild(newImage);
  } else {
    logoSpan.innerHTML = window.location.host;
  }

  var headerBarHtml =
      pagespeed.MobTheme.synthesizeHamburgerIcon_(
          pagespeed.MobUtil.colorNumbersToString(foregroundColor));

  // TODO(huibao): Instead of appending the new logo span to document, sending
  // them to MobNav as part of themeData.
  document.body.appendChild(logoSpan);

  var themeData = new pagespeed.MobUtil.ThemeData(foregroundColor,
                                                  backgroundColor,
                                                  headerBarHtml);
  return themeData;
};


/**
 * Remove foreground image for the logo from document.
 * @param {pagespeed.MobLogo.LogoRecord} logo
 * @private
 */
pagespeed.MobTheme.removeLogoImage_ = function(logo) {
  if (logo && logo.foregroundElement && logo.foregroundSource) {
    var element = logo.foregroundElement;
    switch (logo.foregroundSource) {
      case pagespeed.MobUtil.ImageSource.IMG:
      case pagespeed.MobUtil.ImageSource.SVG:
        element.parentNode.removeChild(element);
        break;
      case pagespeed.MobUtil.ImageSource.BACKGROUND:
        element.style.backgroundImage = 'none';
        break;
    }
  }
};


/**
 * Compute color and synthesize logo span.
 * @param {pagespeed.MobLogo.LogoRecord} logo
 * @param {!goog.color.Rgb} backgroundColor
 * @param {!goog.color.Rgb} foregroundColor
 * @this {pagespeed.MobTheme}
 * @private
 */
pagespeed.MobTheme.prototype.colorComplete_ = function(
    logo, backgroundColor, foregroundColor) {
  var themeData = pagespeed.MobTheme.synthesizeLogoSpan_(logo,
                                                         backgroundColor,
                                                         foregroundColor);
  pagespeed.MobTheme.removeLogoImage_(logo);
  this.doneCallback(themeData);
};


/**
 * Extract theme of the page. This is the entry method.
 * @param {!pagespeed.Mob} psMob
 * @param {function(!pagespeed.MobUtil.ThemeData)} doneCallback
 */
pagespeed.MobTheme.extractTheme = function(psMob, doneCallback) {
  // TODO(huibao): If the logo image is in 'imageMap', use it directly instead
  // of creating a new IMG tag.
  if (!doneCallback) {
    alert('Not expecting to start onloads after the callback is called');
  }

  var mobLogo = new pagespeed.MobTheme();
  mobLogo.doneCallback = doneCallback;
  mobLogo.logo = (new pagespeed.MobLogo(psMob)).run();
  (new pagespeed.MobColor()).run(mobLogo.logo,
                                 goog.bind(mobLogo.colorComplete_, mobLogo));
};
