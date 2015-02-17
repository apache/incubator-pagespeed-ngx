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

/**
 * @fileoverview Externs used by mobilization JS.  These variables are all
 * populated from the server in
 * net/instaweb/rewriter/mobilize_rewrite_filter.cc by injecting the variable
 * declarations in the DOM.
 */


/** @type {string} */
var pagespeedContentIds;


/** @type {Array<string>} */
var pagespeedHeaderIds;


/** @type {Array<string>} */
var pagespeedNavigationalIds;


/** @type {string} */
var psConversionId;


/** @type {boolean} */
var psDebugMode;


/** @type {boolean} */
var psLayoutMode;


/** @type {?string} */
var psMapConversionLabel;


/** @type {string} */
var psMapLocation;


/** @type {Object} */
var psMobStaticImageInfo;


/** @type {boolean} */
var psNavMode;


/** @type {string} */
var psPhoneConversionLabel;


/** @type {?string} */
var psPhoneNumber;


/** @type {boolean} */
var psStaticJs;


/** @type {goog.color.Rgb} */
var psMobBackgroundColor;


/** @type {goog.color.Rgb} */
var psMobForegroundColor;


/** @type {string} */
var psMobLogoUrl;
