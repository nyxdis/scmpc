#!/bin/bash

##
# This script is more than likely the spawn of Satan, and pretty much a bad
# idea. On the other hand, it works for me, and no-one else should need to use
# it.
#

prefix=$1
shift;

while [ -n "$1" -a "x$1" != "x" ]; do
	echo -n "\$(${prefix})/"
	gcc -MM $1 | sed -e 's: [^ ]\+/: :g'
	shift;
done
