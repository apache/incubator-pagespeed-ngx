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

goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('pagespeed.MobLogo');
goog.require('pagespeed.MobUtil');



/**
 * Creates a context for Pagespeed theme extraction.
 * @param {function(!pagespeed.MobUtil.ThemeData)} doneCallback
 * @constructor
 */
pagespeed.MobTheme = function(doneCallback) {
  /**
   * Callback to invoke when this object finishes its work.
   * @type {function(!pagespeed.MobUtil.ThemeData)}
   */
  this.doneCallback = doneCallback;
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
    // If foregroundImage is a reference to foregroundElement, we clone it
    // for the logo span; otherwise, we move foregroundImage (which has not
    // been attached to DOM) to log span.
    var img = null;
    if (logo.foregroundElement == logo.foregroundImage) {
      img = logo.foregroundImage.cloneNode(false);
    } else {
      img = logo.foregroundImage;
    }
    img.id = 'psmob-logo-image';
    img.style.backgroundColor =
        pagespeed.MobUtil.colorNumbersToString(backgroundColor);
    logoSpan.appendChild(img);
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
  // TODO(huibao): When we pre-configure logo, foregroundElement and
  // foregroundSource aren't set, only the URL is. Redesign this method since
  // we have to figure out which element contains the image and how to handle,
  // if there are multiple elements containing this image.
  if (logo && logo.foregroundElement) {
    var element = logo.foregroundElement;
    element.parentNode.removeChild(element);
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

  // Export theme info if we are asked to.
  if (window.psMobPrecompute) {
    window.psMobBackgroundColor = backgroundColor;
    window.psMobForegroundColor = foregroundColor;
    window.psMobLogoUrl = logo ? logo.foregroundImage.src : null;
  }

  this.doneCallback(themeData);
};


/**
 * Returns true if precomputed theme information is available.
 * @return {boolean}
 */
pagespeed.MobTheme.precomputedThemeAvailable = function() {
  return Boolean(psMobBackgroundColor && psMobForegroundColor &&
                 !window.psMobPrecompute);
};


/**
 * Extract theme of the page. This is the entry method.
 * @param {!pagespeed.Mob} psMob
 * @param {function(!pagespeed.MobUtil.ThemeData)} doneCallback
 */
pagespeed.MobTheme.extractTheme = function(psMob, doneCallback) {
  var mobTheme = new pagespeed.MobTheme(doneCallback);

  // See if there is a precomputed theme + logo (and we are not asked to do
  // the computation ourselves).
  if (psMobBackgroundColor && psMobForegroundColor && !window.psMobPrecompute) {
    var logo = null;
    if (psMobLogoUrl) {
      var img = document.createElement(goog.dom.TagName.IMG);
      img.src = psMobLogoUrl;
      logo = new pagespeed.MobLogo.LogoRecord(1 /* metric */, img);
    }
    mobTheme.colorComplete_(logo, psMobBackgroundColor, psMobForegroundColor);
  } else {
    var mobLogo = new pagespeed.MobLogo(
        psMob, goog.bind(mobTheme.colorComplete_, mobTheme));
    mobLogo.run();
  }
};
