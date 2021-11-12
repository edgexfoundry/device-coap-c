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

#include "device-coap.h"
#ifdef __cplusplus
extern "C" {
#endif

#define COAP_URI_MAXLEN \
  32  // should be minimum of (sizeof("/a1r") + sizeof("/device-name") +
      // sizeof("resource-name"))

typedef struct {
  char end_dev_addr[256];             // To hold IPv6 address
  coap_security_mode_t security_mode; /**< CoAP transport security mode */
  char psk_key[16];
} end_dev_params;

bool GetEndDeviceProtocolProperties(const devsdk_protocols *protocols,
                                    char *protocol_name, iot_data_t **exception,
                                    end_dev_params *end_dev_params_ptr,
                                    coap_driver *driver);
extern int CoapSendCommandToEndDevice(uint8_t *data, size_t len, char *dev_name,
                                      char *resource_name,
                                      end_dev_params *end_dev_params_ptr,
                                      coap_driver *driver);
extern int CoapGetRequestToEndDevice(char *dev_name, char *resource_name,
                                     end_dev_params *end_dev_params_ptr,
                                     coap_driver *driver);
#ifdef __cplusplus
}
#endif
#endif
