# Ideally the system should only rewrite an image once when when it gets
# a burst of requests.  A bug was fixed where we were not obeying a
# failed lock and were rewriting it potentially many times.  It still
# happens fairly often that we rewrite the image twice.  I am not sure
# why that is, except to observe that our locks are 'best effort'.
start_test A burst of image requests should yield only one two rewrites.
URL="$EXAMPLE_ROOT/images/Puzzle.jpg?a=$RANDOM"
start_image_rewrites=$(scrape_stat image_rewrites)
echo Running burst of 20x: \"wget -q -O - $URL '|' wc -c\"
for ((i = 0; i < 20; ++i)); do
  echo -n $(wget -q -O - $URL | wc -c) ""
done
echo "... done"
sleep 1
num_image_rewrites=$(($(scrape_stat image_rewrites) - start_image_rewrites))
check [ $num_image_rewrites = 1 -o $num_image_rewrites = 2 ]
URL=""
