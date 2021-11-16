Presently `device-coap-c` is a proof of concept. Below are ideas for future development, more or less in priority order.

* Define security parameters in Vault/Consul rather than configuration file
* Require use of DTLS client identity
* Support a DTLS pre-shared key per client; presently supports a single key shared by all clients
* Use OpenSSL for TLS library rather than tinydtls; OpenSSL available in Alpine v3.11 Docker base image, so don't have to build it
* Define libcoap ports in device config

