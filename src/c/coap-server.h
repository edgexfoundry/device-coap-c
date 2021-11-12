/*
 * Copyright (c) 2020
 * Ken Bannister
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _COAP_SERVER_H_
#define _COAP_SERVER_H_ 1

/**
 * @file
 * @brief Defines coap server artifacts for the CoAP device service.
 */
#ifdef __cplusplus
extern "C" {
#endif
/**
 * Runs a CoAP server until a SIGINT or SIGTERM event.
 *
 * @return EXIT_SUCCESS on normal completion
 * @return EXIT_FAILURE if unable to run server
 */
extern int run_server(void);
#ifdef __cplusplus
}
#endif

#endif
