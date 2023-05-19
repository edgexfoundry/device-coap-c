cmake_minimum_required(VERSION 3.0)
project(device_coap LANGUAGES C)

# IOT utils version
set (IOT_VER 1.5)
set (IOT_INCLUDE /opt/iotech/iot/${IOT_VER}/include)
set (IOT_LIB /opt/iotech/iot/${IOT_VER}/lib)

# Package support
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

# Versioning
file (STRINGS ${CMAKE_SOURCE_DIR}/../../VERSION COAP_DOT_VERSION)

find_package (LIBCOAP REQUIRED)
if (NOT LIBCOAP_FOUND)
  message (WARNING "coap library or header not found")
endif ()

find_library(EDGEX_CSDK_LIB NAMES csdk PATHS ENV CSDK_DIR PATH_SUFFIXES lib)
if(EDGEX_CSDK_LIB STREQUAL "EDGEX_CSDK_LIB-NOTFOUND")
    message(FATAL_ERROR "No C SDK library found in the default paths or $CSDK_DIR/lib. Please check your installation.")
endif()

find_path(EDGEX_CSDK_INCLUDE NAMES devsdk/devsdk.h PATHS ENV CSDK_DIR PATH_SUFFIXES include)
if(EDGEX_CSDK_INCLUDE STREQUAL "EDGEX_CSDK_INCLUDE-NOTFOUND")
    message(FATAL_ERROR "No C SDK header found in the default paths or $CSDK_DIR/include. Please check your installation.")
endif()

# Set additional C compiler flags
if (DEFINED CMAKE_C_COMPILER_ID)
  if (CMAKE_C_COMPILER_ID MATCHES GNU)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wstrict-prototypes -Wmissing-prototypes")
    set (CMAKE_C_STANDARD 99)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
    set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -ggdb -O0")
    set (CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -ggdb -O0")
    set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -DNDEBUG")
    set (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -O2")
   endif ()
endif ()

file (GLOB C_FILES *.c)
find_library(LIBCOAP_LIB coap-2)
find_library(TINYDTLS_LIB tinydtls)
add_executable(device-coap ${C_FILES})
target_compile_definitions(device-coap PRIVATE VERSION="${COAP_DOT_VERSION}")
target_include_directories (device-coap PRIVATE ${EDGEX_CSDK_INCLUDE} ${IOT_INCLUDE})
target_link_directories(device-coap PUBLIC ${EDGEX_CSDK_LIB} ${IOT_LIB})
target_link_libraries (device-coap PUBLIC ${LIBCOAP_LIB} ${TINYDTLS_LIB} csdk iot m)
install(TARGETS device-coap DESTINATION bin)
