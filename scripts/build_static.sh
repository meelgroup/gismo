#!/bin/bash

set -e

rm -rf lib* Test* tests* include tests CM* cmake* gismo
cmake -DSTATICCOMPILE=ON ..
make -j6
make test
strip gismo
