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

goog.provide('mob.button.Menu');

goog.require('mob.button.AbstractButton');
goog.require('mob.util.ElementId');



/**
 * Menu button.
 * @param {!goog.color.Rgb} color
 * @param {!Function} clickHandlerFn
 * @constructor
 * @extends {mob.button.AbstractButton}
 */
mob.button.Menu = function(color, clickHandlerFn) {
  this.clickHandlerFn_ = clickHandlerFn;

  mob.button.Menu.base(this, 'constructor', mob.util.ElementId.MENU_BUTTON,
                       mob.button.Menu.ICON_, color, null);
};
goog.inherits(mob.button.Menu, mob.button.AbstractButton);


/**
 * GIF icon for the menu button.
 * https://www.gstatic.com/images/icons/material/system/2x/menu_black_48dp.png
 * Generated with 'curl $url | convert - gif:- | base64'
 * @const {string}
 * @private
 */
mob.button.Menu.ICON_ =
    'R0lGODlhYABgAPAAAAAAAAAAACH5BAEAAAEALAAAAABgAGAAAAK6jI+py+0Po5y02ouz3rz7' +
    'D4biSJbmiabqyrbuC8fyTNf2jef6zvf+DwwKh8Si8agCKJfMpvMJjUqnTg71is1qldat97vt' +
    'gsfkp7iMJp/T7PCmDXdr4vQr8o7P6/f8vv8PGCg4SHhTdxi1hriouHjY6EgHGQk3SclmeYmW' +
    'qalW+AkaKjpKWmp6ipqqatH5+NYq+QpbKTuLWWu7iZvrOcebxvlrt0pcbHyMnKy8zNzs/Awd' +
    'LT1NXW19TVoAADs=';


/** @override */
mob.button.Menu.prototype.clickHandler = function() {
  this.clickHandlerFn_();
};
