**Introduction**
----
  REST API contains easy to use interface to the LwM2M server and client communication.
  
  Detailed [REST API documentation](./RESTAPI.md).

**Building**
----
1. Install tools and libraries required for project building for Debian based distributions (Debian, Ubuntu):
```
$ sudo apt-get install -y git cmake build-essential
$ sudo apt-get install -y libmicrohttpd-dev libjansson-dev libcurl4-gnutls-dev
```
2. Install required libraries from Github:
```
$ git clone https://github.com/babelouest/ulfius.git
$ cd ulfius/
$ git submodule update --init
$ cd lib/orcania
$ make && sudo make install
$ cd ../yder
$ make && sudo make install
$ cd ../..
$ make
$ sudo make install
$ cd ..
$ git clone https://github.com/benmcollins/libjwt
$ autoreconf -i
$ ./configure
$ make
$ sudo make install

```
3. Build LwM2M-REST server
```
$ git clone https://github.com/8devices/wakaama.git
$ cd wakaama/
$ mkdir build
$ cd build/
$ cmake ../examples/rest-server
$ make
```
After third step you should have binary file called `restserver` in your `wakaama/build/` directory.

**Usage**
----
You can get some details about `restserver` by using `--help` or `-?` argument:
```
wakaama/build $ ./restserver --usage
Usage: restserver [OPTION...]
Restserver - interface to LwM2M server and all clients connected to it

  -c, --config=FILE          Specify parameters configuration file
  -l, --log=LOGGING_LEVEL    Specify logging level (0-5)
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.
```

You can get some details about `restserver` usage by using `--usage` argument:
```
wakaama/build $ ./restserver --usage
Usage: restserver [-?V] [-c FILE] [-C CERTIFICATE] [-k PRIVATE_KEY]
            [-l LOGGING_LEVEL] [--config=FILE] [--certificate=CERTIFICATE]
            [--private_key=PRIVATE_KEY] [--log=LOGGING_LEVEL] [--help]
```

**Arguments list:**
- `-c CONFIG_FILE` and `--config CONFIG_FILE` is used to load config file.

     Example of configuration file is in configuration section (below)
     
- `-k PRIVATE_KEY` and `--private_key PRIVATE_KEY` specify TLS security private key file.
  Private key could be generated with following command:
  ```
  $ openssl genrsa -out private.key 2048
  ```
  
- `-C CERTIFICATE` and `--certificate CERTIFICATE` specify TLS security certificate file.
  Certificate could be generated with following command (it requires private key file)
  ```
  $ openssl req -days 365 -new -x509 -key private.key -out certificate.pem
  ```
  
- `-l LOGGING_LEVEL` and `--log LOGGING_LEVEL` specify logging level from 0 to 5:

    `0: FATAL` - only very important messages are printed to console (usually the ones that inform about program malfunction).
    
    `1: ERROR` - important messages are printed to console (usually the ones that inform about service malfunction).
    
    `2: WARN` - warnings about possible malfunctions are reported.
    
    `3: INFO` - information about service actions (e.g., registration of new clients).
    
    `4: DEBUG` - more detailed information about service actions (e.g., detailed information about new clients).
    
    `5: TRACE` - very detailed information about program actions, including code tracing.
    
- `-V` and `--version` - print program version.

**configuration file**

Example of configuration file:
```
{
  "http": {
    "port": 8888,
    "security": {
      "private_key": "private.key",
      "certificate": "certificate.pem",
      "jwt": {
        "method": "header",
        "decode_key": "some-very-secret-key",
        "algorithm": "HS512",
        "expiration_time": 3600,
        "users": [
          {
            "name": "admin",
            "secret": "not-same-as-name",
            "scope": [".*"]
          },
          {
            "name": "get-all",
            "secret": "lets-get",
            "scope": ["GET.*$"]
          },
          {
            "name": "one-device",
            "secret": "only-one-dev",
            "scope": [".* /endpoints/threeSeven/.*"]
          }
        ]
      }
    }
  },
  "coap": {
    "port": 5555
  },
  "logging": {
    "level": 5
  }
}
```

- **`http` settings section:**
  - `port` - HTTP port to create socket on (is mentioned in arguments list)
  
  - **`security` settings subsection:**
    - ``private_key`` - TLS security private key file (is mentioned in arguments list)
    - ``certificate`` - TLS security certificate file (is mentioned in arguments list)
    - **`jwt` settings subsection (more about JWT could be found in [official website](https://jwt.io/)):**
      -  ``method`` - Describes where in the packet user will have to put token. Valid values: ``"body"``, ``"header"`` _(string)_, _more about it can be found in [REST API documentation](./RESTAPI.md)_
      -  ``decode_key`` - Key which will be used in token signing and verification, must be a secure key, which would decrease token falsification risk.
      -  ``algorithm`` - Signature encoding method _(more complex method will cause signature to be longer, which means increased load, however this would increase security too)_. Valid values: ``"HS256"``, ``"HS384"``, ``"HS512"``, ``"RS256"``, ``"RS384"``, ``"RS512"``, ``"ES256"``, ``"ES384"``, ``"ES512"`` _(string)_
      -  ``expiration_time`` - Seconds after which token is expired and wont be accepted anymore _(integer)_, default is `3600`
      -  ``users`` - List, which contains JWT authentication users. If no Users are specified, users are not required to perform authentication _(list of objects)_
      
         User object structure (more in [REST API documentation](./RESTAPI.md)):
         - ``name`` - User name, which will be used on authentication process _(string)_
         - ``secret`` - User secret, which will be used on authentication process _(string)_
         - ``scope`` - User scope, which will be used on validating user request access, if user wont have required scope, it will get _Access Denied_ _(list of strings)_.
         
         User scope should be **Regular expression pattern**, for example if you want user to have access to all GET requests , pattern should be `"GET .*"`, or if you would like user to have access to specific device manipulation: `".* /endpoints/threeSeven/.*"`, ultimate scope (all access) would be `".*"`.

  
- **`coap`**
  - `port` - COAP port to create socket on (is mentioned in arguments list)

- **`logging`**
  - `level` - visible messages logging level requirement (is mentioned in arguments list)
