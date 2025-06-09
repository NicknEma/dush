#!/usr/bin/bash
clang src/dush.c -o dush -Wall -Wextra -pedantic -Wno-unused-function -Wno-initializer-overrides -g -O1
