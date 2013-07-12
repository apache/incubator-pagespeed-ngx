(function(){window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.DetectReflow = function() {
  this.preScriptHeights_ = {};
  this.reflowElementHeights_ = "";
  this.jsDeferDone_ = !1
};
pagespeed.DetectReflow.prototype.getPreScriptHeights = function() {
  return this.preScriptHeights_
};
pagespeed.DetectReflow.prototype.getPreScriptHeights = pagespeed.DetectReflow.prototype.getPreScriptHeights;
pagespeed.DetectReflow.prototype.getReflowElementHeight = function() {
  return this.reflowElementHeights_
};
pagespeed.DetectReflow.prototype.getReflowElementHeight = pagespeed.DetectReflow.prototype.getReflowElementHeight;
pagespeed.DetectReflow.prototype.isJsDeferDone = function() {
  return this.jsDeferDone_
};
pagespeed.DetectReflow.prototype.isJsDeferDone = pagespeed.DetectReflow.prototype.isJsDeferDone;
pagespeed.DetectReflow.prototype.findChangedDivsAfterDeferredJs = function() {
  !this.preScriptHeights_ && console && console.log("preScriptHeights_ is not available");
  for(var newDivs = document.getElementsByTagName("div"), newLen = newDivs.length, i = 0;i < newLen;++i) {
    var div = newDivs[i];
    if(div.hasAttribute("id")) {
      var divId = div.getAttribute("id");
      void 0 != this.preScriptHeights_[divId] && this.preScriptHeights_[divId] != div.clientHeight && (this.reflowElementHeights_ = this.reflowElementHeights_ + divId + ":" + window.getComputedStyle(div, null).getPropertyValue("height") + ",")
    }
  }
};
pagespeed.DetectReflow.prototype.findChangedDivsAfterDeferredJs = pagespeed.DetectReflow.prototype.findChangedDivsAfterDeferredJs;
pagespeed.DetectReflow.prototype.labelDivsBeforeDeferredJs = function() {
  for(var divs = document.getElementsByTagName("div"), len = divs.length, i = 0;i < len;++i) {
    var div = divs[i];
    div.hasAttribute("id") && void 0 != div.clientHeight && (this.preScriptHeights_[div.getAttribute("id")] = div.clientHeight)
  }
};
pagespeed.DetectReflow.prototype.labelDivsBeforeDeferredJs = pagespeed.DetectReflow.prototype.labelDivsBeforeDeferredJs;
pagespeed.DetectReflow.prototype.setJsDeferDone = function() {
  this.jsDeferDone_ = !0
};
pagespeed.DetectReflow.prototype.setJsDeferDone = pagespeed.DetectReflow.prototype.setJsDeferDone;
if("undefined" != pagespeed.deferJs && "undefined" != pagespeed.deferJs.addBeforeDeferRunFunctions && "undefined" != pagespeed.deferJs.addAfterDeferRunFunctions) {
  var x = new pagespeed.DetectReflow;
  pagespeed.detectReflow = x;
  var pre = function() {
    pagespeed.detectReflow.labelDivsBeforeDeferredJs()
  };
  pagespeed.deferJs.addBeforeDeferRunFunctions(pre);
  var post = function() {
    pagespeed.detectReflow.setJsDeferDone()
  };
  pagespeed.deferJs.addAfterDeferRunFunctions(post)
}
;})();
