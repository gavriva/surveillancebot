#!/bin/sh

#g++ -std=c++17 main.cpp -o survbot -lpthread `pkg-config --libs opencv`
clang++ -std=c++17 main.cpp -o survbot -Ilibs/spdlog/include -lpthread `pkg-config --libs opencv`
