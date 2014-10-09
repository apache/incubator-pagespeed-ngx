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
var ENTER_KEY_CODE = 13;
window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.ClientDomainRewriter = function(a) {
  this.mappedDomainNames_ = a;
};
pagespeed.ClientDomainRewriter.prototype.anchorListener = function(a) {
  a = a || window.event;
  if ("keypress" != a.type || a.keyCode == ENTER_KEY_CODE) {
    for (var b = a.target;null != b;b = b.parentNode) {
      if ("A" == b.tagName) {
        this.processEvent(b.href, a);
        break;
      }
    }
  }
};
pagespeed.ClientDomainRewriter.prototype.addEventListeners = function() {
  var a = this;
  document.body.onclick = function(b) {
    a.anchorListener(b);
  };
  document.body.onkeypress = function(b) {
    a.anchorListener(b);
  };
};
pagespeed.ClientDomainRewriter.prototype.processEvent = function(a, b) {
  for (var c = 0;c < this.mappedDomainNames_.length;c++) {
    if (0 == a.indexOf(this.mappedDomainNames_[c])) {
      window.location = window.location.protocol + "//" + window.location.hostname + "/" + a.substr(this.mappedDomainNames_[c].length);
      b.preventDefault();
      break;
    }
  }
};
pagespeed.clientDomainRewriterInit = function(a) {
  a = new pagespeed.ClientDomainRewriter(a);
  pagespeed.clientDomainRewriter = a;
  a.addEventListeners();
};
pagespeed.clientDomainRewriterInit = pagespeed.clientDomainRewriterInit;
})();
