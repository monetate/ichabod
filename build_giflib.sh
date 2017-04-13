#!/bin/bash

pushd code
mkdir m4
./autogen.sh
make
popd
