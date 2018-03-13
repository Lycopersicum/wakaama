#!/bin/sh

if [ ! -x ./build/restserver ]
then
    exit 1
fi

./build/restserver &
RESTSERVER_PID=$!
echo $RESTSERVER_PID
curl http://localhost:8888/endpoints

cd tests-rest && npm install && npm test
TEST_STATUS=$?

kill -2 $RESTSERVER_PID
sleep 1

exit $TEST_STATUS
