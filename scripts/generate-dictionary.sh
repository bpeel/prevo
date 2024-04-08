#!/bin/bash

set -eu

cd "$(dirname "$0")"/..

mkdir -p prevodb-build
cd prevodb-build

../extern/prevodb/autogen.sh
make -j$(nproc)

./src/prevodb \
    -i ../extern/revo-fonto \
    -i ../extern/voko-grundo \
    -o ../app/src/main
