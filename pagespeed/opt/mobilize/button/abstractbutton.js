/*
 * Copyright 2015 Google Inc.
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
 * Author: jmarantz@google.com (Joshua Marantz)
 */

goog.provide('mob.button.AbstractButton');

goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('mob.util');
goog.require('mob.util.ElementClass');



/**
 * Base class for buttons.
 * @param {!mob.util.ElementId} id
 * @param {string} iconImage base64 encoded image.
 * @param {!goog.color.Rgb} color for the image icon.
 * @param {?string} labelText Optional text for label.
 * @constructor
 */
mob.button.AbstractButton = function(id, iconImage, color, labelText) {
  /**
   * Top level element.
   * @type {!Element}
   */
  this.el = document.createElement(goog.dom.TagName.A);

  /**
   * @private {!mob.util.ElementId}
   */
  this.id_ = id;

  /**
   * Base64 encoded gif image.
   * @private {string}
   */
  this.iconImage_ = iconImage;

  /**
   * Color to use for the image.
   * @private {!goog.color.Rgb}
   */
  this.color_ = color;

  /**
   * Text to add next to the button. Can be null if no text should be displayed.
   * @private {?string}
   */
  this.labelText_ = labelText;

  this.createButton();
};


/**
 * Initialize the button. If overridden by a subclass, this should still be
 * called.
 * @protected
 */
mob.button.AbstractButton.prototype.createButton = function() {
  this.el.id = this.id_;

  goog.dom.classlist.add(this.el, mob.util.ElementClass.BUTTON);
  this.el.onclick = goog.bind(this.clickHandler, this);

  var icon = document.createElement(goog.dom.TagName.DIV);
  goog.dom.classlist.add(icon, mob.util.ElementClass.BUTTON_ICON);
  // We set the image using backgroundImage because if it's a foreground image
  // then dialing fails on a Samsung Galaxy Note 2.
  icon.style.backgroundImage =
      'url(' + mob.util.synthesizeImage(this.iconImage_, this.color_) + ')';
  this.el.appendChild(icon);

  if (this.labelText_) {
    var label = document.createElement(goog.dom.TagName.P);
    goog.dom.classlist.add(label, mob.util.ElementClass.BUTTON_TEXT);
    this.el.appendChild(label);
    label.appendChild(document.createTextNode(this.labelText_));
  }
};


/**
 * Function to be called when button is clicked.
 * @protected
 */
mob.button.AbstractButton.prototype.clickHandler = goog.abstractMethod;
