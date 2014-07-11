(function(){window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.DedupInlinedImages = function() {
};
pagespeed.DedupInlinedImages.prototype.inlineImg = function(from_img_id, to_img_id, script_id) {
  var srcNode = document.getElementById(from_img_id);
  if (srcNode) {
    var dstNode = document.getElementById(to_img_id);
    if (dstNode) {
      var scriptNode = document.getElementById(script_id);
      scriptNode && (dstNode.src = srcNode.getAttribute("src"), scriptNode.parentNode.removeChild(scriptNode));
    }
  }
};
pagespeed.DedupInlinedImages.prototype.inlineImg = pagespeed.DedupInlinedImages.prototype.inlineImg;
pagespeed.dedupInlinedImagesInit = function() {
  pagespeed.dedupInlinedImages = new pagespeed.DedupInlinedImages;
};
pagespeed.dedupInlinedImagesInit = pagespeed.dedupInlinedImagesInit;
})();
