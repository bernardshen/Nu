#!/bin/bash

# build caladan
cd caladan
make -j`nproc`
cd ..

# build Nu
make -j`nproc`