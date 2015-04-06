#!/bin/bash
# This cgi script just sleeps for a while then returns an image.  It's meant to
# simulate a server environment where some resources are dynamically generated
# by a process which is subject to delay (e.g. mysql, php).
sleep 10;
echo Content-type: image/jpeg
echo
cat ../images/Puzzle.jpg
