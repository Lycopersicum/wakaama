const chai = require('chai');
const chai_http = require('chai-http');
const fs = require('fs');
const https = require('https');

const should = chai.should();
chai.use(chai_http);

const version_regex = /^1\.\d+\.\d+$/

describe('Secure connection', function () {
  before(function (done) {
    done();
  });

  after(function (done) {
    done();
  });

  describe('GET /version', function() {
    it('should return 200 and correct version', function(done) {
      const options = {
        host: 'localhost',
        port: '8889',
        path: '/version',
        ca: [
          fs.readFileSync('../certificate.pem'),
        ],
      };

      options.agent = new https.Agent(options);

      https.request(options, (response) => {
        let data = '';
        response.statusCode.should.be.equal(200);

        response.on('data', (chunk) => {
          data = data + chunk;
        });

        response.on('end', () => {
          data.should.match(version_regex);
          done();
        });
      }).end();
    });

    it('should return DEPTH_ZERO_SELF_SIGNED_CERT error code', function(done) {
      const options = {
        host: 'localhost',
        port: '8889',
        path: '/version',
        ca: [
          fs.readFileSync('../other_certificate.pem'),
        ],
      };

      options.agent = new https.Agent(options);

      const testRequest = https.request(options, (response) => {});

      testRequest.on('error', (err) => {
        err.code.should.be.equal('DEPTH_ZERO_SELF_SIGNED_CERT');

        done();
      });

      testRequest.end();
    });
  });

  describe('POST /authenticate', function() {
    it('should return 202 and object with valid jwt and method', function(done) {
      const credentials = '{"name": "admin", "secret": "not-same-as-name"}';

      const options = {
        host: 'localhost',
        port: '8889',
        path: '/authenticate',
        ca: [
          fs.readFileSync('../certificate.pem'),
        ],
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
      };

      options.agent = new https.Agent(options);

      const testRequest = https.request(options, (response) => {
        let data = '';
        response.statusCode.should.be.equal(202);
        response.should.have.header('content-type', 'application/json');

        response.on('data', (chunk) => {
          data = data + chunk;
        });

        response.on('end', () => {
          let headerBuffer, bodyBuffer, headerString, bodyString, jwt;
          const parsedBody = JSON.parse(data);

          parsedBody.should.be.a('object');

          parsedBody.should.have.property('jwt');
          parsedBody.should.have.property('method');

          parsedBody['jwt'].should.be.a('string');
          parsedBody['method'].should.be.a('string');

          parsedBody['method'].should.be.eql('header');

          headerBuffer = new Buffer(parsedBody['jwt'].split('.')[0], 'base64');
          bodyBuffer = new Buffer(parsedBody['jwt'].split('.')[1], 'base64');

          headerString = headerBuffer.toString();
          bodyString = bodyBuffer.toString();

          jwt = {
            header: JSON.parse(headerString),
            body: JSON.parse(bodyString),
            signature: parsedBody['jwt'].split('.')[2],
          };

          jwt.header.should.be.a('object');
          jwt.body.should.be.a('object');

          jwt.header.should.have.property('typ');
          jwt.header.should.have.property('alg');

          jwt.body.should.have.property('iat');
          jwt.body.should.have.property('name');

          done();
        });
      });

      testRequest.write(credentials);
      testRequest.end();
    });

    it('should return 401 and \'WWW-Authenticate\' header with error description', function(done) {
      const credentials = '{"name": "unexisting-user", "secret": "probably-incorrect"}';

      const options = {
        host: 'localhost',
        port: '8889',
        path: '/authenticate',
        ca: [
          fs.readFileSync('../certificate.pem'),
        ],
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
      };

      options.agent = new https.Agent(options);

      const testRequest = https.request(options, (response) => {
        response.statusCode.should.be.equal(401);
        response.should.have.header('WWW-Authenticate', 'User name or secret is invalid');

        done();
      });

      testRequest.write(credentials);
      testRequest.end();
    });

    it('should return 401 and \'WWW-Authenticate\' header with error description', function(done) {
      const options = {
        host: 'localhost',
        port: '8889',
        path: '/authenticate',
        ca: [
          fs.readFileSync('../certificate.pem'),
        ],
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
      };

      options.agent = new https.Agent(options);

      const testRequest = https.request(options, (response) => {
        response.statusCode.should.be.equal(401);
        response.should.have.header('WWW-Authenticate', 'Invalid authentication request format');

        done();
      });

      testRequest.end();
    });
  });

  describe('GET /endpoints', function() {
    it('should return 200 and endpoints list', function(done) {
      const credentials = '{"name": "admin", "secret": "not-same-as-name"}';

      const options = {
        host: 'localhost',
        port: '8889',
        ca: [
          fs.readFileSync('../certificate.pem'),
        ],
      };

      options.path = '/authenticate';
      options.method = 'POST';
      options.agent = new https.Agent(options);
      options.headers = {
        'Content-Type': 'application/json',
      };

      const authenticationRequest = https.request(options, (response) => {
        let data = '';
        response.statusCode.should.be.equal(202);
        response.should.have.header('content-type', 'application/json');

        response.on('data', (chunk) => {
          data = data + chunk;
        });

        response.on('end', () => {
          const parsedBody = JSON.parse(data);

          options.path = '/endpoints';
          options.method = 'GET';
          options.headers = {
            'Authorization': parsedBody['jwt'],
          };

          https.request(options, (response) => {
            let data = '';
            response.statusCode.should.be.equal(200);
            response.should.have.header('content-type', 'application/json');

            response.on('data', (chunk) => {
              data = data + chunk;
            });

            response.on('end', () => {
              const parsedBody = JSON.parse(data);

              parsedBody.should.be.a('array');

              done();
            });
          }).end();
        });
      });

      authenticationRequest.write(credentials);
      authenticationRequest.end();
    });

    it('should return 401 and \'WWW-Authenticate\' header with error description', function(done) {
      const credentials = '{"name": "put-all", "secret": "restricted-user"}';

      const options = {
        host: 'localhost',
        port: '8889',
        ca: [
          fs.readFileSync('../certificate.pem'),
        ],
      };

      options.agent = new https.Agent(options);

      options.path = '/authenticate';
      options.method = 'POST';
      options.headers = {
        'Content-Type': 'application/json',
      };

      const authenticationRequest = https.request(options, (response) => {
        let data = '';
        response.statusCode.should.be.equal(202);
        response.should.have.header('content-type', 'application/json');

        response.on('data', (chunk) => {
          data = data + chunk;
        });

        response.on('end', () => {
          const parsedBody = JSON.parse(data);

          options.path = '/endpoints';
          options.method = 'GET';
          options.headers = {
            'Authorization': parsedBody['jwt'],
          };

          https.request(options, (response) => {
            response.statusCode.should.be.equal(401);
            response.should.have.header('WWW-Authenticate', 'User doesn\'t have required permissions');

            done();
          }).end();
        });
      });

      authenticationRequest.write(credentials);
      authenticationRequest.end();
    });

    it('should return 401 and \'WWW-Authenticate\' header with error description', function(done) {
      const options = {
        host: 'localhost',
        port: '8889',
        ca: [
          fs.readFileSync('../certificate.pem'),
        ],
      };

      options.agent = new https.Agent(options);
      options.path = '/endpoints';
      options.headers = {
        'Authorization': 'invalid.token.specified',
      };

      https.request(options, (response) => {
        response.statusCode.should.be.equal(401);
        response.should.have.header('WWW-Authenticate', 'Invalid token specified');

        done();
      }).end();
    });

    it('should return 401 and \'WWW-Authenticate\' header with error description', function(done) {
      const options = {
        host: 'localhost',
        port: '8889',
        ca: [
          fs.readFileSync('../certificate.pem'),
        ],
      };

      // Token created at https://jwt.io/ (settings from secure.cfg)
      const expiredToken = 'eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzUxMiJ9.eyJpYXQiOjAsIm5hbWUiOiJhZG1pbiJ9.cju-l56DmXM3CkS9lhWt-ik-XcV7pEtjQXIymWZiNxOde2xrUpwJXn-sVOI7vKcUwhXLUWpv2WqQ3wkNqmv2Yg';

      options.agent = new https.Agent(options);
      options.path = '/endpoints';
      options.headers = {
        'Authorization': expiredToken,
      };

      https.request(options, (response) => {
        response.statusCode.should.be.equal(401);
        response.should.have.header('WWW-Authenticate', 'Token is expired');

        done();
      }).end();
    });

    it('should return 415', function(done) {
      const options = {
        host: 'localhost',
        port: '8889',
        ca: [
          fs.readFileSync('../certificate.pem'),
        ],
      };

      options.agent = new https.Agent(options);
      options.path = '/endpoints';

      https.request(options, (response) => {
        response.statusCode.should.be.equal(415);

        done();
      }).end();
    });
  });
});
