# Check all the pagespeed_admin pages, both in its default location and an
# alternate.
start_test pagespeed_admin and alternate_admin_path
function check_admin_banner() {
  path="$1"
  title="$2"
  tmpfile=$TESTTMP/admin.html
  echo $WGET_DUMP $PRIMARY_SERVER/$path '>' $tmpfile ...
  $WGET_DUMP $PRIMARY_SERVER/$path > $tmpfile
  check fgrep -q "<title>PageSpeed $title</title>" $tmpfile
  rm -f $tmpfile
}
for admin_path in pagespeed_admin pagespeed_global_admin alt/admin/path; do
  check_admin_banner $admin_path/statistics "Statistics"
  check_admin_banner $admin_path/config "Configuration"
  check_admin_banner $admin_path/histograms "Histograms"
  check_admin_banner $admin_path/cache "Caches"
  check_admin_banner $admin_path/console "Console"
  check_admin_banner $admin_path/message_history "Message History"
done

start_test pagespeed_admin quoting on not-found page
OUT=$($CURL $PRIMARY_SERVER/'pagespeed_admin/a/<boo>')
check_not_from "$OUT" fgrep -q "<boo>"
check_from "$OUT" fgrep -q "%3Cboo%3E"

# Note that query params are stripped completely
OUT=$($CURL $PRIMARY_SERVER/'pagespeed_admin/a?<boo>')
check_not_from "$OUT" fgrep -q "boo"
