(function(){var m,n=this;function q(a,b){var c=a.split("."),d=n;c[0]in d||!d.execScript||d.execScript("var "+c[0]);for(var e;c.length&&(e=c.shift());)c.length||void 0===b?d=d[e]?d[e]:d[e]={}:d[e]=b}
function aa(a){var b=typeof a;if("object"==b)if(a){if(a instanceof Array)return"array";if(a instanceof Object)return b;var c=Object.prototype.toString.call(a);if("[object Window]"==c)return"object";if("[object Array]"==c||"number"==typeof a.length&&"undefined"!=typeof a.splice&&"undefined"!=typeof a.propertyIsEnumerable&&!a.propertyIsEnumerable("splice"))return"array";if("[object Function]"==c||"undefined"!=typeof a.call&&"undefined"!=typeof a.propertyIsEnumerable&&!a.propertyIsEnumerable("call"))return"function"}else return"null";
else if("function"==b&&"undefined"==typeof a.call)return"object";return b}function r(a){return"string"==typeof a};var s=String.prototype.trim?function(a){return a.trim()}:function(a){return a.replace(/^[\s\xa0]+|[\s\xa0]+$/g,"")};function t(a,b){return a<b?-1:a>b?1:0};var u;a:{var w=n.navigator;if(w){var x=w.userAgent;if(x){u=x;break a}}u=""};var y=Array.prototype,ba=y.indexOf?function(a,b,c){return y.indexOf.call(a,b,c)}:function(a,b,c){c=null==c?0:0>c?Math.max(0,a.length+c):c;if(r(a))return r(b)&&1==b.length?a.indexOf(b,c):-1;for(;c<a.length;c++)if(c in a&&a[c]===b)return c;return-1},ca=y.filter?function(a,b,c){return y.filter.call(a,b,c)}:function(a,b,c){for(var d=a.length,e=[],f=0,g=r(a)?a.split(""):a,h=0;h<d;h++)if(h in g){var k=g[h];b.call(c,k,h,a)&&(e[f++]=k)}return e};function da(a){return y.concat.apply(y,arguments)}
function z(a){return y.concat.apply(y,arguments)}function A(a){var b=a.length;if(0<b){for(var c=Array(b),d=0;d<b;d++)c[d]=a[d];return c}return[]};function B(a){if(a.classList)return a.classList;a=a.className;return r(a)&&a.match(/\S+/g)||[]}function C(a,b){var c;a.classList?c=a.classList.contains(b):(c=B(a),c=0<=ba(c,b));return c}function D(a,b){a.classList?a.classList.add(b):C(a,b)||(a.className+=0<a.className.length?" "+b:b)}function ea(a,b){a.classList?a.classList.remove(b):C(a,b)&&(a.className=ca(B(a),function(a){return a!=b}).join(" "))}function E(a,b){C(a,b)?ea(a,b):D(a,b)};var fa=-1!=u.indexOf("Opera")||-1!=u.indexOf("OPR"),F=-1!=u.indexOf("Trident")||-1!=u.indexOf("MSIE"),G=-1!=u.indexOf("Gecko")&&-1==u.toLowerCase().indexOf("webkit")&&!(-1!=u.indexOf("Trident")||-1!=u.indexOf("MSIE")),ga=-1!=u.toLowerCase().indexOf("webkit");function H(){var a=n.document;return a?a.documentMode:void 0}
var I=function(){var a="",b;if(fa&&n.opera)return a=n.opera.version,"function"==aa(a)?a():a;G?b=/rv\:([^\);]+)(\)|;)/:F?b=/\b(?:MSIE|rv)[: ]([^\);]+)(\)|;)/:ga&&(b=/WebKit\/(\S+)/);b&&(a=(a=b.exec(u))?a[1]:"");return F&&(b=H(),b>parseFloat(a))?String(b):a}(),ha={};
function ia(a){if(!ha[a]){for(var b=0,c=s(String(I)).split("."),d=s(String(a)).split("."),e=Math.max(c.length,d.length),f=0;0==b&&f<e;f++){var g=c[f]||"",h=d[f]||"",k=RegExp("(\\d*)(\\D*)","g"),p=RegExp("(\\d*)(\\D*)","g");do{var l=k.exec(g)||["","",""],v=p.exec(h)||["","",""];if(0==l[0].length&&0==v[0].length)break;b=t(0==l[1].length?0:parseInt(l[1],10),0==v[1].length?0:parseInt(v[1],10))||t(0==l[2].length,0==v[2].length)||t(l[2],v[2])}while(0==b)}ha[a]=0<=b}}
var ja=n.document,ma=ja&&F?H()||("CSS1Compat"==ja.compatMode?parseInt(I,10):5):void 0;var J;if(!(J=!G&&!F)){var K;if(K=F)K=F&&9<=ma;J=K}J||G&&ia("1.9.1");F&&ia("9");function L(a){var b=null;a&&(b=a.indexOf("px"),-1!=b&&(a=a.substring(0,b)),b=parseInt(a,10),isNaN(b)&&(b=null));return b}function M(a,b){var c=null;a&&(c=L(a.getPropertyValue(b)));return c}function N(a,b){a.style&&a.style.removeProperty(b);a.removeAttribute(b)}function na(a,b){var c=null;a.style&&(c=L(a.style.getPropertyValue(b)));null==c&&(c=L(a.getAttribute(b)));return c}function O(a,b,c){a.style.setProperty(b,c,"important")}
function P(a,b){if(b&&0!=b.length){var c=a.getAttribute("style")||"";0<c.length&&";"!=c[c.length-1]&&(c+=";");a.setAttribute("style",c+b)}}function Q(a){var b=null;"SCRIPT"!=a.tagName&&"STYLE"!=a.tagName&&a.style&&(a=window.getComputedStyle(a))&&(b=a.getPropertyValue("background-image"),"none"==b&&(b=null),b&&5<b.length&&0==b.indexOf("url(")&&")"==b[b.length-1]&&(b=b.substring(4,b.length-1)));return b}
function R(){if(null!=window.parent&&window!=window.parent)try{if(window.parent.document.domain==document.domain)return!0}catch(a){}return!1}function oa(a){var b=1;for(a=a.firstChild;a;a=a.nextSibling)b+=oa(a);return b};function S(a){this.k=a;this.p={};if(this.a=document.documentElement.clientWidth){a=window.getComputedStyle(document.body);for(var b=["margin-left","margin-right","padding-left","padding-right"],c=0;c<b.length;++c){var d=M(a,b[c]);d&&(this.a-=d)}}else this.a=400;console.log("window.pagespeed.MobLayout.maxWidth="+this.a)}
var pa="padding-left padding-bottom padding-right padding-top margin-left margin-bottom margin-right margin-top border-left-width border-bottom-width border-right-width border-top-width left top".split(" "),qa=["left","width"];
function ra(a,b){return!b||!b.style||"SCRIPT"==b.tagName||"STYLE"==b.tagName||"IFRAME"==b.tagName||b.id&&a.p[b.id]||b.classList.contains("psmob-nav-panel")||b.classList.contains("psmob-header-bar")||b.classList.contains("psmob-header-spacer-div")||b.classList.contains("psmob-logo-span")}function T(a,b){return ra(a,b)?null:b}m=S.prototype;m.children=function(a){var b=[];for(a=a.firstChild;a;a=a.nextSibling)null!=(a.tagName?a:null)&&b.push(a);return b};
function U(a,b,c){c=c.bind(a);for(b=b.firstChild;b;b=b.nextSibling)null!=T(a,b)&&c(b)}
m.v=function(a){var b=Q(a);if(b){var b=this.k.e[b],c=window.getComputedStyle(a);if(b&&b.width&&b.height){var d=b.height;c.getPropertyValue("background-repeat");if(b.width>this.a){var d=Math.round(this.a/b.width*b.height),e="background-size:"+this.a+"px "+d+"px;background-repeat:no-repeat;",c=M(c,"height");b.height==c&&(e+="height:"+d+"px;");P(a,e)}O(a,"min-height",""+d+"px")}}"PRE"==a.tagName&&(a.style.overflowX="scroll");U(this,a,this.v)};
function sa(a){O(a,"overflow-x","auto");O(a,"width","auto");O(a,"display","block")}
m.u=function(a,b){var c,d;d=a.getBoundingClientRect();c=document.body;var e=document.documentElement||c.parentNode||c;c="pageXOffset"in window?window.pageXOffset:e.scrollLeft;e="pageYOffset"in window?window.pageYOffset:e.scrollTop;d.top+=e;d.bottom+=e;d.left+=c;d.right+=c;if(d)c=d.top,d=d.bottom;else{c=b;if(a.offsetParent==a.parentNode)c+=a.offsetTop;else if(a.offsetParent!=a.parentNode.parentNode)return null;d=c+a.offsetHeight-1}if(ra(this,a))return d;d=c-1;e=window.getComputedStyle(a);if(!e)return null;
var f=M(e,"min-height");null!=f&&(d+=f);for(var f=c+a.offsetHeight-1,g=!1,h=!1,k,p=a.firstChild;p;p=p.nextSibling)if(k=p.tagName?p:null){var l=window.getComputedStyle(k);l&&"absolute"==l.getPropertyValue("position")&&"0px"!=l.getPropertyValue("height")&&(h=!0);k=this.u(k,c);null!=k&&(g=!0,d=Math.max(d,k))}if("fixed"==e.getPropertyValue("position")&&g)return null;"BODY"!=a.tagName&&(e=f-c+1,g?d!=f&&(h?O(a,"height",""+(d-c+1)+"px"):O(a,"height","auto")):("IMG"!=a.tagName&&0<e&&""==a.style.backgroundSize&&
(N(a,"height"),O(a,"height","auto"),a.offsetHeight&&(f=c+a.offsetHeight)),d=f));return d};
m.t=function(a){for(var b=this.children(a),c=0;c<b.length;++c)this.t(b[c]);b=!1;if(a.offsetWidth>this.a)if(b=!1,"TABLE"==a.tagName)if(ta(this,a))sa(a);else if("CSS1Compat"!==document.compatMode){var d,e,f,g,h,k,p=document.createElement("DIV");p.style.display="inline-block";for(var l=this.children(a),c=0;c<l.length;++c){var v=this.children(l[c]);for(d=0;d<v.length;++d){var ka=this.children(v[d]);for(e=0;e<ka.length;++e)if(h=ka[e],1==h.childNodes.length)g=h.childNodes[0],h.removeChild(g),p.appendChild(g);
else if(1<h.childNodes.length){k=document.createElement("DIV");k.style.display="inline-block";var la=this.children(h);for(f=0;f<la.length;++f)g=la[f],h.removeChild(g),k.appendChild(g);p.appendChild(k)}}}a.parentNode.replaceChild(p,a)}else for(N(a,"width"),O(a,"max-width","100%"),c=a.firstChild;c;c=c.nextSibling){if(d=c.tagName?c:null,null!=d)for(N(d,"width"),O(d,"max-width","100%"),d=d.firstChild;d;d=d.nextSibling)if(e=d.tagName?d:null,null!=e&&"TR"==e.tagName)for(N(e,"width"),O(e,"max-width","100%"),
e=e.firstChild;e;e=e.nextSibling)f=e.tagName?e:null,null!=f&&"TD"==f.tagName&&(O(f,"max-width","100%"),O(f,"display","inline-block"))}else{c=null;d=a.offsetWidth;e=a.offsetHeight;f="img";if("IMG"==a.tagName)c=a.getAttribute("src");else if(f="background-image",c=Q(a),g=null==c?null:this.k.e[c])d=g.width,e=g.height;null!=c?(g=d/this.a,1<g&&(g=e/g,console.log("Shrinking "+f+" "+c+" from "+d+"x"+e+" to "+this.a+"x"+g),"IMG"==a.tagName?(O(a,"width",""+this.a+"px"),O(a,"height",""+g+"px")):O(a,"background-size",
""+this.a+"px "+g+"px"))):"CODE"==a.tagName||"PRE"==a.tagName||"UL"==a.tagName?sa(a):"P"==a.tagName||"FORM"==a.tagName||"H1"==a.tagName||"H2"==a.tagName||"H3"==a.tagName||"H4"==a.tagName||"A"==a.tagName||"SPAN"==a.tagName||"TR"==a.tagName||"TBODY"==a.tagName||"THEAD"==a.tagName||"TFOOT"==a.tagName||"TD"==a.tagName||"TH"==a.tagName||"DIV"==a.tagName?(O(a,"max-width","100%"),N(a,"width")):console.log("Punting on resize of "+a.tagName+" which wants to be "+a.offsetWidth+" but this.maxWidth_="+this.a)}if(b&&
"TABLE"==a.tagName){a:{b=a;c=document.getElementsByTagName("*");for(d=[];b&&1==b.nodeType;b=b.parentNode)if(b.hasAttribute("id")){for(f=e=0;f<c.length&&1>=e;++f)c[f].hasAttribute("id")&&c[f].id==b.id&&++e;if(1==e){d.unshift('id("'+b.getAttribute("id")+'")');b=d.join("/");break a}d.unshift(b.localName.toLowerCase()+'[@id="'+b.getAttribute("id")+'"]')}else if(b.hasAttribute("class"))d.unshift(b.localName.toLowerCase()+'[@class="'+b.getAttribute("class")+'"]');else{e=1;for(f=b.previousSibling;f;f=f.previousSibling)f.localName==
b.localName&&e++;d.unshift(b.localName.toLowerCase()+"["+e+"]")}b=d.length?"/"+d.join("/"):null}ta(this,a)?console.log("data-table: "+b):console.log("layout-table: "+b)}};function ua(a,b){var c=0;"DIV"!=b.tagName&&"TABLE"!=b.tagName&&"UL"!=b.tagName||++c;for(var d=b.firstChild;d;d=d.nextSibling)c+=ua(a,d);return c}
function ta(a,b){for(var c=0,d=b.firstChild;d;d=d.nextSibling)for(var e=d.firstChild;e;e=e.nextSibling){if("THEAD"==d.tagName||"TFOOT"==d.tagName)return!0;for(var f=e.firstChild;f;f=f.nextSibling){if("TH"==f.tagName)return!0;++c}}return 3*ua(a,b)>c?!1:!0}m.D=function(a){var b=document.body.style.display;document.body.style.display="none";this.n(a);document.body.style.display=b};
m.n=function(a){if(a.style){var b=window.getComputedStyle(a);"nowrap"==b.getPropertyValue("white-space")&&O(a,"white-space","normal");U(this,a,this.n);var b=window.getComputedStyle(a),c,d,e;for(c=0;c<qa.length;++c)d=qa[c],(e=b.getPropertyValue(d))&&"100%"!=e&&"auto"!=e&&0<e.length&&"%"==e[e.length-1]&&O(a,d,"auto");var f="UL"==a.tagName||"OL"==a.tagName,g="BODY"==a.tagName,h="";for(c=0;c<pa.length;++c){d=pa[c];if(e=f)e=d.length-5,e=0<=e&&d.indexOf("-left",e)==e;e||g&&0==d.lastIndexOf("margin-",0)||
(e=M(b,d),null!=e&&(4<e?h+=d+":4px !important;":0>e&&(h+=d+":0px !important;")))}P(a,h)}};
m.s=function(a){U(this,a,this.s);if("IMG"==a.tagName){var b=window.getComputedStyle(a),c=na(a,"width"),d=na(a,"height");if(c&&d&&b){var e=M(b,"width"),b=M(b,"height");if(e&&b&&(e/=c,b/=d,.95<(e>b?b/e:e/b)||(console.log("aspect ratio problem for "+a.getAttribute("src")),1==a.naturalHeight&&1==a.naturalWidth?(b=Math.min(e,b),N(a,"width"),N(a,"height"),a.style.width=c*b,a.style.height=d*b):e>b?N(a,"height"):(N(a,"width"),N(a,"height"),a.style.maxHeight=d)),.25>e)){for(console.log("overshrinkage for "+
a.getAttribute("src"));a&&"TD"!=a.tagName;)a=a.parentNode;if(a&&(c=a.parentNode)){d=0;for(a=c.firstChild;a;a=a.nextSibling)"TD"==a.tagName&&++d;if(1<d)for(d="width:"+Math.round(100/d)+"%;",c=c.firstChild;c;c=c.nextSibling)a=c.tagName?c:null,null!=a&&"TD"==a.tagName&&P(a,d)}}}}};
m.w=function(a){var b=window.getComputedStyle(a).getPropertyValue("position");if("fixed"==b)return"fixed";var c,d,e,f=[],g=[];for(d=a.firstChild;d;d=d.nextSibling)c=d.tagName?d:null,null!=c&&(c.H=[c.offsetLeft+c.offsetWidth/2,c.offsetTop+c.offsetHeight/2]);for(d=a.firstChild;d;d=d.nextSibling)if(c=T(this,d),null!=c&&(e=this.w(c),"fixed"!=e&&null!=T(this,c)))if("absolute"==e)g.push(c);else{var h=window.getComputedStyle(c);e=h.getPropertyValue("float");var k="right"==e;if(k||"left"==e)O(c,"float","none"),
"none"!=h.getPropertyValue("display")&&O(c,"display","inline-block");k&&f.push(c)}for(c=f.length-1;0<=c;--c)d=f[c],a.removeChild(d);for(c=f.length-1;0<=c;--c)d=f[c],a.appendChild(d);return b};
m.q=function(a){if("fixed"!=window.getComputedStyle(a).getPropertyValue("position")){var b,c,d=[],e=[];for(c=a.firstChild;c;c=c.nextSibling)if(a=T(this,c),null!=a){var f=window.getComputedStyle(a);b=f.getPropertyValue("position");"fixed"!=b&&"absolute"!=b&&0!=c.offsetWidth&&(d.push(a),e.push(f))}var g=null;for(c=0;c<d.length;++c)a=d[c],b=c<d.length-1?d[c+1]:null,f=a.offsetLeft+a.offsetWidth,(null==g||a.offsetLeft<g)&&(null==b||b.offsetLeft<f)&&(b=a,"INPUT"!=b.tagName&&"SELECT"!=b.tagName&&(""==b.style.backgroundSize&&
"auto"!=e[c].getPropertyValue("width")&&O(b,"width","auto"),"IMG"!=b.tagName&&(g=b.getAttribute("width"),null!=g&&b.removeAttribute(g)),N(b,"border-left"),N(b,"border-right"),N(b,"margin-left"),N(b,"margin-right"),N(b,"padding-left"),N(b,"padding-right"),b.className=""!=b.className?b.className+" psSingleColumn":"psSingleColumn"),this.q(a)),g=f}};
var V=[S.prototype.v,"shrink wide elements",S.prototype.w,"string floats",S.prototype.D,"cleanup styles",S.prototype.s,"repair distored images",S.prototype.t,"resize if too wide",S.prototype.q,"expand columns",S.prototype.u,"resize vertically"];function va(){this.i=[];this.A=!0}va.prototype.B="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAJAAAACQCAQAAABNTyozAAAAIGNIUk0AAHomAACAhAAA+gAAAIDoAAB1MAAA6mAAADqYAAAXcJy6UTwAAAACYktHRAD/h4/MvwAAAAlwSFlzAAALEwAACxMBAJqcGAAAATdJREFUeNrt0klOBTEQREHfDI7FTEn/4M2GBULQo22wK+ICKT1lKQAAAAAAAAAAAAAAAAAA/Jm7rmv3o+W5laU8d1uLspQYLc/SLVF8rsVoefokii9rMVqe9oni21qMlqdtovhhLUbL0y5R/LIWo+VZylKeuuX5t4nW8tRPFBtrMV6gmolic+t9xA/VSjRonl6JBs7TI9HgeVonmiDPvkSPmfO0SjRRnhaJJstTO9GEeWom2s7zVgZVI9HEeWokmjzPvkQPmfNcSZQkz9lEifKcSZQsz9FECfMcSZQ0z95E23ley8S2E6XOcz3R9HmuJUqR53yiNHnOJUqV53iidHmOJUqZZ3+itHn2JXopyd3kOZ9IntVE8qwmkmc1kTyrieRZTSTPaiJ5AAAAAAAAAAAAAAAAAGjgA62rM0XB6dNxAAAAAElFTkSuQmCC";
function W(a,b,c){var d=[];for(b=b.firstChild;b;b=b.nextSibling)"UL"==b.tagName?d=z(d,W(a,b,c+1)):("A"==b.tagName&&(b.setAttribute("data-mobilize-nav-level",c),d.push(b)),d=z(d,W(a,b,c)));return d}function wa(){var a=document.querySelector(".psmob-nav-panel"),b=document.querySelector(".psmob-header-bar");E(b,"open");E(a,"open");E(document.body,"noscroll")}
function xa(a){var b=document.querySelector(".psmob-menu-button");document.body.addEventListener("click",function(a){if(b.contains(a.target))wa();else{var d=document.querySelector(".psmob-nav-panel");C(d,"open")&&!d.contains(a.target)&&(wa(),a.stopPropagation(),a.preventDefault())}}.bind(a),!0)}
function ya(){document.querySelector("nav.psmob-nav-panel > ul").addEventListener("click",function(a){var b=a.target,c=typeof b;a=("object"==c&&null!=b||"function"==c)&&1==b.nodeType&&C(a.target,"psmob-menu-expand-icon")?a.target.parentNode:a.target;"DIV"==a.tagName&&(E(a.nextSibling,"open"),E(a.firstChild,"open"))})};function X(){this.f=0;this.e={};this.G=Date.now();this.o=!1;this.d=this.l=this.r=this.b=this.c=0;this.h=!1;this.m=this.j=0;this.g=new S(this);this.g.p["ps-progress-scrim"]=!0}function za(a){if(0==a.d){console.log("mobilizing site");var b=window.extractTheme;b&&!R()?(++a.j,b(a.e,a.F.bind(a))):Y(a)}else a.h=!0}
X.prototype.F=function(){--this.j;Z(this,this.c,"extract theme");if(window.psNavMode){var a=new va;console.log("Starting nav resynthesis.");a.i=da(A(document.querySelectorAll('[data-mobile-role="navigational"]')),A(document.querySelectorAll(".topNavList")));for(var b=document.getElementsByTagName("*"),c=0,d;d=b[c];c++){var e=window.getComputedStyle(d);if("fixed"==e.getPropertyValue("position")){var f=d.getBoundingClientRect().top;d.style.top=String(60+f)+"px"}999999<=e.getPropertyValue("z-index")&&
(console.log("Element z-index exceeded 999999, setting to 999998."),d.style.zIndex=999998)}c=document.createElement("div");document.body.insertBefore(c,document.body.childNodes[0]);D(c,"psmob-header-spacer-div");b=document.createElement("header");document.body.insertBefore(b,c);D(b,"psmob-header-bar");b.innerHTML=psHeaderBarHtml;b.style.borderBottom="thin solid "+psMenuFrontColor;(c=document.getElementsByClassName("psmob-logo-span")[0])&&b.appendChild(c);if(c=document.querySelector('[data-mobile-role="logo"]'))b.style.backgroundColor=
c.style.backgroundColor;b=a.A&&psMenuBackColor?psMenuBackColor:"#3c78d8";b=".psmob-header-bar { background-color: "+b+" }\n.psmob-nav-panel { background-color: "+(a.A&&psMenuFrontColor?psMenuFrontColor:"white")+" }\n.psmob-nav-panel > ul li { color: "+b+" }\n.psmob-nav-panel > ul li a { color: "+b+" }\n";c=document.createElement("style");c.type="text/css";c.appendChild(document.createTextNode(b));document.head.appendChild(c);if(0!=a.i.length&&!R()){b=document.getElementsByClassName("psmob-header-bar")[0];
b=document.body.insertBefore(document.createElement("nav"),b.nextSibling);D(b,"psmob-nav-panel");b=b.appendChild(document.createElement("ul"));D(b,"open");for(c=0;d=a.i[c];c++){d.setAttribute("data-mobilize-nav-section",c);e=W(a,d,0);f=[];f.push(b);for(var g=0,h=e.length;g<h;g++){var k=e[g].getAttribute("data-mobilize-nav-level"),p=g+1==h?k:e[g+1].getAttribute("data-mobilize-nav-level");if(k<p){var l=document.createElement("li"),k=l.appendChild(document.createElement("div")),p=k.appendChild(document.createElement("img"));
p.setAttribute("src",a.B);D(p,"psmob-menu-expand-icon");k.appendChild(document.createTextNode(e[g].textContent||e[g].innerText));f[f.length-1].appendChild(l);k=document.createElement("ul");l.appendChild(k);f.push(k)}else for(l=document.createElement("li"),f[f.length-1].appendChild(l),l.appendChild(e[g].cloneNode(!0)),l=k-p;0<l&&1<f.length;)f.pop(),l--}d.parentNode.removeChild(d)}d=document.querySelector(".psmob-nav-panel > ul a");e={};b=[];for(c=0;f=d[c];c++)f.href in e?(g=f.innerHTML.toLowerCase(),
-1==e[f.href].indexOf(g)?e[f.href].push(g):"LI"==f.parentNode.tagName&&b.push(f.parentNode)):(e[f.href]=[],e[f.href].push(f.innerHTML.toLowerCase()));for(c=0;d=b[c];c++)d.parentNode.removeChild(d);c=document.querySelectorAll(".psmob-nav-panel *");for(b=0;d=c[b];b++)d.removeAttribute("style"),d.removeAttribute("width"),d.removeAttribute("height"),"A"==d.tagName&&""==d.innerText&&d.hasAttribute("title")&&d.appendChild(document.createTextNode(d.getAttribute("title")));c=document.querySelectorAll(".psmob-nav-panel img:not(.psmob-menu-expand-icon)");
for(b=0;d=c[b];++b)e=Math.min(2*d.naturalHeight,40),d.setAttribute("height",e);xa(a);ya()}Z(this,this.c,"navigation")}Y(this)};X.prototype.C=function(){--this.d;Z(this,1E3,"background image");0==this.d&&this.h&&(za(this),this.h=!1)};function Aa(a,b){var c=T(a.g,b);if(null!=c){var d=Q(c);if(d&&(0==d.lastIndexOf("http://",0)||0==d.lastIndexOf("https://",0))&&!a.e[d]){var e=new Image;++a.d;e.onload=a.C.bind(a);e.onerror=e.onload;e.src=d;a.e[d]=e}for(c=c.firstChild;c;c=c.nextSibling)Aa(a,c)}}
X.prototype.xhrSendHook=function(){++this.f};X.prototype.xhrResponseHook=function(){--this.f;this.b+=this.m;Y(this)};function Y(a){if(0==a.f&&0==a.j&&0==a.d){var b=a.g;if(null!=document.body)for(var c=0;c<V.length;++c){V[c].bind(b)(document.body,0);++c;var d=b.k;Z(d,d.c,V[c])}if(a.o){if(a=document.getElementById("ps-progress-remove"))a.textContent="Remove Progress Bar and show mobilized site"}else Ba()}}
function Ca(a){$.o=a;var b=document.getElementById("ps-progress-log");b&&(b.style.color=a?"#333":"white");a&&(a=document.getElementById("ps-progress-show-log"))&&(a.style.display="none")}function Z(a,b,c){a.l+=b;b=100;0<a.b&&(b=Math.round(100*a.l/a.b),100<b&&(b=100));if(b!=a.r){var d=document.getElementById("ps-progress-span");d&&(d.style.width=b+"%");a.r=b}a=""+b+"% "+(Date.now()-a.G)+"ms: "+c;console.log(a);if(c=document.getElementById("ps-progress-log"))c.textContent+=a+"\n"}
function Ba(){var a=document.getElementById("ps-progress-scrim");a&&(a.style.display="none",a.parentNode.removeChild(a))}var $=new X;Ca(window.psDebugMode);$.c=oa(document.body);$.m=V.length/2*$.c;$.b+=$.m;window.extractTheme&&R()&&($.b+=$.c,window.psNavMode&&($.b+=$.c));null!=document.body&&Aa($,document.body);$.b+=1E3*$.d;window.pagespeedXhrHijackSetListener($);za($);q("psGetVisiblity",function(a){a.getAttribute("ps-save-visibility")||(a=window.getComputedStyle(a))&&a.getPropertyValue("visibility")});
q("psSetDebugMode",function(){Ca(!0)});q("psRemoveProgressBar",function(){Ba()});})();