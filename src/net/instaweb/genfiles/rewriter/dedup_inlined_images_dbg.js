(function(){window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.DedupInlinedImages = function() {
};
pagespeed.DedupInlinedImages.prototype.inlineImg = function(a, c, b) {
  if (a = document.getElementById(a)) {
    if (c = document.getElementById(c)) {
      if (b = document.getElementById(b)) {
        c.src = a.getAttribute("src"), b.parentNode.removeChild(b);
      }
    }
  }
};
pagespeed.DedupInlinedImages.prototype.inlineImg = pagespeed.DedupInlinedImages.prototype.inlineImg;
pagespeed.dedupInlinedImagesInit = function() {
  var a = new pagespeed.DedupInlinedImages;
  pagespeed.dedupInlinedImages = a;
};
pagespeed.dedupInlinedImagesInit = pagespeed.dedupInlinedImagesInit;
})();
