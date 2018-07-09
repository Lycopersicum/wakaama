#!/bin/sh

git clone https://github.com/benmcollins/libjwt
autoreconf -i
./configure
make
sudo make install
