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
   * @private {?function(!pagespeed.MobUtil.ThemeData)}
   */
  this.doneCallback_ = doneCallback;
};


/**
 * Create an element for the menu button.
 * @param {!goog.color.Rgb} color
 * @return {!Element}
 * @private
 */
pagespeed.MobTheme.createMenuButton_ = function(color) {
  var button = document.createElement(goog.dom.TagName.BUTTON);
  goog.dom.classlist.add(button, 'psmob-menu-button');
  var hamburgerDiv = document.createElement(goog.dom.TagName.DIV);
  goog.dom.classlist.add(hamburgerDiv, 'psmob-hamburger-div');
  button.appendChild(hamburgerDiv);

  var colorStr = pagespeed.MobUtil.colorNumbersToString(color);
  for (var i = 0; i < 3; ++i) {
    var hamburgerLine = document.createElement(goog.dom.TagName.DIV);
    goog.dom.classlist.add(hamburgerLine, 'psmob-hamburger-line');
    hamburgerLine.style.backgroundColor = colorStr;
    hamburgerDiv.appendChild(hamburgerLine);
  }
  return button;
};


/**
 * Add a logo span to window.document.
 * @param {pagespeed.MobLogo.LogoRecord} logo (can be null)
 * @param {!goog.color.Rgb} backgroundColor
 * @param {!goog.color.Rgb} foregroundColor
 * @return {!pagespeed.MobUtil.ThemeData}
 */
pagespeed.MobTheme.synthesizeLogoSpan = function(
    logo, backgroundColor, foregroundColor) {
  // When in configuration-mode, touch the logo to bring up a logo picker.
  // Actually the logo picker comes up in the first place, but you can
  // then alter the logo iteratively by touching it.
  //
  // TODO(jmarantz): also consider having this element be an anchor
  // in non-config mode, pointing either to the landing page or to
  // the non-mobilized version.
  var anchorOrSpan = document.createElement(
      psConfigMode ? goog.dom.TagName.A : goog.dom.TagName.SPAN);
  var logoElement;

  if (logo && logo.foregroundImage) {
    // If foregroundImage is a reference to foregroundElement, we clone it
    // for the logo span; otherwise, we move foregroundImage (which has not
    // been attached to DOM) to log span.
    if (window.psConfigMode ||
        (logo.foregroundElement == logo.foregroundImage)) {
      // Note that we always clone nodes in psConfigMode because we need
      // to reference the logo in the popup as well as the menu bar.
      logoElement = logo.foregroundImage.cloneNode(false);
    } else {
      logoElement = logo.foregroundImage;
    }
    logoElement.removeAttribute('id');
  } else {
    logoElement = document.createElement('span');
    logoElement.textContent = window.location.host;
    logoElement.style.color =
        pagespeed.MobUtil.colorNumbersToString(foregroundColor);
  }
  anchorOrSpan.appendChild(logoElement);

  var menuButton = pagespeed.MobTheme.createMenuButton_(foregroundColor);
  var themeData = new pagespeed.MobUtil.ThemeData(
      foregroundColor, backgroundColor, menuButton, anchorOrSpan, logoElement);
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
 * @param {pagespeed.MobLogo.LogoRecord} logo (can be null)
 * @param {!goog.color.Rgb} backgroundColor
 * @param {!goog.color.Rgb} foregroundColor
 * @return {!pagespeed.MobUtil.ThemeData}
 */
pagespeed.MobTheme.createThemeData = function(
    logo, backgroundColor, foregroundColor) {
  var themeData = pagespeed.MobTheme.synthesizeLogoSpan(
      logo, backgroundColor, foregroundColor);
  if (window.psLayoutMode) {
    pagespeed.MobTheme.removeLogoImage_(logo);
  }

  // Export theme info if we are asked to.
  if (window.psMobPrecompute) {
    window.psMobBackgroundColor = backgroundColor;
    window.psMobForegroundColor = foregroundColor;
    window.psMobLogoUrl = logo ? logo.foregroundImage.src : null;
  }
  return themeData;
};


/**
 * @param {!Array.<pagespeed.MobLogoCandidate>} candidates
 */
pagespeed.MobTheme.prototype.logoComplete = function(candidates) {
  if (this.doneCallback_) {
    var themeData;
    if (candidates.length >= 1) {
      var candidate = candidates[0];
      themeData = pagespeed.MobTheme.createThemeData(
          candidate.logoRecord, candidate.background, candidate.foreground);
    } else {
      themeData = pagespeed.MobTheme.createThemeData(
          null, [255, 255, 255], [0, 0, 0]);
    }
    pagespeed.MobTheme.installLogo(themeData);
    var doneCallback = this.doneCallback_;
    this.doneCallback_ = null;
    doneCallback(themeData);
  }
};


/**
 * @param {pagespeed.MobUtil.ThemeData} themeData
 */
pagespeed.MobTheme.installLogo = function(themeData) {
  if (window.psConfigMode) {
    themeData.anchorOrSpan.href = 'javascript:psPickLogo()';
  }
  themeData.anchorOrSpan.id = 'psmob-logo-span';
  themeData.logoElement.id = 'psmob-logo-image';
  themeData.logoElement.style.backgroundColor =
      pagespeed.MobUtil.colorNumbersToString(themeData.menuBackColor);
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
 * @param {!pagespeed.MobLogo} mobLogo
 * @param {function(!pagespeed.MobUtil.ThemeData)} doneCallback
 */
pagespeed.MobTheme.extractTheme = function(mobLogo, doneCallback) {
  // See if there is a precomputed theme + logo (and we are not asked to do
  // the computation ourselves).
  if (window.psConfigMode) {
    var psMob = mobLogo.psMob();
    var mobTheme = new pagespeed.MobTheme(doneCallback);
    mobLogo.run(goog.bind(psMob.setLogoCandidatesAndShow, psMob, mobTheme),
                5 /* numCandidates */);
  } else if (psMobBackgroundColor && psMobForegroundColor &&
             !window.psMobPrecompute) {
    var logo = null;
    if (psMobLogoUrl) {
      var img = document.createElement(goog.dom.TagName.IMG);
      img.src = psMobLogoUrl;
      logo = new pagespeed.MobLogo.LogoRecord(1 /* metric */, img);
    }
    var themeData = pagespeed.MobTheme.createThemeData(
        logo, psMobBackgroundColor, psMobForegroundColor);
    pagespeed.MobTheme.installLogo(themeData);
    doneCallback(themeData);
  } else {
    var mobTheme = new pagespeed.MobTheme(doneCallback);
    mobLogo.run(goog.bind(mobTheme.logoComplete, mobTheme),
                1 /* numCandidates */);
  }
};
