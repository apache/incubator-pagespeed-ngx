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
var orig_date = Date, random_count = 0, date_count = 0, random_seed = .462, time_seed = 1204251968254, random_count_threshold = 25, date_count_threshold = 25;
Math.random = function() {
  random_count++;
  random_count > random_count_threshold && (random_seed += .1, random_count = 1);
  return random_seed % 1;
};
Date = function() {
  if (this instanceof Date) {
    switch(date_count++, date_count > date_count_threshold && (time_seed += 50, date_count = 1), arguments.length) {
      case 0:
        return new orig_date(time_seed);
      case 1:
        return new orig_date(arguments[0]);
      default:
        return new orig_date(arguments[0], arguments[1], 3 <= arguments.length ? arguments[2] : 1, 4 <= arguments.length ? arguments[3] : 0, 5 <= arguments.length ? arguments[4] : 0, 6 <= arguments.length ? arguments[5] : 0, 7 <= arguments.length ? arguments[6] : 0);
    }
  }
  return(new Date).toString();
};
Date.__proto__ = orig_date;
Date.prototype.constructor = Date;
orig_date.now = function() {
  return(new Date).getTime();
};
})();
