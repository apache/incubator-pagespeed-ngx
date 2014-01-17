(function(){window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.DeferIframe = function() {
};
pagespeed.DeferIframe.prototype.convertToIframe = function() {
  var nodes = document.getElementsByTagName("pagespeed_iframe");
  if (0 < nodes.length) {
    for (var oldElement = nodes[0], newElement = document.createElement("iframe"), i = 0, a = oldElement.attributes, n = a.length;i < n;++i) {
      newElement.setAttribute(a[i].name, a[i].value);
    }
    oldElement.parentNode.replaceChild(newElement, oldElement);
  }
};
pagespeed.DeferIframe.prototype.convertToIframe = pagespeed.DeferIframe.prototype.convertToIframe;
pagespeed.deferIframeInit = function() {
  pagespeed.deferIframe = new pagespeed.DeferIframe;
};
pagespeed.deferIframeInit = pagespeed.deferIframeInit;
})();
