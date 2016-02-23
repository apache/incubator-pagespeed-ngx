(function(){var f=this;
function h(a){var b=typeof a;if("object"==b)if(a){if(a instanceof Array)return"array";if(a instanceof Object)return b;var c=Object.prototype.toString.call(a);if("[object Window]"==c)return"object";if("[object Array]"==c||"number"==typeof a.length&&"undefined"!=typeof a.splice&&"undefined"!=typeof a.propertyIsEnumerable&&!a.propertyIsEnumerable("splice"))return"array";if("[object Function]"==c||"undefined"!=typeof a.call&&"undefined"!=typeof a.propertyIsEnumerable&&!a.propertyIsEnumerable("call"))return"function"}else return"null";else if("function"==
b&&"undefined"==typeof a.call)return"object";return b}function l(a,b,c){return a.call.apply(a.bind,arguments)}function m(a,b,c){if(!a)throw Error();if(2<arguments.length){var d=Array.prototype.slice.call(arguments,2);return function(){var c=Array.prototype.slice.call(arguments);Array.prototype.unshift.apply(c,d);return a.apply(b,c)}}return function(){return a.apply(b,arguments)}}
function p(a,b,c){p=Function.prototype.bind&&-1!=Function.prototype.bind.toString().indexOf("native code")?l:m;return p.apply(null,arguments)}var q=Date.now||function(){return+new Date};function r(a,b){function c(){}c.prototype=b.prototype;a.A=b.prototype;a.prototype=new c;a.w=function(a,c,g){for(var n=Array(arguments.length-2),k=2;k<arguments.length;k++)n[k-2]=arguments[k];return b.prototype[c].apply(a,n)}};function t(a){if(Error.captureStackTrace)Error.captureStackTrace(this,t);else{var b=Error().stack;b&&(this.stack=b)}a&&(this.message=String(a))}r(t,Error);t.prototype.name="CustomError";function u(a,b){for(var c=a.split("%s"),d="",e=Array.prototype.slice.call(arguments,1);e.length&&1<c.length;)d+=c.shift()+e.shift();return d+c.join("%s")};function v(a,b){b.unshift(a);t.call(this,u.apply(null,b));b.shift()}r(v,t);v.prototype.name="AssertionError";function w(a,b){throw new v("Failure"+(a?": "+a:""),Array.prototype.slice.call(arguments,1));};function x(a,b,c,d,e){this.reset(a,b,c,d,e)}x.prototype.j=null;var y=0;x.prototype.reset=function(a,b,c,d,e){"number"==typeof e||y++;d||q();this.c=a;this.u=b;delete this.j};x.prototype.m=function(a){this.c=a};x.prototype.getMessage=function(){return this.u};function z(a){this.v=a;this.l=this.h=this.c=this.f=null}function A(a,b){this.name=a;this.value=b}A.prototype.toString=function(){return this.name};var B=new A("WARNING",900),C=new A("CONFIG",700);z.prototype.getParent=function(){return this.f};z.prototype.getChildren=function(){this.h||(this.h={});return this.h};z.prototype.m=function(a){this.c=a};function D(a){if(a.c)return a.c;if(a.f)return D(a.f);w("Root logger has no level set.");return null}
z.prototype.log=function(a,b,c){if(a.value>=D(this).value)for("function"==h(b)&&(b=b()),a=new x(a,String(b),this.v),c&&(a.j=c),c="log:"+a.getMessage(),f.console&&(f.console.timeStamp?f.console.timeStamp(c):f.console.markTimeline&&f.console.markTimeline(c)),f.msWriteProfilerMark&&f.msWriteProfilerMark(c),c=this;c;){b=c;var d=a;if(b.l)for(var e=0,g=void 0;g=b.l[e];e++)g(d);c=c.getParent()}};var E={},F=null;
function G(a){F||(F=new z(""),E[""]=F,F.m(C));var b;if(!(b=E[a])){b=new z(a);var c=a.lastIndexOf("."),d=a.substr(c+1),c=G(a.substr(0,c));c.getChildren()[d]=b;b.f=c;E[a]=b}return b};function H(a){this.i=a;this.i._wect=this;this.b={};this.a={};this.g={}}H.prototype.s=G("wireless.events.ListenerCoalescer");function I(a){a._wect||new H(a);return a._wect}H.prototype.o=function(a,b){void 0==this.b[a]&&(this.b[a]=0);this.b[a]++;for(var c=this.a[a],d=c.length,e,g=0;g<d;g++)try{c[g](b)}catch(k){var n=this.s;n&&n.log(B,"Exception during event processing.",k);e=e||k}this.b[a]--;if(e)throw e;};function J(a,b){a.g[b]||(a.g[b]=p(a.o,a,b));return a.g[b]}
function K(a,b,c){var d=b+":capture";a.a[d]||(a.a[d]=[],a.i.addEventListener(b,J(a,d),!0));a.a[d].push(c)};function L(){var a=M,b=document,c=N,d=I(b);K(d,c,a);O(b,function(){K(d,c,a)},function(){var b=c+":capture";if(d.a[b]){d.b[b]&&(d.a[b]=d.a[b].slice(0));var g=d.a[b].indexOf(a);-1!=g&&d.a[b].splice(g,1);0==d.a[b].length&&(d.a[b]=void 0,d.i.removeEventListener(c,J(d,b),!0))}})}function O(a,b,c){a.addEventListener("DOMFocusIn",function(a){a.target&&"TEXTAREA"==a.target.tagName&&b()},!1);a.addEventListener("DOMFocusOut",function(a){a.target&&"TEXTAREA"==a.target.tagName&&c()},!1)};function P(a,b){this.x=void 0!==a?a:0;this.y=void 0!==b?b:0}P.prototype.toString=function(){return"("+this.x+", "+this.y+")"};var Q=/Mac OS X.+Silk\//,R=/Chrome\/([0-9.]+)/;var S=/iPhone|iPod|iPad/.test(navigator.userAgent)||-1!=navigator.userAgent.indexOf("Android")||Q.test(navigator.userAgent),T=window.navigator.msPointerEnabled,N=S?"touchstart":T?"MSPointerDown":"mousedown",aa=S?"touchend":T?"MSPointerUp":"mouseup";function ba(){var a=M;return function(b){b.touches=[];b.targetTouches=[];b.changedTouches=[];b.type!=aa&&(b.touches[0]=b,b.targetTouches[0]=b);b.changedTouches[0]=b;a(b)}}
function U(a){var b;if(b=-1!=navigator.userAgent.indexOf("Android")&&-1!=navigator.userAgent.indexOf("Chrome/"))b=R.exec(navigator.userAgent),b=18==+(b?b[1]:"").split(".")[0];return b?new P(a.clientX,a.pageY-window.scrollY):new P(a.clientX,a.clientY)};var V,W,X,Y=G("wireless.events.clickbuster");function ca(a){if(!(2500<q()-W)){var b=U(a);if(1>b.x&&1>b.y)Y&&Y.log(B,"Not busting click on label elem at ("+b.x+", "+b.y+")",void 0);else{for(var c=0;c<V.length;c+=2)if(25>Math.abs(b.x-V[c])&&25>Math.abs(b.y-V[c+1])){V.splice(c,c+2);return}Y&&Y.log(B,"busting click at "+b.x+", "+b.y,void 0);a.stopPropagation();a.preventDefault();(a=X)&&a()}}}
function da(a){var b=U((a.touches||[a])[0]);V.push(b.x,b.y);window.setTimeout(function(){for(var a=b.x,d=b.y,e=0;e<V.length;e+=2)if(V[e]==a&&V[e+1]==d){V.splice(e,e+2);break}X=void 0},2500)};X=void 0;if(!V){document.addEventListener("click",ca,!0);var M=da;S||T||(M=ba());L();V=[]}W=q();for(var Z=0;Z<V.length;Z+=2)if(25>Math.abs(0-V[Z])&&25>Math.abs(0-V[Z+1])){V.splice(Z,Z+2);break};})();