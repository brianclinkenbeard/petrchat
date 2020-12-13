#!/bin/sh
name=${1:-brian}
./bin/petr_client "$name" 127.0.0.1 4000
