(function(){var pagespeedutils = {MAX_POST_SIZE:131072, sendBeacon:function(a, c, b) {
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
  a = a + f + "url=" + encodeURIComponent(c);
  d.open("POST", a);
  d.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
  d.send(b);
  return!0;
}, addHandler:function(a, c, b) {
  if (a.addEventListener) {
    a.addEventListener(c, b, !1);
  } else {
    if (a.attachEvent) {
      a.attachEvent("on" + c, b);
    } else {
      var d = a["on" + c];
      a["on" + c] = function() {
        b.call(this);
        d && d.call(this);
      };
    }
  }
}, getPosition:function(a) {
  for (var c = a.offsetTop, b = a.offsetLeft;a.offsetParent;) {
    a = a.offsetParent, c += a.offsetTop, b += a.offsetLeft;
  }
  return{top:c, left:b};
}, getWindowSize:function() {
  return{height:window.innerHeight || document.documentElement.clientHeight || document.body.clientHeight, width:window.innerWidth || document.documentElement.clientWidth || document.body.clientWidth};
}, inViewport:function(a, c) {
  var b = pagespeedutils.getPosition(a);
  return pagespeedutils.positionInViewport(b, c);
}, positionInViewport:function(a, c) {
  return a.top < c.height && a.left < c.width;
}, getRequestAnimationFrame:function() {
  return window.requestAnimationFrame || window.webkitRequestAnimationFrame || window.mozRequestAnimationFrame || window.oRequestAnimationFrame || window.msRequestAnimationFrame || null;
}};
window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.AddInstrumentation = function(a, c, b, d) {
  this.beaconUrlPrefix_ = a;
  this.event_ = c;
  this.extraParams_ = b;
  this.htmlUrl_ = d;
};
pagespeed.beaconUrl = "";
pagespeed.AddInstrumentation.prototype.sendBeacon = function() {
  var a = this.beaconUrlPrefix_, c = window.mod_pagespeed_start, b = Number(new Date) - c, a = a + (-1 == a.indexOf("?") ? "?" : "&"), a = a + "ets=" + ("load" == this.event_ ? "load:" : "unload:"), a = a + b;
  if ("beforeunload" != this.event_ || !window.mod_pagespeed_loaded) {
    a += "&r" + this.event_ + "=";
    if (window.performance) {
      var b = window.performance.timing, d = b.navigationStart, e = b.requestStart, a = a + (b[this.event_ + "EventStart"] - d), a = a + ("&nav=" + (b.fetchStart - d)), a = a + ("&dns=" + (b.domainLookupEnd - b.domainLookupStart)), a = a + ("&connect=" + (b.connectEnd - b.connectStart)), a = a + ("&req_start=" + (e - d)), a = a + ("&ttfb=" + (b.responseStart - e)), a = a + ("&dwld=" + (b.responseEnd - b.responseStart)), a = a + ("&dom_c=" + (b.domContentLoadedEventStart - d));
      window.performance.navigation && (a += "&nt=" + window.performance.navigation.type);
      d = -1;
      b.msFirstPaint ? d = b.msFirstPaint : window.chrome && window.chrome.loadTimes && (d = Math.floor(1E3 * window.chrome.loadTimes().firstPaintTime));
      d -= e;
      0 <= d && (a += "&fp=" + d);
    } else {
      a += b;
    }
    pagespeed.getResourceTimingData && window.parent == window && (a += pagespeed.getResourceTimingData());
    a += window.parent != window ? "&ifr=1" : "&ifr=0";
    "load" == this.event_ && (window.mod_pagespeed_loaded = !0, (b = window.mod_pagespeed_num_resources_prefetched) && (a += "&nrp=" + b), (b = window.mod_pagespeed_prefetch_start) && (a += "&htmlAt=" + (c - b)));
    pagespeed.panelLoader && (c = pagespeed.panelLoader.getCsiTimingsString(), "" != c && (a += "&b_csi=" + c));
    pagespeed.criticalCss && (c = pagespeed.criticalCss, a += "&ccis=" + c.total_critical_inlined_size + "&cces=" + c.total_original_external_size + "&ccos=" + c.total_overhead_size + "&ccrl=" + c.num_replaced_links + "&ccul=" + c.num_unreplaced_links);
    "" != this.extraParams_ && (a += this.extraParams_);
    document.referrer && (a += "&ref=" + encodeURIComponent(document.referrer));
    a += "&url=" + encodeURIComponent(this.htmlUrl_);
    pagespeed.beaconUrl = a;
    (new Image).src = a;
  }
};
pagespeed.addInstrumentationInit = function(a, c, b, d) {
  var e = new pagespeed.AddInstrumentation(a, c, b, d);
  window.addEventListener ? window.addEventListener(c, function() {
    e.sendBeacon();
  }, !1) : window.attachEvent("on" + c, function() {
    e.sendBeacon();
  });
};
pagespeed.addInstrumentationInit = pagespeed.addInstrumentationInit;
})();
