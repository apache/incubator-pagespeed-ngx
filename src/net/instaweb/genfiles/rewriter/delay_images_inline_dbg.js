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
    return!1;
  }
  var e = -1 == a.indexOf("?") ? "?" : "&";
  a = a + e + "url=" + encodeURIComponent(b);
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
pagespeed.DelayImagesInline = function() {
  this.inlineMap_ = {};
};
pagespeed.DelayImagesInline.prototype.addLowResImages = function(a, b) {
  this.inlineMap_[a] = b;
};
pagespeed.DelayImagesInline.prototype.addLowResImages = pagespeed.DelayImagesInline.prototype.addLowResImages;
pagespeed.DelayImagesInline.prototype.replaceElementSrc = function(a) {
  for (var b = 0;b < a.length;++b) {
    var c = a[b].getAttribute("pagespeed_high_res_src"), d = a[b].getAttribute("src");
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
