/* CoAP server for device-coap-c
 *
 * Copyright (C) 2018 Olaf Bergmann <bergmann@tzi.org>
 * Copyright (c) 2020 Ken Bannister
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <float.h>
#include <signal.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include <coap2/coap.h>
#include "edgex/devices.h"
#include "coap-server.h" 
#include "device-coap.h"
#include "coap-util.h"

#define MSG_PAYLOAD_INVALID "payload not valid"
#define MEDIATYPE_TEXT_PLAIN "text/plain"
#define MEDIATYPE_APP_JSON "application/json"
#define CONTENT_FORMAT_UNDEFINED UINT16_MAX

static coap_driver *sdk_ctx;

/* controls input loop */
volatile sig_atomic_t quit = 0;

/* signal handler for input loop */
static void
handle_sig (int signum)
{
  (void)signum;
  quit = 1;
}

/*
 * Read data from device initiated CoAP POST to /a1r/{device-name}/{resource-name},
 * and post it via devsdk_post_readings().
 */
static void
data_handler (coap_context_t *context, coap_resource_t *coap_resource,
              coap_session_t *session, coap_pdu_t *request, coap_binary_t *token,
              coap_string_t *query, coap_pdu_t *response)
{
  (void)context;
  (void)coap_resource;
  (void)session;
  (void)request;
  (void)token;
  (void)query;

  /* reject default PUT method */
  if (request->code == COAP_REQUEST_PUT)
  {
    response->code = COAP_RESPONSE_CODE (405);
    return;
  }

  /* Validate URI, expect 3 segments: /a1r/{device-name}/{resource-name} */
  edgex_device *device = NULL;
  edgex_deviceresource *resource = NULL;
  if (!parse_path (request, &device, &resource))
  {
    response->code = COAP_RESPONSE_CODE (404);
    goto finish;
  }

  iot_data_t *iot_data = NULL;
  size_t len;
  uint8_t *data;
  if (!coap_get_data (request, &len, &data))
  {
    iot_log_info (sdk_ctx->lc, "invalid data of len %u", len);
    /* finalized after else clause */
  }
  else
  {
    /* Read CoAP content format option for validation below. */
    uint16_t cf = CONTENT_FORMAT_UNDEFINED;
    coap_opt_iterator_t it;
    coap_opt_t *opt = coap_check_option (request, COAP_OPTION_CONTENT_FORMAT, &it);
    if (opt)
    {
      cf = coap_decode_var_bytes (coap_opt_value (opt), coap_opt_length (opt));
    }

    /* Validate and read payload. Content format from option must be acceptable
     * for resource value type. */
    switch (resource->properties->type.type)
    {
      case IOT_DATA_FLOAT64:
        if (cf != COAP_MEDIATYPE_TEXT_PLAIN)
        {
          response->code = COAP_RESPONSE_CODE (415);
          goto finish;
        }
        iot_data = read_data_float64 (data, len);
        break;

      case IOT_DATA_INT32:
        if (cf != COAP_MEDIATYPE_TEXT_PLAIN)
        {
          response->code = COAP_RESPONSE_CODE (415);
          goto finish;
        }
        iot_data = read_data_int32 (data, len);
        break;

      case IOT_DATA_STRING:
        if (cf != COAP_MEDIATYPE_TEXT_PLAIN && cf != COAP_MEDIATYPE_APPLICATION_JSON)
        {
          response->code = COAP_RESPONSE_CODE (415);
          goto finish;
        }
        iot_data = read_data_string (data, len);
        break;

      default:
        iot_log_error (sdk_ctx->lc, "unsupported resource type %d", resource->properties->type);
        response->code = COAP_RESPONSE_CODE (500);
        goto finish;
    }
  }
  if (!iot_data)
  {
    response->code = COAP_RESPONSE_CODE (400);
    coap_add_data (response, strlen (MSG_PAYLOAD_INVALID), (uint8_t *)MSG_PAYLOAD_INVALID);
    goto finish;
  }

  /* generate and post an event with the data */
  devsdk_commandresult results[1];
  results[0].origin = 0;
  results[0].value = iot_data;

  devsdk_post_readings (sdk_ctx->service, device->name, resource->name, results);
  iot_data_free (results[0].value);

  response->code = COAP_RESPONSE_CODE (204);

 finish:
  edgex_free_device (sdk_ctx->service, device);
}

int
run_server (void)
{
  coap_context_t  *ctx = NULL;
  coap_address_t bind_addr;
  coap_resource_t *resource = NULL;
  int result = EXIT_FAILURE;
  sdk_ctx = impl;
  struct sigaction sa;

  coap_startup ();

  /* Use EdgeX log level */
  coap_log_t log_level;
  switch (sdk_ctx->lc->level)
  {
    case IOT_LOG_ERROR:
      log_level = LOG_ERR;
      break;
    case IOT_LOG_WARN:
      log_level = LOG_WARNING;
      break;
    case IOT_LOG_DEBUG:
      log_level = LOG_DEBUG;
      break;
    default:
      log_level = LOG_INFO;
  }
  coap_set_log_level (log_level);
  /* workaround for tinydtls log level mismatch to avoid excessive debug logging */
  coap_tls_version_t *ver = coap_get_tls_library_version ();
  if (ver->type == COAP_TLS_LIBRARY_TINYDTLS && log_level == LOG_INFO)
  {
    coap_dtls_set_log_level (log_level - 1);
  }
  else
  {
    coap_dtls_set_log_level (log_level);
  }

  /* Resolve destination address where server should be sent. Use CoAP default ports. */
  coap_proto_t proto = COAP_PROTO_UDP;
  char *port = "5683";
  if (sdk_ctx->security_mode != SECURITY_MODE_NOSEC)
  {
    proto = COAP_PROTO_DTLS;
    port = "5684";
  }
  if (resolve_address (iot_data_string (sdk_ctx->coap_bind_addr), port, &bind_addr) < 0) {
    iot_log_error (sdk_ctx->lc, "failed to resolve CoAP bind address");
    goto finish;
  }

  /* setup libcoap for a server */
  if (!(ctx = coap_new_context (NULL)))
  {
    iot_log_error (sdk_ctx->lc, "cannot initialize context");
    goto finish;
  }

  if (sdk_ctx->security_mode == SECURITY_MODE_PSK)
  {
    /* use iterator just to get address of PSK key data */
    iot_data_array_iter_t array_iter;
    iot_data_array_iter (sdk_ctx->psk_key, &array_iter);
    iot_data_array_iter_next(&array_iter);

    if (!(coap_context_set_psk (ctx, "", (uint8_t *)iot_data_array_iter_value (&array_iter),
                                iot_data_array_length (sdk_ctx->psk_key))))
    {
      iot_log_error (sdk_ctx->lc, "cannot initialize PSK");
      goto finish;
    }
  }

  if (!(coap_new_endpoint (ctx, &bind_addr, proto)))
  {
    iot_log_error (sdk_ctx->lc, "cannot initialize listen endpoint");
    goto finish;
  }

  /* Creates handler for PUT, which is not what we want... */
  resource = coap_resource_unknown_init (&data_handler);
  /* ... so add POST handler also. */
  coap_register_handler (resource, COAP_REQUEST_POST, &data_handler);
  coap_add_resource (ctx, resource);

  /* setup signal handling for input loop */
  sigemptyset (&sa.sa_mask);
  sa.sa_handler = handle_sig;
  sa.sa_flags = 0;
  sigaction (SIGINT, &sa, NULL);
  sigaction (SIGTERM, &sa, NULL);

  iot_log_info (sdk_ctx->lc, "CoAP %s server started on %s", sdk_ctx->psk_key ? "PSK" : "NoSec",
                iot_data_string (sdk_ctx->coap_bind_addr));

  while (!quit)
  {
    coap_io_process (ctx, COAP_IO_WAIT);
  }

  result = EXIT_SUCCESS;

 finish:

  coap_free_context (ctx);
  coap_cleanup ();

  return result;
}
