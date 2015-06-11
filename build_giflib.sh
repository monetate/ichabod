#!/bin/bash

pushd giflib
mkdir m4
./autogen.sh
make
popd
