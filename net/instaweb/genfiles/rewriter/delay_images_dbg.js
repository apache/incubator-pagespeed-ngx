(function(){var pagespeedutils = {MAX_POST_SIZE:131072, sendBeacon:function(a, b, c) {
  var d;
  if (window.XMLHttpRequest) {
    d = new XMLHttpRequest;
  } else {
    if (window.ActiveXObject) {
      try {
        d = new ActiveXObject("Msxml2.XMLHTTP");
      } catch (f) {
        try {
          d = new ActiveXObject("Microsoft.XMLHTTP");
        } catch (g) {
        }
      }
    }
  }
  if (!d) {
    return !1;
  }
  var e = -1 == a.indexOf("?") ? "?" : "&";
  a = a + e + "url=" + encodeURIComponent(b);
  d.open("POST", a);
  d.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
  d.send(c);
  return !0;
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
  return {top:b, left:c};
}, getWindowSize:function() {
  return {height:window.innerHeight || document.documentElement.clientHeight || document.body.clientHeight, width:window.innerWidth || document.documentElement.clientWidth || document.body.clientWidth};
}, inViewport:function(a, b) {
  var c = pagespeedutils.getPosition(a);
  return pagespeedutils.positionInViewport(c, b);
}, positionInViewport:function(a, b) {
  return a.top < b.height && a.left < b.width;
}, getRequestAnimationFrame:function() {
  return window.requestAnimationFrame || window.webkitRequestAnimationFrame || window.mozRequestAnimationFrame || window.oRequestAnimationFrame || window.msRequestAnimationFrame || null;
}};
pagespeedutils.now = Date.now || function() {
  return +new Date;
};
window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.DelayImages = function() {
  this.highResReplaced_ = this.lazyLoadHighResHandlersRegistered_ = !1;
};
pagespeed.DelayImages.prototype.replaceElementSrc = function(a) {
  for (var b = 0;b < a.length;++b) {
    var c = a[b].getAttribute("data-pagespeed-high-res-src");
    c && a[b].setAttribute("src", c);
  }
};
pagespeed.DelayImages.prototype.replaceElementSrc = pagespeed.DelayImages.prototype.replaceElementSrc;
pagespeed.DelayImages.prototype.registerLazyLoadHighRes = function() {
  if (this.lazyLoadHighResHandlersRegistered_) {
    this.highResReplaced_ = !1;
  } else {
    var a = document.body, b, c = 0, d = this;
    this.highResReplaced = !1;
    "ontouchstart" in a ? (pagespeedutils.addHandler(a, "touchstart", function(a) {
      b = pagespeedutils.now();
    }), pagespeedutils.addHandler(a, "touchend", function(a) {
      c = pagespeedutils.now();
      (null != a.changedTouches && 2 == a.changedTouches.length || null != a.touches && 2 == a.touches.length || 500 > c - b) && d.loadHighRes();
    })) : pagespeedutils.addHandler(window, "click", function(a) {
      d.loadHighRes();
    });
    pagespeedutils.addHandler(window, "load", function(a) {
      d.loadHighRes();
    });
    this.lazyLoadHighResHandlersRegistered_ = !0;
  }
};
pagespeed.DelayImages.prototype.registerLazyLoadHighRes = pagespeed.DelayImages.prototype.registerLazyLoadHighRes;
pagespeed.DelayImages.prototype.loadHighRes = function() {
  this.highResReplaced_ || (this.replaceWithHighRes(), this.highResReplaced_ = !0);
};
pagespeed.DelayImages.prototype.replaceWithHighRes = function() {
  this.replaceElementSrc(document.getElementsByTagName("img"));
  this.replaceElementSrc(document.getElementsByTagName("input"));
};
pagespeed.DelayImages.prototype.replaceWithHighRes = pagespeed.DelayImages.prototype.replaceWithHighRes;
pagespeed.delayImagesInit = function() {
  var a = new pagespeed.DelayImages;
  pagespeed.delayImages = a;
};
pagespeed.delayImagesInit = pagespeed.delayImagesInit;
})();
