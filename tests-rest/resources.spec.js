
const chai = require('chai');
const chai_http = require('chai-http');
const should = chai.should();
const events = require('events');
var server = require('./server-if');
var ClientInterface = require('./client-if');

chai.use(chai_http);

describe('Resources interface', function () {
  const client = new ClientInterface();

  before(function (done) {
    var self = this;

    server.start();

    self.events = new events.EventEmitter();
    // TODO: swap interval with long-poll once server supports it
    self.interval = setInterval(function () {
      chai.request(server)
        .get('/notification/pull')
        .end(function (err, res) {
          const responses = res.body['async-responses'];
          if (!responses)
            return;

          for (var i=0; i<responses.length; i++) {
            self.events.emit('async-response', responses[i]);
          }
        });
    }, 1000);

    client.connect(server.address(), (err, res) => {
      done();
    });
  });

  after(function () {
    clearInterval(this.interval);
    client.disconnect();
  });

  describe('GET /endpoints/{endpoint-name}/{resource-path}', function () {

    it('should return async-response-id and 202 code', function(done) {
      const id_regex = /^\d+#[0-9a-z]{8}-[0-9a-z]{4}-[0-9a-z]{4}-[0-9a-z]{4}-[0-9a-z]{4}$/g;
      chai.request(server)
        .get('/endpoints/'+client.name+'/3/0/0')
        .end(function (err, res) {
          should.not.exist(err);
          res.should.have.status(202);
          res.should.have.header('content-type', 'application/json');

          res.body.should.be.a('object');
          res.body.should.have.property('async-response-id');
          res.body['async-response-id'].should.be.a('string');
          res.body['async-response-id'].should.match(id_regex);

          done();
        });
    });

    it('should return 404 for invalid resource-path', function (done) {
      chai.request(server)
        .get('/endpoints/'+client.name+'/some/invalid/path')
        .end(function (err, res) {
          res.should.have.status(404);
          done();
        });
    });

    it('should return 410 for non-existing endpoint', function (done) {
      chai.request(server)
        .get('/endpoints/non-existing-ep/3/0/0')
        .end(function (err, res) {
          res.should.have.status(410);
          done();
        });
    });

    it('response should return 200 and valid payload', function (done) {
      var self = this;

      chai.request(server)
        .get('/endpoints/'+client.name+'/3/0/0')
        .end(function (err, res) {
          should.not.exist(err);
          res.should.have.status(202);

          const id = res.body['async-response-id'];
          self.events.on('async-response', resp => {
            if (resp.id == id) {
              resp.status.should.be.eql(200);
              resp.payload.should.be.eql('0AAIOGRldmljZXM='); // '8devices' TLV
              done();
            }
          });
        });
    });

    it('response should return 404 for invalid resource-path', function (done) {
      var self = this;

      chai.request(server)
        .get('/endpoints/'+client.name+'/123/456/789')
        .end(function (err, res) {
          should.not.exist(err);
          res.should.have.status(202);

          const id = res.body['async-response-id'];
          self.events.on('async-response', resp => {
            if (resp.id == id) {
              resp.status.should.be.eql(404);
              done();
            }
          });
        });
    });

  });

  describe('POST /endpoints/{endpoint-name}/{resource}', function () {
    it('should return async-response-id and 202 code', function(done) {
      const id_regex = /^\d+#[0-9a-z]{8}-[0-9a-z]{4}-[0-9a-z]{4}-[0-9a-z]{4}-[0-9a-z]{4}$/g;
      chai.request(server)
        .post('/endpoints/'+client.name+'/1/0/8')
        .end(function (err, res) {
          should.not.exist(err);
          res.should.have.status(202);
          res.should.have.header('content-type', 'application/json');

          res.body.should.be.a('object');
          res.body.should.have.property('async-response-id');
          res.body['async-response-id'].should.be.a('string');
          res.body['async-response-id'].should.match(id_regex);

          done();
        });
    });

    it('should return 404 for invalid resource-path', function (done) {
      chai.request(server)
        .post('/endpoints/'+client.name+'/some/invalid/path')
        .end(function (err, res) {
          res.should.have.status(404);
          done();
        });
    });

    it('should return 410 for non-existing endpoint', function (done) {
      chai.request(server)
        .post('/endpoints/non-existing-ep/1/0/8')
        .end(function (err, res) {
          res.should.have.status(410);
          done();
        });
    });

    it('response should return 200 and empty payload', function (done) {
      var self = this;

      chai.request(server)
        .post('/endpoints/'+client.name+'/1/0/8')
        .end(function (err, res) {
          should.not.exist(err);
          res.should.have.status(202);

          const id = res.body['async-response-id'];
          self.events.on('async-response', resp => {
            if (resp.id == id) {
              resp.status.should.be.eql(200);
              console.log("Response payload of /1/0/8: ",resp.payload);
              done();
            }
          });
        });
    });

    it('response should return 404 for invalid resource-path', function (done) {
      var self = this;

      chai.request(server)
        .post('/endpoints/'+client.name+'/123/456/789')
        .end(function (err, res) {
          should.not.exist(err);
          res.should.have.status(202);

          const id = res.body['async-response-id'];
          self.events.on('async-response', resp => {
            if (resp.id == id) {
              resp.status.should.be.eql(404);
              done();
            }
          });
        });
    });
  });

});
