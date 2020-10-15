/* Entry point for an EdgeX device service that provides a CoAP server to receive
 * values asynchronously.
 *
 * Copyright (c) 2020
 * Ken Bannister
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <unistd.h>
#include <stdarg.h>

#include "devsdk/devsdk.h"
#include "device-coap.h"

#define ERR_CHECK(x) if (x.code) { fprintf (stderr, "Error: %d: %s\n", x.code, x.reason); devsdk_service_free (service); free (impl); return x.code; }

#define ERR_BUFSZ 1024
#define COAP_BIND_ADDR_KEY "CoapBindAddr"
#define SECURITY_MODE_KEY  "SecurityMode"
#define PSK_KEY_KEY        "PskKey"


/* Looks up security mode enum value from configuration text value */
static coap_security_mode_t find_security_mode
(
  const char *mode_text
)
{
  assert (mode_text);
  if (!strcmp (mode_text, "PSK"))
  {
    return SECURITY_MODE_PSK;
  }
  else if (!strcmp (mode_text, "NoSec"))
  {
    return SECURITY_MODE_NOSEC;
  }
  else
  {
    return SECURITY_MODE_UNKNOWN;
  }
}

/* Init callback; reads in config values to device driver */
static bool coap_init
(
  void *impl,
  struct iot_logger_t *lc,
  const iot_data_t *config
)
{
  coap_driver *driver = (coap_driver *) impl;

  driver->lc = lc;
  driver->security_mode = find_security_mode (iot_data_string_map_get_string (config, SECURITY_MODE_KEY));

  if (driver->security_mode == SECURITY_MODE_PSK)
  {
    const char *conf_psk_key = iot_data_string_map_get_string (config, PSK_KEY_KEY);
    if (strlen (conf_psk_key))
    {
      iot_data_t *key_array = iot_data_alloc_array_from_base64 (conf_psk_key);
      driver->psk_key = key_array;
      iot_log_info (lc, "PSK key len %u", iot_data_array_length (key_array));
    }
    else
    {
      iot_log_error (lc, "PSK key not in configuration");
      driver->psk_key = NULL;
      return false;
    }
  }
  else
  {
    driver->psk_key = NULL;
  }

  /* CoAP server bind address as text */
  const char *bind_addr = iot_data_string_map_get_string (config, COAP_BIND_ADDR_KEY);
  if (bind_addr)
  {
    driver->coap_bind_addr = iot_data_alloc_string (bind_addr, IOT_DATA_COPY);
  }
  else
  {
    iot_log_error (lc, "CoAP bind address not in configuration");
    driver->coap_bind_addr = NULL;
    return false;
  }

  iot_log_debug (lc, "Init complete");
  return true;
}

static bool coap_get_handler
(
  void *impl,
  const char *devname,
  const devsdk_protocols *protocols,
  uint32_t nreadings,
  const devsdk_commandrequest *requests,
  devsdk_commandresult *readings,
  const devsdk_nvpairs *qparms,
  iot_data_t **exception
)
{
  (void) impl;
  (void) devname;
  (void) protocols;
  (void) nreadings;
  (void) requests;
  (void) readings;
  (void) qparms;
  (void) exception;
  return true;
}

static bool coap_put_handler
(
  void *impl,
  const char *devname,
  const devsdk_protocols *protocols,
  uint32_t nvalues,
  const devsdk_commandrequest *requests,
  const iot_data_t *values[],
  iot_data_t **exception
)
{
  (void) impl;
  (void) devname;
  (void) protocols;
  (void) nvalues;
  (void) requests;
  (void) values;
  (void) exception;
  return true;
}

static void coap_stop (void *impl, bool force) {}

int main (int argc, char *argv[])
{
  coap_driver * impl = malloc (sizeof (coap_driver));
  memset (impl, 0, sizeof (coap_driver));

  devsdk_error e;
  e.code = 0;

  /* Device Callbacks */
  devsdk_callbacks coapImpls =
  {
    coap_init,
    NULL,              /* discovery */
    coap_get_handler,
    coap_put_handler,
    coap_stop,
    NULL,              /* auto-event started */
    NULL,              /* auto-event stopped */
    NULL,              /* device added */
    NULL,              /* device updated */
    NULL               /* device removed */
  };

  /* Initialize a new device service */
  devsdk_service_t *service = devsdk_service_new
    ("device-coap", VERSION, impl, coapImpls, &argc, argv, &e);
  ERR_CHECK (e);
  impl->service = service;

  int res = 0;
  int n = 1;
  while (n < argc)
  {
    if (strcmp (argv[n], "-h") == 0 || strcmp (argv[n], "--help") == 0)
    {
      printf ("Usage: device-coap [options]\n");
      printf ("\nOptions:\n");
      printf ("  -r, --registry  set registry URL\n");
      printf ("  -i, --instance  set instance name\n");
      printf ("  -p, --profile   set profile file\n");
      printf ("  -c, --confdir   set configuration directory\n");
      printf ("  -f, --file      set configuration file\n");
      printf ("  -h, --help      show this text\n");
      goto end;
    }
    else
    {
      printf ("%s: Unrecognized option %s\n", argv[0], argv[n]);
      res = 1;
      goto end;
    }
  }

  devsdk_service_start (service, &e);
  ERR_CHECK (e);

  /* Validate transport security mode from config/environment */
  if (impl->security_mode == SECURITY_MODE_UNKNOWN) {
      iot_log_error (impl->lc, "Unknown security mode\n");
      res = 1;
      goto stop;
  }

  /* Run CoAP server */
  run_server(impl);

 stop:
  devsdk_service_stop (service, true, &e);
  ERR_CHECK (e);

 end:
  devsdk_service_free (service);
  if (impl->coap_bind_addr)
  {
    iot_data_free (impl->coap_bind_addr);
  }
  if (impl->psk_key)
  {
    iot_data_free (impl->psk_key);
  }
  free (impl);
  puts ("Exiting gracefully");
  return res;
}
