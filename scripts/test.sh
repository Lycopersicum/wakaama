#!/bin/sh

if [ ! -x ./build/restserver ]
then
    exit 1
fi

openssl genrsa -out private.key 2048
openssl req -days 365 -out certificate.pem -new -x509 -key private.key -subj '/CN=localhost'

openssl genrsa -out other_private.key 2048
openssl req -days 365 -out other_certificate.pem -new -x509 -key other_private.key -subj '/CN=localhost'

GCOV_PREFIX_STRIP=5 GCOV_PREFIX=regular ./build/restserver -l 5 &
REGULAR_RESTSERVER_PID=$!

sleep 2

GCOV_PREFIX_STRIP=5 GCOV_PREFIX=secure ./build/restserver -c ./tests-rest/secure.cfg &
SECURE_RESTSERVER_PID=$!

cd tests-rest && npm install && npm test
TEST_STATUS=$?

kill -2 $REGULAR_RESTSERVER_PID && echo "Regular restserver killed ($REGULAR_RESTSERVER_PID)!"
kill -2 $SECURE_RESTSERVER_PID && echo "Secure restserver killed ($SECURE_RESTSERVER_PID)!"
sleep 1

find ./ -name '*.gcda'

exit $TEST_STATUS
