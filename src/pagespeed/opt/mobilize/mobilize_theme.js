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

goog.require('goog.dom.classlist');
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
 * Create an element for the menu button.
 * @param {!goog.color.Rgb} color
 * @return {!Element}
 * @private
 */
pagespeed.MobTheme.createMenuButton_ = function(color) {
  var button = document.createElement('button');
  goog.dom.classlist.add(button, 'psmob-menu-button');
  var hamburgerDiv = document.createElement('div');
  goog.dom.classlist.add(hamburgerDiv, 'psmob-hamburger-div');
  button.appendChild(hamburgerDiv);

  var colorStr = pagespeed.MobUtil.colorNumbersToString(color);
  for (var i = 0; i < 3; ++i) {
    var hamburgerLine = document.createElement('div');
    goog.dom.classlist.add(hamburgerLine, 'psmob-hamburger-line');
    hamburgerLine.style.backgroundColor = colorStr;
    hamburgerDiv.appendChild(hamburgerLine);
  }
  return button;
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
  logoSpan.id = 'psmob-logo-span';

  if (logo && logo.foregroundImage) {
    var newImage = document.createElement('IMG');
    newImage.src = logo.foregroundImage;
    newImage.style.backgroundColor =
        pagespeed.MobUtil.colorNumbersToString(backgroundColor);

    newImage.id = 'psmob-logo-image';
    logoSpan.appendChild(newImage);
  } else {
    logoSpan.textContent = window.location.host;
    logoSpan.style.color =
        pagespeed.MobUtil.colorNumbersToString(foregroundColor);
  }

  var menuButton = pagespeed.MobTheme.createMenuButton_(foregroundColor);
  var themeData = new pagespeed.MobUtil.ThemeData(foregroundColor,
                                                  backgroundColor,
                                                  menuButton,
                                                  logoSpan);
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
  if (window.psLayoutMode) {
    pagespeed.MobTheme.removeLogoImage_(logo);
  }
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
