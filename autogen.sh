#!/bin/sh

autoreconf -ifv

if [ -z "${NOCONFIGURE}" ]; then
	./configure $@
fi
