/*
 * Copyright (c) 2020
 * Ken Bannister
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _DEVICE_COAP_H_
#define _DEVICE_COAP_H_ 1

/**
 * @file
 * @brief Defines common artifacts for the CoAP device service.
 */

#include "devsdk/devsdk.h"

#ifdef __cplusplus
extern "C" {
#endif

/** CoAP messaging transport security mode */
typedef enum
{
  SECURITY_MODE_PSK,        /**< pre-shared key */
  SECURITY_MODE_NOSEC,      /**< no security */
  SECURITY_MODE_UNKNOWN     /**< not a security mode; just means mode not known */
} coap_security_mode_t;


/**
 * device-coap-c specific data included with service callbacks
 */
typedef struct coap_driver
{
  iot_logger_t * lc;
  devsdk_service_t *service;
  iot_data_t *coap_bind_addr;           /**< Address server binds to, for incoming data */
  coap_security_mode_t security_mode;   /**< CoAP transport security mode */
  iot_data_t *psk_key;                  /**< PSK key as uint8_t array; unused if not PSK mode */
} coap_driver;

/**
 * Runs a CoAP server until a SIGINT or SIGTERM event.
 *
 * @param driver   EdgeX driver
 * @return EXIT_SUCCESS on normal completion
 * @return EXIT_FAILURE if unable to run server
 */
int run_server(coap_driver *driver);


#ifdef __cplusplus
}
#endif

#endif
