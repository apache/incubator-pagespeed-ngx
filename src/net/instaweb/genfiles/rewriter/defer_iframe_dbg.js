(function(){window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.DeferIframe = function() {
};
pagespeed.DeferIframe.prototype.convertToIframe = function() {
  var a = document.getElementsByTagName("pagespeed_iframe");
  if (0 < a.length) {
    for (var a = a[0], d = document.createElement("iframe"), b = 0, c = a.attributes, e = c.length;b < e;++b) {
      d.setAttribute(c[b].name, c[b].value);
    }
    a.parentNode.replaceChild(d, a);
  }
};
pagespeed.DeferIframe.prototype.convertToIframe = pagespeed.DeferIframe.prototype.convertToIframe;
pagespeed.deferIframeInit = function() {
  var a = new pagespeed.DeferIframe;
  pagespeed.deferIframe = a;
};
pagespeed.deferIframeInit = pagespeed.deferIframeInit;
})();
