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

goog.provide('mob.button.Map');

goog.require('mob.button.AbstractButton');
goog.require('mob.util');
goog.require('mob.util.BeaconEvents');
goog.require('mob.util.ElementId');



/**
 * Map button.
 * @param {!goog.color.Rgb} color
 * @param {string} mapLocation
 * @param {?string} conversionId
 * @param {?string} conversionLabel
 * @constructor
 * @extends {mob.button.AbstractButton}
 */
mob.button.Map = function(color, mapLocation, conversionId, conversionLabel) {
  /**
   * Map location to navigate to when the button is clicked. Passed to the "q"
   * query param of the google maps url.
   * @private {string}
   */
  this.mapLocation_ = mapLocation;


  /**
   * Adwords conversion ID.
   * @private {?string}
   */
  this.conversionId_ = conversionId;

  /**
   * Adwords map conversion label.
   * @private {?string}
   */
  this.conversionLabel_ = conversionLabel;

  mob.button.Map.base(this, 'constructor', mob.util.ElementId.MAP_BUTTON,
                      mob.button.Map.ICON_, color, mob.button.Map.LABEL_);
};
goog.inherits(mob.button.Map, mob.button.AbstractButton);


/**
 * GIF icon for the map button.
 * https://www.gstatic.com/images/icons/material/system/2x/place_black_48dp.png
 * Generated with 'curl $url | convert - gif:- | base64'
 * @const {string}
 * @private
 */
mob.button.Map.ICON_ =
    'R0lGODlhYABgAPAAAAAAAAAAACH5BAEAAAEAIf8LSW1hZ2VNYWdpY2sHZ2FtbWE9MQAsAAAA' +
    'AGAAYAAAAv6Mj6nL7Q+jnLTai7PevPsPhuJIluaJpuo6Au4Ls3IA1/Y7l/fO5x8P7Pk0weJt' +
    'eDEqj0jJ8llrQqBUnJRRzQKuCq2We/B6wWIxtzyWos3IdbntTufi7zldLmMj9Cx895tX5SQY' +
    'CEVB2Ed1qJhoWIGYAjnoqMJoYRlJ+aiJgrn41LiE4XlCOim6YhqhSiK5ytoCiyUr4upgW0q7' +
    'pxuSdQpaiGvgV0nMF3rHW5vs68OsHPuM6iw9TV0dpIadrb3N1O0ddRYeQ0ZuZU4OFna+zu7t' +
    '/r4dLy9NX/98j3+nv8vc7y8ZwIBxBhJ0Y/BgnYT7ADFs2OwhRE4SaRyrOMwhRjeF1jZOBOJx' +
    'FrCQf0aSTGDyJEojKm9xa9kAJEyXO2Y+qGnzpricOl3wHPQzqNChRIsaPYo0acUCADs=';


/**
 * The text to insert next to the map button.
 * @private @const {string}
 */
mob.button.Map.LABEL_ = 'GET DIRECTIONS';


/** @override */
mob.button.Map.prototype.clickHandler = function() {
  mob.util.sendBeaconEvent(mob.util.BeaconEvents.MAP_BUTTON,
                           goog.bind(this.openMap_, this));
};


/**
 * Gets a map URL based on the location.
 * @private
 * @return {string}
 */
mob.button.Map.prototype.getMapUrl_ = function() {
  // TODO(jmarantz): test/fix this in iOS safari/chrome with/without
  // Google Maps installed.  Probably use 'http://maps.apple.com/?='
  //
  // I don't know the best way to do this but I asked the question on
  // Stack Overflow: http://goo.gl/0g8kEV
  //
  // Other links to explore:
  // https://developer.apple.com/library/iad/featuredarticles/iPhoneURLScheme_Reference/MapLinks/MapLinks.html
  // https://developers.google.com/maps/documentation/ios/urlscheme
  // http://stackoverflow.com/questions/17915901/is-there-an-android-equivalent-to-google-maps-url-scheme-for-ios
  var mapUrl =
      'https://maps.google.com/maps?q=' + encodeURIComponent(this.mapLocation_);
  return mapUrl;
};


/**
 * Loads a tracking pixel that triggers a conversion event if the conversion
 * label is set, and then navigates to a map.  Note that we navigate to the map
 * whether the conversion succeeds or fails.
 * @private
 */
mob.button.Map.prototype.openMap_ = function() {
  // We use goog.global here so that we can override it in tests to prevent the
  // page from actually navigating.
  if (!this.conversionId_ || !this.conversionLabel_) {
    // No conversion id or label specified; go straight to the map.
    goog.global.location = this.getMapUrl_();
    return;
  }

  // We will visit the map only after we get the onload/onerror for the
  // tracking pixel.
  var trackingPixel = new Image();
  trackingPixel.onload =
      goog.bind(function() { goog.global.location = this.getMapUrl_(); }, this);

  // The user comes first so he gets to the map even if we can't track it.
  trackingPixel.onerror = trackingPixel.onload;

  // With the handlers set up, we can load the magic pixel to track the
  // conversion.
  //
  // TODO(jmarantz): Is there a better API to report a conversion?  Should
  // we really use script=0, since this is obviously a script?  In any case
  // we should use <a ping> when available.
  trackingPixel.src = '//www.googleadservices.com/pagead/conversion/' +
                      this.conversionId_ + '/?label=' + this.conversionLabel_ +
                      '&amp;guid=ON&amp;script=0';
};
