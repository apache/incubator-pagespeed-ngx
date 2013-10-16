(function(){window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.ClientDomainRewriter = function(mappedDomainNames) {
  this.mappedDomainNames_ = mappedDomainNames;
};
pagespeed.ClientDomainRewriter.prototype.anchorListener = function(event) {
  event = event || window.event;
  if ("keypress" != event.type || 13 == event.keyCode) {
    for (var target = event.target;null != target;target = target.parentNode) {
      if ("A" == target.tagName) {
        this.processEvent(target.href, event);
        break;
      }
    }
  }
};
pagespeed.ClientDomainRewriter.prototype.addEventListeners = function() {
  var me = this;
  document.body.onclick = function(event) {
    me.anchorListener(event);
  };
  document.body.onkeypress = function(event) {
    me.anchorListener(event);
  };
};
pagespeed.ClientDomainRewriter.prototype.processEvent = function(url, event) {
  for (var i = 0;i < this.mappedDomainNames_.length;i++) {
    if (0 == url.indexOf(this.mappedDomainNames_[i])) {
      window.location = window.location.protocol + "//" + window.location.hostname + "/" + url.substr(this.mappedDomainNames_[i].length);
      event.preventDefault();
      break;
    }
  }
};
pagespeed.clientDomainRewriterInit = function(mappedDomainNames) {
  var clientDomainRewriter = new pagespeed.ClientDomainRewriter(mappedDomainNames);
  pagespeed.clientDomainRewriter = clientDomainRewriter;
  clientDomainRewriter.addEventListeners();
};
pagespeed.clientDomainRewriterInit = pagespeed.clientDomainRewriterInit;
})();
