(function(){window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.DelayImagesInline = function() {
  this.inlineMap_ = {};
};
pagespeed.DelayImagesInline.prototype.addLowResImages = function(a, b) {
  this.inlineMap_[a] = b;
};
pagespeed.DelayImagesInline.prototype.addLowResImages = pagespeed.DelayImagesInline.prototype.addLowResImages;
pagespeed.DelayImagesInline.prototype.replaceElementSrc = function(a) {
  for (var b = 0;b < a.length;++b) {
    var c = a[b].getAttribute("data-pagespeed-high-res-src"), d = a[b].getAttribute("src");
    c && !d && (c = this.inlineMap_[c]) && a[b].setAttribute("src", c);
  }
};
pagespeed.DelayImagesInline.prototype.replaceElementSrc = pagespeed.DelayImagesInline.prototype.replaceElementSrc;
pagespeed.DelayImagesInline.prototype.replaceWithLowRes = function() {
  this.replaceElementSrc(document.getElementsByTagName("img"));
  this.replaceElementSrc(document.getElementsByTagName("input"));
};
pagespeed.DelayImagesInline.prototype.replaceWithLowRes = pagespeed.DelayImagesInline.prototype.replaceWithLowRes;
pagespeed.delayImagesInlineInit = function() {
  var a = new pagespeed.DelayImagesInline;
  pagespeed.delayImagesInline = a;
};
pagespeed.delayImagesInlineInit = pagespeed.delayImagesInlineInit;
})();
