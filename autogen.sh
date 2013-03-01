#!/bin/sh -e

autoreconf -ifv

if [ -z "${NOCONFIGURE}" ]; then
	./configure $@
fi
