<?php
header('Content-Type: text/html');

echo "<html><head></head><body>\n";
$size = 5;

for($i = 1; $i <= $size; $i++) {
  echo "<p>foo</p><div>bar:" . $i  . "</div>\n";
  ob_flush();
  flush();
  sleep(1);
}

echo "</body></html>\n";
?>