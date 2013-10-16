(function(){var pagespeedutils = {sendBeacon:function(beaconUrl, htmlUrl, data) {
  var httpRequest;
  if (window.XMLHttpRequest) {
    httpRequest = new XMLHttpRequest;
  } else {
    if (window.ActiveXObject) {
      try {
        httpRequest = new ActiveXObject("Msxml2.XMLHTTP");
      } catch (e) {
        try {
          httpRequest = new ActiveXObject("Microsoft.XMLHTTP");
        } catch (e2) {
        }
      }
    }
  }
  if (!httpRequest) {
    return!1;
  }
  var query_param_char = -1 == beaconUrl.indexOf("?") ? "?" : "&", url = beaconUrl + query_param_char + "url=" + encodeURIComponent(htmlUrl);
  httpRequest.open("POST", url);
  httpRequest.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
  httpRequest.send(data);
  return!0;
}, addHandler:function(elem, eventName, func) {
  if (elem.addEventListener) {
    elem.addEventListener(eventName, func, !1);
  } else {
    if (elem.attachEvent) {
      elem.attachEvent("on" + eventName, func);
    } else {
      var oldHandler = elem["on" + eventName];
      elem["on" + eventName] = function() {
        func.call(this);
        oldHandler && oldHandler.call(this);
      };
    }
  }
}, getPosition:function(element) {
  for (var top = element.offsetTop, left = element.offsetLeft;element.offsetParent;) {
    element = element.offsetParent, top += element.offsetTop, left += element.offsetLeft;
  }
  return{top:top, left:left};
}, getWindowSize:function() {
  var height = window.innerHeight || document.documentElement.clientHeight || document.body.clientHeight, width = window.innerWidth || document.documentElement.clientWidth || document.body.clientWidth;
  return{height:height, width:width};
}, inViewport:function(element, windowSize) {
  var position = pagespeedutils.getPosition(element);
  return pagespeedutils.positionInViewport(position, windowSize);
}, positionInViewport:function(pos, windowSize) {
  return pos.top < windowSize.height && pos.left < windowSize.width;
}};
window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.LocalStorageCache = function() {
  this.regenerate_cookie_ = !0;
};
pagespeed.LocalStorageCache.prototype.hasExpired = function(obj) {
  var expiry = parseInt(obj.substring(0, obj.indexOf(" ")), 10);
  return!isNaN(expiry) && expiry <= Date.now();
};
pagespeed.LocalStorageCache.prototype.hasExpired = pagespeed.LocalStorageCache.prototype.hasExpired;
pagespeed.LocalStorageCache.prototype.getData = function(obj) {
  var pos1 = obj.indexOf(" "), pos2 = obj.indexOf(" ", pos1 + 1);
  return obj.substring(pos2 + 1);
};
pagespeed.LocalStorageCache.prototype.getData = pagespeed.LocalStorageCache.prototype.getData;
pagespeed.LocalStorageCache.prototype.replaceLastScript = function(newElement) {
  var scripts = document.getElementsByTagName("script"), lastElement = scripts[scripts.length - 1];
  lastElement.parentNode.replaceChild(newElement, lastElement);
};
pagespeed.LocalStorageCache.prototype.replaceLastScript = pagespeed.LocalStorageCache.prototype.replaceLastScript;
pagespeed.LocalStorageCache.prototype.inlineCss = function(url) {
  var obj = window.localStorage.getItem("pagespeed_lsc_url:" + url), newNode = document.createElement(obj ? "style" : "link");
  obj && !this.hasExpired(obj) ? (newNode.type = "text/css", newNode.appendChild(document.createTextNode(this.getData(obj)))) : (newNode.rel = "stylesheet", newNode.href = url, this.regenerate_cookie_ = !0);
  this.replaceLastScript(newNode);
};
pagespeed.LocalStorageCache.prototype.inlineCss = pagespeed.LocalStorageCache.prototype.inlineCss;
pagespeed.LocalStorageCache.prototype.inlineImg = function(url, hash) {
  var obj = window.localStorage.getItem("pagespeed_lsc_url:" + url + " pagespeed_lsc_hash:" + hash), newNode = document.createElement("img");
  obj && !this.hasExpired(obj) ? newNode.src = this.getData(obj) : (newNode.src = url, this.regenerate_cookie_ = !0);
  for (var i = 2, n = arguments.length;i < n;++i) {
    var pos = arguments[i].indexOf("=");
    newNode.setAttribute(arguments[i].substring(0, pos), arguments[i].substring(pos + 1));
  }
  this.replaceLastScript(newNode);
};
pagespeed.LocalStorageCache.prototype.inlineImg = pagespeed.LocalStorageCache.prototype.inlineImg;
pagespeed.LocalStorageCache.prototype.processTags_ = function(tagName, isHashInKey, dataFunc) {
  for (var elements = document.getElementsByTagName(tagName), i = 0, n = elements.length;i < n;++i) {
    var element = elements[i], hash = element.getAttribute("pagespeed_lsc_hash"), url = element.getAttribute("pagespeed_lsc_url");
    if (hash && url) {
      var urlkey = "pagespeed_lsc_url:" + url;
      isHashInKey && (urlkey += " pagespeed_lsc_hash:" + hash);
      var expiry = element.getAttribute("pagespeed_lsc_expiry"), millis = expiry ? (new Date(expiry)).getTime() : "", data = dataFunc(element);
      if (!data) {
        var obj = window.localStorage.getItem(urlkey);
        obj && (data = this.getData(obj));
      }
      data && (window.localStorage.setItem(urlkey, millis + " " + hash + " " + data), this.regenerate_cookie_ = !0);
    }
  }
};
pagespeed.LocalStorageCache.prototype.saveInlinedData_ = function() {
  this.processTags_("img", !0, function(e) {
    return e.src;
  });
  this.processTags_("style", !1, function(e) {
    return e.firstChild ? e.firstChild.nodeValue : null;
  });
};
pagespeed.LocalStorageCache.prototype.generateCookie_ = function() {
  if (this.regenerate_cookie_) {
    for (var deadUns = [], goodUns = [], minExpiry = 0, currentTime = Date.now(), i = 0, n = window.localStorage.length;i < n;++i) {
      var key = window.localStorage.key(i);
      if (!key.indexOf("pagespeed_lsc_url:")) {
        var obj = window.localStorage.getItem(key), pos1 = obj.indexOf(" "), expiry = parseInt(obj.substring(0, pos1), 10);
        if (!isNaN(expiry)) {
          if (expiry <= currentTime) {
            deadUns.push(key);
            continue;
          } else {
            if (expiry < minExpiry || 0 == minExpiry) {
              minExpiry = expiry;
            }
          }
        }
        var pos2 = obj.indexOf(" ", pos1 + 1), hash = obj.substring(pos1 + 1, pos2);
        goodUns.push(hash);
      }
    }
    var expires = "";
    minExpiry && (expires = "; expires=" + (new Date(minExpiry)).toUTCString());
    document.cookie = "_GPSLSC=" + goodUns.join("!") + expires;
    i = 0;
    for (n = deadUns.length;i < n;++i) {
      window.localStorage.removeItem(deadUns[i]);
    }
    this.regenerate_cookie_ = !1;
  }
};
pagespeed.localStorageCacheInit = function() {
  if (window.localStorage) {
    var temp = new pagespeed.LocalStorageCache;
    pagespeed.localStorageCache = temp;
    pagespeedutils.addHandler(window, "load", function() {
      temp.saveInlinedData_();
    });
    pagespeedutils.addHandler(window, "load", function() {
      temp.generateCookie_();
    });
  }
};
pagespeed.localStorageCacheInit = pagespeed.localStorageCacheInit;
})();
