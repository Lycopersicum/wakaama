#!/bin/sh

git clone https://github.com/benmcollins/libjwt
cd libjwt
autoreconf -i
./configure
make
sudo make install
