<?php

header("Connection: close\r\n");
header("Cache-Control: max-age=86400");
header("Pragma: ", true);
header("Content-Type: text/css");
header("Expires: " . gmdate("D, d M Y H:i:s", time() + 86400) . " GMT");

$data = ".peachpuff {background-color: peachpuff;}" .
  "\n" .
  "." . str_pad("", 10000, uniqid()) . " {background-color: antiquewhite;}\n";

$output = "\x1f\x8b\x08\x00\x00\x00\x00\x00" .
    substr(gzcompress($data, 2), 0, -4) .
    pack('V', crc32($data)) .
    pack('V', mb_strlen($data, "latin1"));

header("Content-Encoding: gzip\r\n");
header("Content-Length: " . mb_strlen($output, "latin1"));

echo $output;
