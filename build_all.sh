#!/bin/bash

function not_supported {
    echo 'Please set env var $NODE_TYPE, 
supported list: [r650, r6525, c6525, d6515, xl170, xl170_uswitch, yinyang, zg]'
    exit 1
}

# Check node type.
if [[ ! -v NODE_TYPE ]]; then
    not_supported
fi

#sudo apt update
#sudo apt install python3-pip libboost-all-dev -y
#pip3 install --user meson
#export PATH=$HOME/.local/bin:$PATH

# Apply patches.
patch_file=caladan/build/$NODE_TYPE.patch

if [ ! -f $patch_file ]; then
    not_supported
fi

patch -p1 -d caladan/ < $patch_file

# Build caladan.
cd caladan
./build.sh
cd ..

# Build Nu.
make clean
make -j`nproc`

# Setup Nu.
# ./setup.sh

