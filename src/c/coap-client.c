/* CoAP cli.t .r thread device service
 *
 * Copyright (c) 2021
 * Sudhamani
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "coap-client.h"

#include <coap2/coap.h>
#include <errno.h>
#include <float.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "coap-util.h"
#include "device-coap.h"
#include "edgex/devices.h"

/* controls input loop */
extern volatile sig_atomic_t quit;
iot_data_t *coap_resp_data = NULL;
static bool coap_resp_rcvd = false;

/*
 * Get End device protocol property, expect 5 arguments:
 * @param[in] protocol structure
 * @param[in] protocol name
 * @param[in] exception ptr
 * @param[in] end device structure variable address to store protocol properties
 * @param[in] coap driver pointer
 * @return success or fail
 */
bool GetEndDeviceProtocolProperties(const devsdk_protocols *protocols,
                                    char *protocol_name, iot_data_t **exception,
                                    end_dev_params *end_dev_params_ptr,
                                    coap_driver *driver) {
  coap_driver *sdk_ctx = (coap_driver *)driver;
  const iot_data_t *props =
      devsdk_protocols_properties(protocols, protocol_name);
  if (props == NULL) {
    *exception = iot_data_alloc_string("No COAP protocol in device address",
                                       IOT_DATA_REF);
    return false;
  }
  const char *params_ptr = iot_data_string_map_get_string(props, "ED_ADDR");
  if (params_ptr == NULL) {
    *exception = iot_data_alloc_string("property in device address missing",
                                       IOT_DATA_REF);
    return false;
  }
  strcpy(end_dev_params_ptr->end_dev_addr, params_ptr);
  iot_log_debug(sdk_ctx->lc, "COAP:End dev addr ptr= %s",
                end_dev_params_ptr->end_dev_addr);

  params_ptr = iot_data_string_map_get_string(props, "ED_SecurityMode");
  if (params_ptr == NULL) {
    *exception = iot_data_alloc_string("property in device address missing",
                                       IOT_DATA_REF);
    return false;
  }
  iot_log_debug(sdk_ctx->lc, "COAP: End dev SecMode ptr= %s", params_ptr);
  end_dev_params_ptr->security_mode = find_security_mode(params_ptr);

  switch (end_dev_params_ptr->security_mode) {
    case SECURITY_MODE_UNKNOWN: {
      iot_log_error(sdk_ctx->lc, "COAP:ED Unknown security mode");
      return false;
    }
    case SECURITY_MODE_PSK: {
      params_ptr = iot_data_string_map_get_string(props, "ED_PskKey");
      if (params_ptr == NULL) {
        *exception = iot_data_alloc_string("property in device address missing",
                                           IOT_DATA_REF);
        return false;
      }
      iot_log_debug(sdk_ctx->lc, "COAP:End dev PskKey ptr= %s", params_ptr);
      if (strlen(params_ptr)) {
        strcpy(end_dev_params_ptr->psk_key, params_ptr);
        iot_log_debug(sdk_ctx->lc, "COAP:PSK key len %u",
                      strlen(end_dev_params_ptr->psk_key));
      } else {
        iot_log_error(sdk_ctx->lc, "COAP:ED PSK key not in configuration");
        return false;
      }
      break;
    }
    case SECURITY_MODE_NOSEC:
      break;
    default:
      break;
  }
  return true;
}
/*
 * coap get request callback handler. Get coap data from the coap pdu and update
 * it to coap_resp_data
 */
static void message_handler(struct coap_context_t *ctx, coap_session_t *session,
                            coap_pdu_t *sent, coap_pdu_t *received,
                            const coap_tid_t id) {
  edgex_device *device = NULL;
  edgex_deviceresource *resource = NULL;
  uint8_t *data = NULL;
  size_t len = 0;
  coap_driver *sdk_ctx = (coap_driver *)impl;
  coap_resp_data = NULL;
  coap_show_pdu(LOG_WARNING, received);
  switch (sent->code) {
    case COAP_REQUEST_GET: {
      /* Validate URI, expect 3 segments: /a1r/{device-name}/{resource-name} */
      if (!parse_path(sent, &device, &resource)) {
        goto finish;
      }

      if (!coap_get_data(received, &len, &data)) {
        iot_log_error(sdk_ctx->lc, "COAP:invalid data of len %u", len);
      }

      iot_log_debug(sdk_ctx->lc, "COAP: coap device resource type %d",
                    resource->properties->type);

      /* Validate and read payload. Content format from option must be
       * acceptable for resource value type. */

      switch (resource->properties->type.type) {
        case IOT_DATA_FLOAT64:
          /* data conversion requires a null terminated string */
          coap_resp_data = read_data_float64(data, len);
          iot_log_debug(sdk_ctx->lc, "COAP:coap float data=%s, len = %d", data,
                        len);
          break;

        case IOT_DATA_INT32:
          iot_log_debug(sdk_ctx->lc, "COAP:coap int32 data=%s, len = %d", data,
                        len);
          coap_resp_data = read_data_int32(data, len);
          break;

        case IOT_DATA_STRING:
          coap_resp_data = read_data_string(data, len);
          iot_log_debug(sdk_ctx->lc, "COAP:coap json data=%s, len = %d", data,
                        len);
          break;

        default:
          iot_log_error(sdk_ctx->lc, "COAP:unsupported resource type %d",
                        resource->properties->type);
          goto finish;
      }
      break;
    }
    case COAP_REQUEST_PUT:
      break;
    default:
      break;
  }

finish:
  if (device != NULL) {
    edgex_free_device(sdk_ctx->service, (edgex_device *)device);
  }
  coap_resp_rcvd = true;
}
/*
Handling NACK messages for coap requests
*/
static void nack_handler(coap_context_t *context, coap_session_t *session,
                         coap_pdu_t *sent, coap_nack_reason_t reason,
                         const coap_tid_t id) {
  coap_driver *sdk_ctx = (coap_driver *)impl;
  switch (reason) {
    case COAP_NACK_TOO_MANY_RETRIES:
    case COAP_NACK_NOT_DELIVERABLE:
    case COAP_NACK_RST:
    case COAP_NACK_TLS_FAILED:
    case COAP_NACK_ICMP_ISSUE:
      coap_resp_rcvd = true;
      iot_log_error(sdk_ctx->lc, "COAP:NACK response from server");
      break;
    default:
      coap_resp_rcvd = true;
      break;
  }
  return;
}

/*
waits for coap response for CON request from end device
*/
static void WaitForCoapResponseFromEndDevice(coap_context_t *ctx,
                                             coap_session_t *session) {
  while (1) {
    coap_io_process(ctx, COAP_IO_WAIT);
    if (coap_resp_rcvd == true || quit == 1) {
      coap_session_release(session);
      coap_free_context(ctx);
      coap_resp_rcvd = false;
      break;
    }
  }
}
/*
send put request to end device
*/
int CoapSendCommandToEndDevice(uint8_t *data, size_t len, char *dev_name,
                               char *resource_name,
                               end_dev_params *end_dev_params_ptr,
                               coap_driver *driver) {
  coap_context_t *ctx = NULL;
  coap_session_t *session = NULL;
  coap_address_t dst;
  coap_pdu_t *pdu = NULL;
  coap_driver *sdk_ctx = (coap_driver *)driver;
  int result = EXIT_FAILURE;
  uint8_t *post_data = data;
  char uri[COAP_URI_MAXLEN + 1] = {0};

  coap_startup();
  coap_proto_t proto = COAP_PROTO_UDP;
  char *port = "5683";

  if (end_dev_params_ptr->security_mode != SECURITY_MODE_NOSEC) {
    proto = COAP_PROTO_DTLS;
    port = "5684";
  }

  if (resolve_address(end_dev_params_ptr->end_dev_addr, port, &dst) < 0) {
    coap_log(LOG_CRIT, "COAP:failed to resolve address\n");
    return result;
  }
  iot_log_debug(sdk_ctx->lc, "COAP: End dev addr = %s",
                end_dev_params_ptr->end_dev_addr);

  iot_log_debug(sdk_ctx->lc, "COAP: Data = %d, Len = %d", *post_data, len);
  /* create CoAP context and a client session */
  ctx = coap_new_context(NULL);
  if (ctx == NULL) {
    iot_log_error(sdk_ctx->lc, "COAP:coap new context creation failed\n");
    return result;
  }
  if (end_dev_params_ptr->security_mode == SECURITY_MODE_PSK) {
    iot_log_debug(sdk_ctx->lc, "COAP-client:ED psk key = %s, len=%d",
                  (uint8_t *)end_dev_params_ptr->psk_key,
                  strlen(end_dev_params_ptr->psk_key));
    if (!ctx || !(session = coap_new_client_session_psk(
                      ctx, NULL, &dst, proto, "r17",
                      (uint8_t *)(end_dev_params_ptr->psk_key),
                      strlen(end_dev_params_ptr->psk_key)))) {
      iot_log_error(sdk_ctx->lc, "COAP:cannot initialize PSK");
      return result;
    }
  } else {
    if (!ctx || !(session = coap_new_client_session(ctx, NULL, &dst, proto))) {
      coap_log(LOG_EMERG, "COAP:cannot create client session\n");
      return result;
    }
  }
  coap_register_response_handler(ctx, message_handler);
  coap_register_nack_handler(ctx, nack_handler);

  /* construct CoAP message */
  pdu = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_PUT,
                      coap_new_message_id(session) /* message id */,
                      coap_session_max_pdu_size(session));
  if (!pdu) {
    coap_log(LOG_EMERG, "COAP:cannot create PDU\n");
    return result;
  }

  sprintf(uri, "/a1r/%s/%s", dev_name, resource_name);
  int res;
#define BUFSIZE 40
  unsigned char _buf[BUFSIZE] = {0};
  unsigned char *buf = _buf;
  size_t buflen;
  buflen = BUFSIZE;
  res = coap_split_path((const uint8_t *)uri, strlen(uri), buf, &buflen);

  while (res--) {
    coap_add_option(pdu, COAP_OPTION_URI_PATH, coap_opt_length(buf),
                    coap_opt_value(buf));
    iot_log_debug(sdk_ctx->lc, "uri.path_val = %s\n", coap_opt_value(buf));

    buf += coap_opt_size(buf);
  }
  if (result <= 0) {  // size
    result = EXIT_FAILURE;
    return result;
  }
  if (!coap_add_data(pdu, len, (unsigned char *)post_data)) {
    return result;
  }
  coap_show_pdu(LOG_WARNING, pdu);

  /* and send the PDU */
  if (coap_send(session, pdu) == COAP_INVALID_TID) {
    return result;
  }
  WaitForCoapResponseFromEndDevice(ctx, session);
  result = EXIT_SUCCESS;
  return result;
}
/*
send coap get request to end device. waits for the response form end device.
*/
int CoapGetRequestToEndDevice(char *dev_name, char *resource_name,
                              end_dev_params *end_dev_params_ptr,
                              coap_driver *driver) {
  coap_context_t *ctx = NULL;
  coap_session_t *session = NULL;
  coap_address_t dst;
  coap_pdu_t *pdu = NULL;
  coap_driver *sdk_ctx = (coap_driver *)driver;
  int result = EXIT_FAILURE;
  char uri[COAP_URI_MAXLEN + 1] = {0};

  coap_startup();

  coap_proto_t proto = COAP_PROTO_UDP;
  char *port = "5683";

  if (end_dev_params_ptr->security_mode != SECURITY_MODE_NOSEC) {
    proto = COAP_PROTO_DTLS;
    port = "5684";
  }

  if (resolve_address(end_dev_params_ptr->end_dev_addr, port, &dst) < 0) {
    coap_log(LOG_CRIT, "COAP:failed to resolve address\n");
    return result;
  }
  iot_log_debug(sdk_ctx->lc, "COAP: End dev addr = %s",
                end_dev_params_ptr->end_dev_addr);

  /* create CoAP context and a client session */
  ctx = coap_new_context(NULL);
  if (ctx == NULL) {
    iot_log_error(sdk_ctx->lc, "COAP:coap new context creation failed\n");
    return result;
  }
  if (end_dev_params_ptr->security_mode == SECURITY_MODE_PSK) {
    iot_log_debug(sdk_ctx->lc, "COAP-client:ED psk key = %s, len=%d",
                  (uint8_t *)end_dev_params_ptr->psk_key,
                  strlen(end_dev_params_ptr->psk_key));
    if (!ctx || !(session = coap_new_client_session_psk(
                      ctx, NULL, &dst, proto, "r17",
                      (uint8_t *)(end_dev_params_ptr->psk_key),
                      strlen(end_dev_params_ptr->psk_key)))) {
      iot_log_error(sdk_ctx->lc, "COAP:cannot initialize PSK");
      return result;
    }
  } else {
    if (!ctx || !(session = coap_new_client_session(ctx, NULL, &dst, proto))) {
      coap_log(LOG_EMERG, "COAP:cannot create client session\n");
      return result;
    }
  }
  coap_register_response_handler(ctx, message_handler);
  coap_register_nack_handler(ctx, nack_handler);

  /* construct CoAP message */
  pdu = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_GET,
                      coap_new_message_id(session) /* message id */,
                      coap_session_max_pdu_size(session));
  if (!pdu) {
    coap_log(LOG_EMERG, "COAP:cannot create PDU\n");
    return result;
  }

  sprintf(uri, "/a1r/%s/%s", dev_name, resource_name);
  int res;
#define BUFSIZE 40
  unsigned char _buf[BUFSIZE] = {0};
  unsigned char *buf = _buf;
  size_t buflen;
  buflen = BUFSIZE;
  res = coap_split_path((const uint8_t *)uri, strlen(uri), buf, &buflen);
  while (res--) {
    result = coap_add_option(pdu, COAP_OPTION_URI_PATH, coap_opt_length(buf),
                             coap_opt_value(buf));
    iot_log_debug(sdk_ctx->lc, "uri.path_val = %s\n", coap_opt_value(buf));

    buf += coap_opt_size(buf);
  }
  if (result <= 0) {  // size
    result = EXIT_FAILURE;
    return result;
  }

  coap_show_pdu(LOG_WARNING, pdu);
  /* and send the PDU */
  if (coap_send(session, pdu) == COAP_INVALID_TID) {
    coap_log(LOG_EMERG, "COAP:coap_send cannot send pdu\n");
    return result;
  }
  WaitForCoapResponseFromEndDevice(ctx, session);
  result = EXIT_SUCCESS;
  return result;
}
