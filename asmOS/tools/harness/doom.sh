#!/bin/sh
# doom.sh <builddir> <shots dir> : DOOM's attract demo with Frame Times on
AUTOSTART=doom "$(dirname "$0")/play.sh" $1 doom-demo $2 | grep "\[\|errors"
