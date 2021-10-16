/*
 * Copyright (c) 2020
 * Ken Bannister
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _COAP_CLIENT_H_
#define _COAP_CLIENT_H_ 1

/**
 * @file
 * @brief Defines coap client artifacts for the CoAP device service.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define COAP_URI_MAXLEN \
  32  // should be minimum of (sizeof("/a1r") + sizeof("/device-name") +
      // sizeof("resource-name"))

extern int CoapSendCommandToEndDevice(uint8_t *data, size_t len, char *dev_name,
                                      char *resource_name);
extern bool GetEndDeviceConfig(char *dev_name);
extern int CoapGetRequestToEndDevice(char *dev_name, char *resource_name);
#ifdef __cplusplus
}
#endif
#endif
