goog.provide('mob.button.Menu');

goog.require('mob.button.AbstractButton');
goog.require('mob.util.ElementId');



/**
 * Menu button.
 * @param {!goog.color.Rgb} color
 * @param {!function()} clickHandlerFn
 * @constructor
 * @extends {mob.button.AbstractButton}
 */
mob.button.Menu = function(color, clickHandlerFn) {
  this.clickHandlerFn_ = clickHandlerFn;

  mob.button.Menu.base(
      this, 'constructor', mob.util.ElementId.MENU_BUTTON,
      mob.button.Menu.ICON_, color, null);
};
goog.inherits(mob.button.Menu, mob.button.AbstractButton);


/**
 * GIF icon for the menu button.
 * https://www.gstatic.com/images/icons/material/system/2x/more_vert_black_48dp.png
 * Generated with 'curl $url | convert - gif:- | base64'
 * @const {string}
 * @private
 */
mob.button.Menu.ICON_ =
    'R0lGODlhYABgAPAAAAAAAAAAACH5BAEAAAEAIf8LSW1hZ2VNYWdpY2sHZ2FtbWE9MQAsAAAA' +
    'AGAAYAAAAsKMj6nL7Q+jnLTai7PevPsPhuJIluaJpurKtu4Lx/JM1/aN5/reAv7PI/2GwOCH' +
    'iAQYO8nkctNEPjNR59RSlV4rWeKW2y1+JWHxGFL2nSflNTvsflfjlDkdPLzr9/y+/w8YKDio' +
    'oUWIYHfYpQg32CaYphQYCZlW+QhIOWl5mUWI2dl0aJA4GmBomqq6ytrq+gqrgzpa6uj52Rga' +
    'ZQvqp5nJCdzb9/tXbJy7uYhbq5unOhsrPU1dbX2Nna29zd3t/T1VAAA7';


/** @override */
mob.button.Menu.prototype.clickHandler = function() {
  this.clickHandlerFn_();
};
