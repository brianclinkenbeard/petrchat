#!/bin/sh
make clean -s
make -s
./bin/petr_server -p 4000
