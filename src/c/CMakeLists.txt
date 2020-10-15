cmake_minimum_required(VERSION 3.0)
project(device_coap LANGUAGES C)

# Package support
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

# Versioning
file (STRINGS ${CMAKE_SOURCE_DIR}/../../VERSION COAP_DOT_VERSION)

find_package (Libcoap REQUIRED)
if (NOT LIBCOAP_FOUND)
  message (WARNING "coap library or header not found")
endif ()

find_package (Libcsdk REQUIRED)
if (NOT LIBCSDK_FOUND)
  message (WARNING "csdk library or header not found")
endif ()

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
find_library(EDGEX_CSDK_RELEASE_LIB NAMES csdk)
find_library(LIBCOAP_LIB coap-2)
find_library(TINYDTLS_LIB tinydtls)
add_executable(device-coap ${C_FILES})
target_compile_definitions(device-coap PRIVATE VERSION="${COAP_DOT_VERSION}")
target_include_directories (device-coap PRIVATE .)
target_link_libraries (device-coap PUBLIC m PRIVATE ${LIBCOAP_LIB} ${TINYDTLS_LIB} ${EDGEX_CSDK_RELEASE_LIB})
install(TARGETS device-coap DESTINATION bin)
