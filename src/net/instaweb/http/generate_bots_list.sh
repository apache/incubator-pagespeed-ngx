#!/bin/bash
#
# Copyright 2010 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Author: fangfei@google.com (Fangfei Zhou)
#
# Run this script and copy&paste the output to the gperf table in
# net/instaweb/http/bot_checker.gperf

# The following names are missing from the database source web
# (http://www.robotstxt.org/db/) but were noticed in access logs.
extra_names=(Googlebot-Image bingbot Yahoo! about.ask.com
Baiduspider BackRub Gigabot OntoSpider Lycos YodaoBot YandexBot
bitlybot vcbot)

cd /tmp
wget -O all.genbot.$$ http://www.robotstxt.org/db/all.txt
awk '/robot-useragent:/ {print $2}' all.genbot.$$ >> s1.genbot.$$
# Get all user-agents matching "bot" or "spider", case insensitive.
awk '/[Bb][Oo][Tt]/ || /[Ss]pider/ {print $0}' s1.genbot.$$ >> s2.genbot.$$
# Remove the version part
awk -F / '{print $1}' s2.genbot.$$ >> id.genbot.$$
# Add some bots
for name in ${extra_names[@]}
do
  echo $name >> id.genbot.$$
done

sort -f id.genbot.$$ -o id.genbot.$$
# Print the strings for gperf format
echo "# Please copy and paste the following strings to \
/net/instaweb/http/bot_checker.gperf"
cat id.genbot.$$
# Print the strings for java HashSet format
echo "# Please copy and past the following strings to \
the corresponding java"
PRE_DELIMITER="\"(/| |^)(\" +"
POST_DELIMITER="\")( |\\\+|\\\.|/|$|;)\" "
echo $PRE_DELIMITER
cat id.genbot.$$ | while read LINE; do
    echo "\""$LINE"|\" +"
done
echo $POST_DELIMITER
rm *.genbot.$$


