#!/bin/sh

# Build dependencies
#
#   build_deps.sh <build-csdk>
#
#   Options:
#   build-csdk: 1 to build EdgeX C SDK, 0 to skip
#
# Assumes WORKDIR is /device-coap
set -e -x

BUILD_CSDK=$1

if [ -d deps ]
then
  exit 0
fi

# get tinydtls and libcoap from source repository and build
mkdir deps
cd /device-coap/deps

# for tinydtls
git clone https://github.com/eclipse/tinydtls.git
cd tinydtls
git checkout develop
git reset --hard b0e230d

# patches are all new files, for cmake build
cp /device-coap/scripts/AutoConf_cmake_patch AutoConf.cmake
cp /device-coap/scripts/CMakeLists_txt_patch CMakeLists.txt
cp /device-coap/scripts/dtls_config_h_cmake_in_patch dtls_config.h.cmake.in
cp /device-coap/scripts/tests_CMakeLists_txt_patch tests/CMakeLists.txt

mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON ..
make && make install
cd /device-coap/deps

# for libcoap
git clone https://github.com/obgm/libcoap.git
cd libcoap
# This version includes the most recent known good tinydtls, as of 2020-08-12
git reset --hard 1739507

# patch for include file path
patch -p1 < /device-coap/scripts/FindTinyDTLS_cmake_patch
# patch for bug
patch -p1 < /device-coap/scripts/config_h_in_patch

mkdir -p build && cd build
cmake -DWITH_EPOLL=OFF -DDTLS_BACKEND=tinydtls -DUSE_VENDORED_TINYDTLS=OFF \
      -DENABLE_TESTS=OFF -DENABLE_EXAMPLES=OFF -DENABLE_DOCS=OFF \
      -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      ..
make && make install

# get c-sdk from edgexfoundry and build
if [ "$BUILD_CSDK" = "1" ]
then
  cd /device-coap/deps

  git clone https://github.com/PJK/libcbor
  cd libcbor
  git reset --hard v0.7.0
  
  mkdir -p build && cd build
  cmake -DCMAKE_BUILD_TYPE=Release -DCBOR_CUSTOM_ALLOC=ON ..
  make
  make install
  cd /device-coap/deps

  wget https://github.com/edgexfoundry/device-sdk-c/archive/v1.2.2.zip
  unzip v1.2.2.zip
  cd device-sdk-c-1.2.2

  ./scripts/build.sh
  cp -rf include/* /usr/include/
  cp build/release/c/libcsdk.so /usr/lib/
fi

rm -rf /device-coap/deps

