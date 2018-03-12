#!/bin/sh

if [ ! -x ./build/restserver ]
then
    exit 1
fi

./build/restserver &
RESTSERVER_PID=$!

cd tests-rest && npm install && npm test

kill -2 $RESTSERVER_PID
sleep 1

