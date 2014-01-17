(function(){window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.DedupInlinedImages = function() {
};
pagespeed.DedupInlinedImages.prototype.inlineImg = function(img_id, script_id) {
  var srcNode = document.getElementById(img_id);
  if (srcNode) {
    var scriptNode = document.getElementById(script_id);
    if (scriptNode) {
      var dstNode = scriptNode.previousSibling;
      dstNode && (dstNode.src = srcNode.getAttribute("src"), scriptNode.parentNode.removeChild(scriptNode));
    }
  }
};
pagespeed.DedupInlinedImages.prototype.inlineImg = pagespeed.DedupInlinedImages.prototype.inlineImg;
pagespeed.dedupInlinedImagesInit = function() {
  pagespeed.dedupInlinedImages = new pagespeed.DedupInlinedImages;
};
pagespeed.dedupInlinedImagesInit = pagespeed.dedupInlinedImagesInit;
})();
