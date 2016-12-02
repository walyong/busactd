#!/bin/sh

set -e

opt="$1"

if [ -f .git/hooks/pre-commit.sample ] && [ ! -f .git/hooks/pre-commit ]; then
    # This part is allowed to fail
    cp -p .git/hooks/pre-commit.sample .git/hooks/pre-commit && \
        chmod +x .git/hooks/pre-commit && \
        echo "Activated pre-commit hook." || :
fi

# README and INSTALL are required by automake, but may be deleted by
# clean up rules. to get automake to work, simply touch these here,
# they will be regenerated from their corresponding *.in files by
# ./configure anyway.
touch README INSTALL

# make sure m4 dir exist
mkdir -p m4

autoreconf --force --install --verbose || exit $?

if [ "x$opt" = "xc" ]; then
    set -x
    ./configure $args
    make clean > /dev/null
elif [ "x$opt" = "xd" ]; then
    set -x
    ./configure CFLAGS='-g -O0 -ftrapv' $args
    make clean > /dev/null
elif [ "x$opt" = "xg" ]; then
    set -x
    ./configure CFLAGS='-g -Og -ftrapv' $args
    make clean > /dev/null
elif [ "x$opt" = "xl" ]; then
    set -x
    ./configure CC=clang CFLAGS='-g -O0 -ftrapv' $args
    make clean > /dev/null
elif [ "x$opt" = "xs" ]; then
    set -x
    scan-build ./configure CFLAGS='-std=gnu99 -g -O0 -ftrapv' $args
    scan-build make
fi
