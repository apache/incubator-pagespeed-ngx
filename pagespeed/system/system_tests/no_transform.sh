# When Cache-Control: no-transform is in the response make sure that
# the URL is not rewritten and that the no-transform header remains
# in the resource.
start_test HonorNoTransform cache-control: no-transform
WGET_ARGS="--header=X-PSA-Blocking-Rewrite:psatest"
FILE=no_transform/image.html
URL=$TEST_ROOT/$FILE
FETCHED=$OUTDIR/output
wget -O - $URL $WGET_ARGS > $FETCHED
# Make sure that the URL is not rewritten
check_not fgrep -q '.pagespeed.' $FETCHED
wget -O - -S $TEST_ROOT/no_transform/BikeCrashIcn.png $WGET_ARGS &> $FETCHED
# Make sure that the no-transfrom header is still there
check grep -q 'Cache-Control:.*no-transform' $FETCHED

# If DisableRewriteOnNoTransform is turned off, verify that the rewriting
# applies even if Cache-control: no-transform is set.
start_test rewrite on Cache-control: no-transform
URL=$TEST_ROOT/disable_no_transform/index.html?PageSpeedFilters=inline_css
fetch_until -save -recursive $URL 'grep -c style' 2

# TODO(jkarlin): Now that IPRO is implemented we should test that we obey
# no-transform in that path.
