#!/bin/bash

autoreconf -ifv

if [ -z "${NOCONFIGURE}" ]; then
	./configure $@
fi
