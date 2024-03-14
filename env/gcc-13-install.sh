#!/bin/bash

########################################################################################
# GCC 12 and GCC 13 installer:
#     - checks out GCC sources from official GCC Git repo;
#     - builds them and installs GCC binaries to $HOME/opt/gcc-x.y.z location
#
# Installed languages are: C/C++, Fortran and Go
# 
# Prerequisites: Flex version 2.5.4 (or later) and tools & packages listed in
# https://gcc.gnu.org/install/prerequisites.html
# 
########################################################################################

GCC_MAJOR_VERSION=13

GCC_CHECKOUT_DIR_NAME=gcc-source

git clone https://gcc.gnu.org/git/gcc.git ./$GCC_CHECKOUT_DIR_NAME/
cd $GCC_CHECKOUT_DIR_NAME/

case $GCC_MAJOR_VERSION in
    12)
        ENABLED_LANGUAGES=c,c++,fortran,go
        git checkout remotes/origin/releases/gcc-12
        ;;
    13)
        ENABLED_LANGUAGES=c,c++,fortran,go
        git checkout remotes/origin/releases/gcc-13
        ;;
esac

./contrib/download_prerequisites

GCC_FULL_VERSION=$(head -n 1 ./gcc/BASE-VER)

echo "==================================================================================================="
echo "Installing GCC $GCC_MAJOR_VERSION ($GCC_FULL_VERSION) with enabled languages: $ENABLED_LANGUAGES..."
echo "==================================================================================================="

cd ..
mkdir ./gcc-$GCC_MAJOR_VERSION-build
cd gcc-$GCC_MAJOR_VERSION-build/

# Installing GCC into $HOME/opt/gcc-x.y.z is normally a good idea.
# To install it globally, installing into /usr/local/gcc-x.y.z is normally a good idea.

$PWD/../$GCC_CHECKOUT_DIR_NAME/configure --prefix=/usr/local --enable-languages=$ENABLED_LANGUAGES --disable-multilib --program-suffix=-$GCC_MAJOR_VERSION

# Next one might take up to 3.5 hours in case of GCC 13
make -j`nproc`

sudo make install