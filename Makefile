
.PHONY: build test clean docker

MICROSERVICES=build/release/device-coap build/debug/device-coap
.PHONY: $(MICROSERVICES)

DOCKERS=docker_device_coap_c
.PHONY: $(DOCKERS)

VERSION=$(shell cat ./VERSION || echo 0.0.0)
GIT_SHA=$(shell git rev-parse HEAD)

build: ${MICROSERVICES}

build/release/device-coap:
	    scripts/build.sh

test:
	    @echo $(MICROSERVICES)

clean:
	    rm -f $(MICROSERVICES)

docker: $(DOCKERS)

docker_device_coap_c:
	    docker build \
	        -f scripts/Dockerfile.alpine-3.11 \
	        --label "git_sha=$(GIT_SHA)" \
	        -t edgexfoundry/docker-device-coap-c:${GIT_SHA} \
	        -t edgexfoundry/docker-device-coap-c:${VERSION} \
            .

build-debug: build/debug/device-coap

build/debug/device-coap:
	    scripts/build_debug.sh

clean-debug:
	    rm -f build/debug/device-coap

.PHONY: build-debug clean-debug
