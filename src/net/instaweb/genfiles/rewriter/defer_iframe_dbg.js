(function(){var pagespeedutils = {MAX_POST_SIZE:131072, sendBeacon:function(a, b, c) {
  var d;
  if (window.XMLHttpRequest) {
    d = new XMLHttpRequest;
  } else {
    if (window.ActiveXObject) {
      try {
        d = new ActiveXObject("Msxml2.XMLHTTP");
      } catch (e) {
        try {
          d = new ActiveXObject("Microsoft.XMLHTTP");
        } catch (g) {
        }
      }
    }
  }
  if (!d) {
    return!1;
  }
  var f = -1 == a.indexOf("?") ? "?" : "&";
  a = a + f + "url=" + encodeURIComponent(b);
  d.open("POST", a);
  d.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
  d.send(c);
  return!0;
}, addHandler:function(a, b, c) {
  if (a.addEventListener) {
    a.addEventListener(b, c, !1);
  } else {
    if (a.attachEvent) {
      a.attachEvent("on" + b, c);
    } else {
      var d = a["on" + b];
      a["on" + b] = function() {
        c.call(this);
        d && d.call(this);
      };
    }
  }
}, getPosition:function(a) {
  for (var b = a.offsetTop, c = a.offsetLeft;a.offsetParent;) {
    a = a.offsetParent, b += a.offsetTop, c += a.offsetLeft;
  }
  return{top:b, left:c};
}, getWindowSize:function() {
  return{height:window.innerHeight || document.documentElement.clientHeight || document.body.clientHeight, width:window.innerWidth || document.documentElement.clientWidth || document.body.clientWidth};
}, inViewport:function(a, b) {
  var c = pagespeedutils.getPosition(a);
  return pagespeedutils.positionInViewport(c, b);
}, positionInViewport:function(a, b) {
  return a.top < b.height && a.left < b.width;
}, getRequestAnimationFrame:function() {
  return window.requestAnimationFrame || window.webkitRequestAnimationFrame || window.mozRequestAnimationFrame || window.oRequestAnimationFrame || window.msRequestAnimationFrame || null;
}};
window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.DeferIframe = function() {
};
pagespeed.DeferIframe.prototype.convertToIframe = function() {
  var a = document.getElementsByTagName("pagespeed_iframe");
  if (0 < a.length) {
    for (var a = a[0], b = document.createElement("iframe"), c = 0, d = a.attributes, e = d.length;c < e;++c) {
      b.setAttribute(d[c].name, d[c].value);
    }
    a.parentNode.replaceChild(b, a);
  }
};
pagespeed.DeferIframe.prototype.convertToIframe = pagespeed.DeferIframe.prototype.convertToIframe;
pagespeed.deferIframeInit = function() {
  var a = new pagespeed.DeferIframe;
  pagespeed.deferIframe = a;
};
pagespeed.deferIframeInit = pagespeed.deferIframeInit;
})();
