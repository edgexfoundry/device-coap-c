/*
 * Copyright (c) 2020
 * Ken Bannister
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _COAP_UTIL_H_
#define _COAP_UTIL_H_ 1

/**
 * @file
 * @brief Defines common utils for the CoAP device service.
 */

#include <coap2/coap.h>
#include <devsdk/devsdk.h>
#include <edgex/devices.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define RESOURCE_SEG1 "a1r"
/* Maximum length of a string containing numeric values. */
#define FLOAT64_STR_MAXLEN 24
#define INT32_STR_MAXLEN 11

// to decode coap encoded uri in coap client message handler
#define __ 0xFF
static const unsigned char hexval[0x100] = {
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, /* 00-0F */
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, /* 00-0F */
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, /* 00-0F */
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  __, __, __, __, __, __, /* 30-3F */
    __, 10, 11, 12, 13, 14, 15, __, __, __, __, __, __, __, __, __, /* 40-4F */
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, /* 00-0F */
    __, 10, 11, 12, 13, 14, 15, __, __, __, __, __, __, __, __, __, /* 60-6F */
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, /* 00-0F */
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, /* 00-0F */
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, /* 00-0F */
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, /* 00-0F */
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, /* 00-0F */
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, /* 00-0F */
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, /* 00-0F */
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, /* 00-0F */
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, /* 00-0F */
};

extern int resolve_address(const char *host, const char *service,
                           coap_address_t *lib_addr);
extern iot_data_t *read_data_float64(uint8_t *data, size_t len);
extern iot_data_t *read_data_int32(uint8_t *data, size_t len);
extern iot_data_t *read_data_string(uint8_t *data, size_t len);
extern bool parse_path(coap_pdu_t *request, edgex_device **device_ptr,
                       edgex_deviceresource **resource_ptr);
#ifdef __cplusplus
}
#endif

#endif
