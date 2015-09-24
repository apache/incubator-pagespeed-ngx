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
 */

goog.provide('mob.ThemePicker');

goog.require('goog.color');
goog.require('goog.dom.TagName');
goog.require('goog.events.EventType');
goog.require('mob.Logo');
goog.require('mob.Nav');
goog.require('mob.util.ElementClass');
goog.require('mob.util.ElementId');
goog.require('mob.util.ThemeData');



/**
 * Create a theme config picker.
 * @constructor
 */
mob.ThemePicker = function() {
  /**
   * Popup dialog for logo choices.  This is actually 2 different things:
   *   1. A table of logo choices and colors that can be clicked to change them.
   *   2. A PRE tag showing the config snippet for the chosen logo/colors.
   * @private {?Element}
   */
  this.logoChoicePopup_ = null;


  /**
   * Navigation context.
   * @private {!mob.Nav}
   */
  this.mobNav_ = new mob.Nav();


  /**
   * @private {?Array.<!mob.LogoCandidate>}
   */
  this.logoCandidates_ = null;
};


/**
 * PNG image of a swap icon (drawn by hand).
 * TODO(huibao): optimize this image.
 * @private @const {string}
 */
mob.ThemePicker.SWAP_ICON_ =
    'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAIYAAABaCAAAAABY7nEZAAAABG' +
    'dBTUEAALGPC/xhBQAAAcpJREFUaN7t2ItxgkAQgOHtADvADkwH2oHpADrADlKCdoAdaAekA0' +
    'qgBEowYIZRE7jbe+xjktsCnE/m54CFm4qBxEgMIkavgwHlVQUDYHVoNTCGWZ86DYxhdudeA2' +
    'OYgExiMgIyicvwziQ6wy8TCoZHJkQM10zIGG6ZUDIcMiFmYDOhZ6AymWd8ogdwY8sEvP+j6x' +
    'gz4WMYM2FlLGfCzBgzqTUwiov81djWvXgb+bFzvFO26MEasqqVPkWXguBlLAbByDAFwcWwBM' +
    'HDsAbBwMAEQc1ABkHKwAdByHAJgorhGAQJI/8IWC5EYmRF2KolCmN/uYX+TDBjU0dYw4UyQo' +
    'JI69n/wmhKeUZ3WAMIM/rT2/34EGWc36fjXI7RlKvHU0WIcQ8CZBlTEKKMRxByjJcghBg/g5' +
    'BgzATBz5gNgpmxFAQnwxAEG8McBBPDFsTzAhY7rgxEED7jxMAFQctAB0HLqPeggTFcj+NGA2' +
    'Oso8o1MMZ7pcg0MFwyabDjeZgjM2F4tGEy4XnQWzNhe+0xZ8L4EmjKhPeVeDET9g+E+UwkPp' +
    'dmMpH5ePyVidin9GsmkouFp0yE1yxTJuJLp+9MNKzg2ipPC8nE+LuMLwqlrYBVqy8VAAAAAE' +
    'lFTkSuQmCC';


/**
 * Create the theme picker.
 */
mob.ThemePicker.prototype.run = function() {
  window.addEventListener(goog.events.EventType.DOMCONTENTLOADED,
                          goog.bind(this.extractThemes_, this));
};


/**
 * @private
 */
mob.ThemePicker.prototype.extractThemes_ = function() {
  var logo = new mob.Logo();
  logo.run(goog.bind(this.logoDone_, this), 5);
};


/**
 * @param {!Array.<!mob.LogoCandidate>} logoCandidates
 * @private
 */
mob.ThemePicker.prototype.logoDone_ = function(logoCandidates) {
  this.logoCandidates_ = logoCandidates;
  if (this.logoCandidates_.length == 0) {
    console.log('No logos detected.');
    return;
  }
  var candidate = this.logoCandidates_[0];
  var theme =
      new mob.util.ThemeData(candidate.logoRecord.foregroundImage.src,
                             candidate.background, candidate.foreground);
  this.mobNav_.run(theme);
  this.chooserShowCandidates_();
};


/**
 * @private
 */
mob.ThemePicker.prototype.chooserShowCandidates_ = function() {
  if (this.logoChoicePopup_) {
    this.chooserDismissLogoChoicePopup_();
    return;
  }

  var table = document.createElement(goog.dom.TagName.TABLE);
  table.className = mob.util.ElementClass.LOGO_CHOOSER_TABLE;

  var thead = document.createElement(goog.dom.TagName.THEAD);
  table.appendChild(thead);
  var trow = document.createElement(goog.dom.TagName.TR);
  thead.appendChild(trow);
  function addData() {
    var td = document.createElement(goog.dom.TagName.TD);
    trow.appendChild(td);
    return td;
  }
  trow.className = mob.util.ElementClass.LOGO_CHOOSER_COLUMN_HEADER;
  addData().textContent = 'Logo';
  addData().textContent = 'Foreground';
  addData().textContent = '';
  addData().textContent = 'Background';

  var tbody = document.createElement(goog.dom.TagName.TBODY);
  table.appendChild(tbody);
  for (var i = 0; i < this.logoCandidates_.length; ++i) {
    trow = document.createElement(goog.dom.TagName.TR);
    trow.className = mob.util.ElementClass.LOGO_CHOOSER_CHOICE;
    tbody.appendChild(trow);
    var candidate = this.logoCandidates_[i];

    var img = document.createElement(goog.dom.TagName.IMG);
    img.src = candidate.logoRecord.foregroundImage.src;
    addData().appendChild(img);

    img.className = mob.util.ElementClass.LOGO_CHOOSER_IMAGE;
    img.onclick = goog.bind(this.chooserSetLogo_, this, candidate);

    var foreground = addData();
    foreground.style.backgroundColor =
        goog.color.rgbArrayToHex(candidate.foreground);
    foreground.className = mob.util.ElementClass.LOGO_CHOOSER_COLOR;

    var swapTd = addData();
    swapTd.className = mob.util.ElementClass.LOGO_CHOOSER_COLOR;
    var swapImg = document.createElement(goog.dom.TagName.IMG);
    swapImg.className = mob.util.ElementClass.LOGO_CHOOSER_SWAP;
    swapImg.src = mob.ThemePicker.SWAP_ICON_;
    swapTd.appendChild(swapImg);

    var background = addData();
    background.style.backgroundColor =
        goog.color.rgbArrayToHex(candidate.background);
    background.className = mob.util.ElementClass.LOGO_CHOOSER_SWAP;

    swapTd.onclick = goog.bind(this.chooserSwapColors_, this, candidate,
        foreground, background);
  }

  this.chooserDisplayPopup_(table);
};


/**
 * @param {!Element} popup
 * @private
 */
mob.ThemePicker.prototype.chooserDisplayPopup_ = function(popup) {
  // The natural width of the table is about 350px, and we'll
  // want it to occupy 2/3 of the screen.  We'll add it to the DOM
  // hidden so we can get the width computed by the browser, and
  // thereby know how to center it.
  popup.style.visibility = 'hidden';
  document.body.appendChild(popup);

  var naturalWidth = popup.offsetWidth;
  var fractionOfScreen = 2.0 / 3.0;
  var scale = window.innerWidth * fractionOfScreen / naturalWidth;
  var offset = Math.round(0.5 * (1 - fractionOfScreen) * window.innerWidth) +
      'px';

  var transform =
      'scale(' + scale + ')' +
      ' translate(' + offset + ',' + offset + ')';
  popup.style['-webkit-transform'] = transform;
  popup.style.transform = transform;

  // Now that we have transformed it, make it show up.
  popup.style.visibility = 'visible';

  if (this.logoChoicePopup_ != null) {
    this.logoChoicePopup_.parentNode.removeChild(this.logoChoicePopup_);
  }
  this.logoChoicePopup_ = popup;
};


/**
 * Sets the logo in response to clicking on an image in the logo chooser
 * popup.
 * @param {!mob.LogoCandidate} candidate
 * @private
 */
mob.ThemePicker.prototype.chooserSetLogo_ = function(candidate) {
  var themeData =
      new mob.util.ThemeData(candidate.logoRecord.foregroundImage.src,
                             candidate.background, candidate.foreground);
  this.mobNav_.updateTheme(themeData);
  // updateTheme currently just deletes and recreates the nav bar, so we have to
  // setup the listener again.
  var logo = document.getElementById(mob.util.ElementId.LOGO_SPAN);
  logo.addEventListener(goog.events.EventType.CLICK,
                        goog.bind(this.chooserShowCandidates_, this));

  var configSnippet = document.createElement(goog.dom.TagName.PRE);
  configSnippet.className = mob.util.ElementClass.LOGO_CHOOSER_CONFIG_FRAGMENT;

  // TODO(jmarantz): Generate nginx syntax as needed.
  configSnippet.textContent =
      'ModPagespeedMobTheme "\n' +
      '    ' + goog.color.rgbArrayToHex(themeData.menuBackColor) + '\n' +
      '    ' + goog.color.rgbArrayToHex(themeData.menuFrontColor) + '\n' +
      '    ' + themeData.logoUrl + '"';
  this.chooserDisplayPopup_(configSnippet);
};


/**
 * Swaps the background and colors for a logo candidate.
 * @param {!mob.LogoCandidate} candidate
 * @param {!Element} foregroundTd table data element (TD) for the foreground
 * @param {!Element} backgroundTd table data element (TD) for the background
 * @private
 */
mob.ThemePicker.prototype.chooserSwapColors_ = function(candidate, foregroundTd,
                                                        backgroundTd) {
  // TODO(jmarantz): we probably only want to swap the fg/bg for the menus,
  // and not for the header bar.  The logo background computation is generally
  // correct, as far as I can tell, and it's only a question of whether the
  // menus would look better in reverse video.
  var tmp = candidate.background;
  candidate.background = candidate.foreground;
  candidate.foreground = tmp;
  tmp = foregroundTd.style['background-color'];
  foregroundTd.style['background-color'] =
      backgroundTd.style['background-color'];
  backgroundTd.style['background-color'] = tmp;
};


/**
 * Dismisses any logo-choice pop.
 * @private
 */
mob.ThemePicker.prototype.chooserDismissLogoChoicePopup_ = function() {
  if (this.logoChoicePopup_) {
    this.logoChoicePopup_.parentNode.removeChild(this.logoChoicePopup_);
    this.logoChoicePopup_ = null;
  }
};
