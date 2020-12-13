#!/bin/sh
make clean -s
make -s
./bin/petr_server 4000 audit.log
