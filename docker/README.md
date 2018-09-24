# Supported tags and respective `Dockerfile` links

- [`1.13.35.2`, `stable`, `1.13.35`, `1.13`, `1.13.35.2-alpine3.8`, `1.13.35-alpine3.8`, `1.13-alpine3.8`, `stable-alpine3.8`, `1.13.35.2-alpine3.8-ngx1.14`, `1.13.35-alpine3.8-ngx1.14`, `1.13-alpine3.8-ngx1.14`, `stable-alpine3.8-ngx1.14`, `latest` (*alpine-3.8/nginx-stable/Dockerfile*)](https://github.com/apache/incubator-pagespeed-ngx/blob/master/docker/alpine-3.8/nginx-stable/Dockerfile)
- [`1.13.35.2-edge`, `stable-edge`, `edge`, `1.13.35-edge`, `1.13-edge`, `1.13.35.2-edge-ngx1.14`, `1.13.35-edge-ngx1.14`, `1.13-edge-ngx1.14`, `stable-edge-ngx1.14` (*alpine-edge/nginx-stable/Dockerfile*)](https://github.com/apache/incubator-pagespeed-ngx/blob/master/docker/alpine-edge/nginx-stable/Dockerfile)
- [`1.13.35.2-alpine3.8-ngx1.15`, `ngx1.15`, `1.13.35-alpine3.8-ngx1.15`, `1.13-alpine3.8-ngx1.15`, `stable-alpine3.8-ngx1.15` (*alpine-3.8/nginx-mainline/Dockerfile*)](https://github.com/apache/incubator-pagespeed-ngx/blob//master/docker/alpine-3.8/nginx-mainline/Dockerfile)
- [`1.13.35.2-edge-ngx1.15`, `1.13.35-edge-ngx1.15`, `1.13-edge-ngx1.15`, `stable-edge-ngx1.15` (*alpine-edge/nginx-mainline/Dockerfile*)](https://github.com/apache/incubator-pagespeed-ngx/blob/master/docker/alpine-edge/nginx-mainline/Dockerfile)

# Quick reference

-	**Where to get help**:
  [Read the wiki](https://github.com/apache/incubator-pagespeed-mod/wiki), [Ask a question on the mailing list](https://groups.google.com/group/ngx-pagespeed-discuss)  

- **Docker image repository**:
  [Dockerhub](https://hub.docker.com/r/pagespeed/nginx-pagespeed)

- **Git Dockerfile repository**:
  [Github](https://github.com/apache/incubator-pagespeed-ngx/tree/master/docker)

-	**Where to file issues**:
	[https://github.com/We-Amp/ngx-pagespeed-alpine/issues](https://github.com/apache/incubator-pagespeed-ngx/issues)

-	**Docker images maintained by**:
	[Nico Berlee](mailto:nico.berlee@on2it.net)

-	**Supported Docker versions**:
	[the latest release](https://github.com/docker/docker-ce/releases/latest) (down to 1.12 on a best-effort basis)

# What is pagespeed?

The PageSpeed Modules, [mod_pagespeed](https://github.com/apache/incubator-pagespeed-mod) and [ngx_pagespeed](https://github.com/apache/incubator-pagespeed-ngx), are open-source webserver modules that [optimize your site automatically](https://www.modpagespeed.com/doc/filters).

ngx_pagespeed speeds up your site and reduces page load time by automatically
applying web performance best practices to pages and associated assets (CSS,
JavaScript, images) without requiring you to modify your existing content or
workflow. Features include:

- Image optimization: stripping meta-data, dynamic resizing, recompression
- CSS & JavaScript minification, concatenation, inlining, and outlining
- Small resource inlining
- Deferring image and JavaScript loading
- HTML rewriting
- Cache lifetime extension
- and [more](https://developers.google.com/speed/docs/mod_pagespeed/config_filters)

To see ngx_pagespeed in action, with example pages for each of the
optimizations, see our [demonstration site](http://ngxpagespeed.com).

![logo](https://camo.githubusercontent.com/4138679c6cf85adb18c4cf820189c898f7dbf5cb/68747470733a2f2f6c68362e676f6f676c6575736572636f6e74656e742e636f6d2f2d71756665644a494a7137592f55584576565978795976492f4141414141414141446f382f4a48444651687339315f632f733430312f30345f6e67785f7061676573706565642e706e67)

# What is nginx?

Nginx (pronounced "engine-x") is an open source reverse proxy server for HTTP, HTTPS, SMTP, POP3, and IMAP protocols, as well as a load balancer, HTTP cache, and a web server (origin server). The nginx project started with a strong focus on high concurrency, high performance and low memory usage.
> [wikipedia.org/wiki/Nginx](https://en.wikipedia.org/wiki/Nginx)

# What is nginx-pagespeed?

The nginx-pagespeed brings all the goods of nginx and pagespeed together in one single small alpine docker image. Nginx-pagespeed aims to be 100% compatible with the plain [nginx](https://hub.docker.com/_/nginx/) images. Meaning, nginx-pagespeed can be a safe drop-in replacement for any container running `nginx:alpine`.

Nginx-pagespeed makes it easy to start optimizing your website by reducing page load time, without requiring you to modify existing content.

# How to use this image

## Hosting some simple static content

```console
$ docker run --name pagespeed-nginx -v /some/content:/usr/share/nginx/html:ro -d pagespeed/nginx-pagespeed
```

Alternatively, a simple `Dockerfile` can be used to generate a new image that includes the necessary content (which is a much cleaner solution than the bind mount above):

```dockerfile
FROM pagespeed/nginx-pagespeed
COPY static-html-directory /usr/share/nginx/html
```

Place this file in the same directory as your directory of content ("static-html-directory"), run `docker build -t some-content-ngxpagespeed .`, then start your container:

```console
$ docker run --name my-nginx-pagespeed -d some-content-ngxpagespeed
```

## Exposing external port

```console
$ docker run --name my-nginx-pagespeed -d -p 8080:80 some-content-ngxpagespeed
```

Then you can hit `http://localhost:8080` or `http://host-ip:8080` in your browser.

## Complex configuration

```console
$ docker run --name my-custom-nginx-pagespeed -v /host/path/nginx.conf:/etc/nginx/nginx.conf:ro -d pagespeed/nginx-pagespeed
```

For information on the syntax of the nginx configuration files, see [the official documentation](http://nginx.org/en/docs/) (specifically the [Beginner's Guide](http://nginx.org/en/docs/beginners_guide.html#conf_structure)). For pagespeed specific nginx config syntax, see [Beginner's guide](https://www.modpagespeed.com/doc/configuration) or a complete overview of [all pagespeed filters](https://www.ngxpagespeed.com/).
For a quick start on pagespeed specific configuration see []

If you wish to adapt the default configuration, use something like the following to copy it from a running nginx-pagespeed container:

```console
$ docker run --name tmp-ngxpagespeed-container -d pagespeed/nginx-pagespeed
$ docker cp tmp-ngxpagespeed-container:/etc/nginx/nginx.conf /host/path/nginx.conf
$ docker rm -f tmp-ngxpagespeed-container
```

This can also be accomplished more cleanly using a simple `Dockerfile` (in `/host/path/`):

```dockerfile
FROM pagespeed/nginx-pagespeed
COPY nginx.conf /etc/nginx/nginx.conf
```

If you add a custom `CMD` in the Dockerfile, be sure to include `-g daemon off;` in the `CMD` in order for nginx to stay in the foreground, so that Docker can track the process properly (otherwise your container will stop immediately after starting)!

Then build the image with `docker build -t custom-ngxpagespeed .` and run it as follows:

```console
$ docker run --name my-custom-ngxpagespeed-container -d custom-ngxpagespeed
```

### Using environment variables in nginx configuration

Out-of-the-box, nginx doesn't support environment variables inside most configuration blocks. But `envsubst` may be used as a workaround if you need to generate your nginx configuration dynamically before nginx starts.

Here is an example using docker-compose.yml:

```yaml
web:
  image: pagespeed/nginx-pagespeed
  volumes:
   - ./mysite.template:/etc/nginx/conf.d/mysite.template
  ports:
   - "8080:80"
  environment:
   - NGINX_HOST=foobar.com
   - NGINX_PORT=80
  command: /bin/bash -c "envsubst < /etc/nginx/conf.d/mysite.template > /etc/nginx/conf.d/default.conf && nginx -g 'daemon off;'"
```

The `mysite.template` file may then contain variable references like this:

`listen       ${NGINX_PORT};
`

# Image Variants

The `pagespeed/nginx-pagespeed` images come in many flavors, each designed for a specific use case.

## `pagespeed/nginx-pagespeed:<version>`

This is the defacto image. If you are unsure about what your needs are, you probably want to use this one. It is designed to be used both as a throw away container (mount your source code and start the container to start your app), as well as the base to build other images off of.

## `nginx:edge`

This image has the most up-to-date system packages available in the [Alpine Linux project](http://alpinelinux.org). This means the latest LibreSSL and musl-libc, with the downside of having less tested system packages.


## Using the Dockerfile
### Use docker build command to build an image from dockerfile:
  docker build -t <image_tag>  <dockerfile_path> .
    $ docker build -t ngxpagespeed-alpine38-ngxstable stable/3.8/nginx-stable
  Refer [this](https://docs.docker.com/engine/reference/commandline/build/) for additional options.

### Run this container as an independent service:
    $ docker run -d -p 80:80 <image_tag>
  Refer [this](https://docs.docker.com/engine/reference/run/) for additional options.

## Requirements for building:
- > 3 GB of free diskspace
- 2GB of memory
- an x86_64 compatible processor
- Docker CE > 17.3.2


# Disclaimer
Apache PageSpeed is an effort undergoing incubation at The Apache Software Foundation (ASF), sponsored by the Apache Incubator. Incubation is required of all newly accepted projects until a further review indicates that the infrastructure, communications, and decision making process have stabilized in a manner consistent with other successful ASF projects. While incubation status is not necessarily a reflection of the completeness or stability of the code, it does indicate that the project has yet to be fully endorsed by the ASF.

# License
View [PageSpeed license information](https://github.com/apache/incubator-pagespeed-ngx/blob/master/LICENSE)
View [Nginx license information](http://nginx.org/LICENSE)

As with all Docker images, these likely also contain other software which may be under other licenses (such as Bash, etc from the base distribution, along with any direct or indirect dependencies of the primary software being contained).

Some additional license information which was able to be auto-detected might be found in [the `repo-info` repository's `nginx/` directory](https://github.com/docker-library/repo-info/tree/master/repos/nginx).

As for any pre-built image usage, it is the image user's responsibility to ensure that any use of this image complies with any relevant licenses for all software contained within.
