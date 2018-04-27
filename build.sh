#!/bin/sh

clang++-5.0 -std=c++17 test.cpp -lpthread -rdynamic -ftemplate-depth=256
#g++ -std=c++17 test.cpp -lpthread -rdynamic -ftemplate-depth=256


#clang++-5.0 -std=c++17 test.cpp -lpthread -O1

