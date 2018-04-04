#!/bin/sh

set -x # echo on

gcc -o resample -Wall -g -O0 `pkg-config vorbisfile --cflags` resample.c `pkg-config vorbisfile --libs` -lm
