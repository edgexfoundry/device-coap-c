
.PHONY: build test clean docker

MICROSERVICES=build/release/device-coap
.PHONY: $(MICROSERVICES)

DOCKERS=device_coap
.PHONY: $(DOCKERS)

VERSION=$(shell cat ./VERSION)
GIT_SHA=$(shell git rev-parse HEAD)

build: ./VERSION ${MICROSERVICES}

build/release/device-coap:
	    scripts/build.sh

test:
	    @echo $(MICROSERVICES)

clean:
	    rm -f $(MICROSERVICES)
	    rm -f ./VERSION

./VERSION:
	    @git describe --abbrev=0 --tags | sed 's/^v//' > ./VERSION

version: ./VERSION
	    @echo ${VERSION}

docker: ./VERSION $(DOCKERS)

device_coap:
	    docker build \
	        -f scripts/Dockerfile.alpine \
	        --label "git_sha=$(GIT_SHA)" \
	        -t edgexfoundry/device-coap:${GIT_SHA} \
	        -t edgexfoundry/device-coap:${VERSION}-dev \
            .

build-debug: build/debug/device-coap

build/debug/device-coap:
	    scripts/build_debug.sh

clean-debug:
	    rm -f build/debug/device-coap

.PHONY: build-debug clean-debug
