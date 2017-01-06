#!/bin/sh

# When setting up a new modpagespeed.com instance, this script goes in
# /var/www/scripts/install_examples.sh

cd $(dirname $(dirname $0))/html || exit 2

if [ $# -ne 1 ]; then
  echo usage: $0 release >&2
  exit 1
fi
RELEASE=$1

RELDIR=release_archive/$RELEASE
TARBALL=$RELDIR/mod_pagespeed_examples.tar.gz

if [ ! -d $RELDIR ]; then
  echo Please create the release directory: $(pwd)/$RELDIR >&2
  exit 2
fi

if [ -f $TARBALL ]; then
  S="!!!"
  echo $S The tarball already exists. I presume it has already been installed
  echo $S so I am going to stop here and make you check manually. This is it:
  echo $S $(ls $(pwd)/$TARBALL)
  exit 0
fi

if [ ! -f $HOME/mod_pagespeed_examples.tar.gz ]; then
  echo "Please scp mod_pagespeed_examples.tar.gz to your home directory" >&2
  exit 3
fi
mv $HOME/mod_pagespeed_examples.tar.gz $TARBALL

# Test the tarball before blowing everything away!
tar tzf $TARBALL >/dev/null || exit 4

# Put up the "Men At Work" sign.
rm -f index.html
cp maintenance.html index.html

# Remove symlinks into the old ./mod_pagespeed_example/ then remove it.
for i in $(find . -type l -print); do
  ls -l $i | grep -qe '-> mod_pagespeed_example/'
  if [ $? -eq 0 ]; then
    rm -f $i
  fi
done
rm -rf mod_pagespeed_example

# Extract the new ./mod_pagespeed_example/ then clean up.
tar xzf $TARBALL
rm  -f system_test.sh apache_system_test.sh # cruft
rm -rf mod_pagespeed_test                   # cruft
chown -R root mod_pagespeed_example 
find mod_pagespeed_example -type d -exec chmod 755 {} \;
find mod_pagespeed_example -type f -exec chmod 644 {} \;

# Create symlinks into the new ./mod_pagespeed_example.
status=0
for i in $(ls mod_pagespeed_example); do
  # Since we copied maintenance.html over it above.
  if [ $i = index.html ]; then
    rm -f index.html
  fi
  if [ -f $i ]; then
    echo "Skipping $i as it already exists!" >&2
    status=3
  else
    ln -s mod_pagespeed_example/$i .
  fi
done

exit $status
