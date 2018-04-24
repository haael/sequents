#!/bin/sh

clang++-5.0 -std=c++17 test.cpp -lpthread -rdynamic -ftemplate-depth=256

