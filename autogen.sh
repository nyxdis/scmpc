#!/bin/bash

aclocal
autoconf

./configure $@
