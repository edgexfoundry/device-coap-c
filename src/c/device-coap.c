/* Entry point for an EdgeX device service that provides a CoAP server to
 * receive values asynchronously.
 *
 * Copyright (c) 2020
 * Ken Bannister
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "device-coap.h"

#include <stdarg.h>
#include <unistd.h>

#include "coap-client.h"
#include "coap-server.h"
#include "coap-util.h"
#include "devsdk/devsdk.h"
#define ERR_CHECK(x)                                      \
  if (x.code) {                                           \
    fprintf(stderr, "Error: %d: %s\n", x.code, x.reason); \
    devsdk_service_free(service);                         \
    free(impl);                                           \
    return x.code;                                        \
  }

#define COAP_BIND_ADDR_KEY "CoapBindAddr"
#define SECURITY_MODE_KEY "SecurityMode"
#define PSK_KEY_KEY "PskKey"

coap_driver *impl;
extern iot_data_t *coap_resp_data;

/* Looks up security mode enum value from configuration text value */
coap_security_mode_t find_security_mode(const char *mode_text) {
  assert(mode_text);
  if (!strcmp(mode_text, "PSK")) {
    return SECURITY_MODE_PSK;
  } else if (!strcmp(mode_text, "NoSec")) {
    return SECURITY_MODE_NOSEC;
  } else {
    return SECURITY_MODE_UNKNOWN;
  }
}

/* Init callback; reads in config values to device driver */
static bool coap_init(void *impl, struct iot_logger_t *lc,
                      const iot_data_t *config) {
  coap_driver *driver = (coap_driver *)impl;
  bool result = true;

  driver->lc = lc;
  pthread_mutex_init(&driver->mutex, NULL);
  driver->security_mode = find_security_mode(
      iot_data_string_map_get_string(config, SECURITY_MODE_KEY));

  switch (driver->security_mode) {

    case SECURITY_MODE_UNKNOWN: {
      iot_log_error(lc, "Unknown security mode");
      driver->psk_key = NULL;
      result = false;
      break;
    }

    case SECURITY_MODE_PSK: {
      iot_data_t *secrets = devsdk_get_secrets (driver->service, "psk");
      const char *conf_psk_key =
          iot_data_string_map_get_string(secrets, PSK_KEY_KEY);
      if (!conf_psk_key) {
          conf_psk_key = iot_data_string_map_get_string(config, PSK_KEY_KEY);
      }
      if (strlen(conf_psk_key)) {
        iot_data_t *key_array = iot_data_alloc_array_from_base64(conf_psk_key);
        driver->psk_key = key_array;
        iot_log_info(lc, "PSK key len %u", iot_data_array_length(key_array));
      } else {
        iot_log_error(lc, "PSK key not in configuration");
        driver->psk_key = NULL;
        result = false;
      }
      iot_data_free (secrets);
      break;
    }

    default: {
      driver->psk_key = NULL;
      break;
    }
  }

  /* CoAP server bind address as text */
  const char *bind_addr =
      iot_data_string_map_get_string(config, COAP_BIND_ADDR_KEY);
  if (bind_addr) {
    driver->coap_bind_addr = iot_data_alloc_string(bind_addr, IOT_DATA_COPY);
  } else {
    iot_log_error(lc, "CoAP bind address not in configuration");
    driver->coap_bind_addr = NULL;
    result = false;
  }

  iot_log_debug(lc, "Init complete");
  return result;
}

static bool coap_get_handler(void *impl, const devsdk_device_t *device,
                             uint32_t nreadings,
                             const devsdk_commandrequest *requests,
                             devsdk_commandresult *readings,
                             const iot_data_t *options,
                             iot_data_t **exception) {
  coap_driver *driver = (coap_driver *)impl;
  pthread_mutex_lock(&driver->mutex);
  bool successful_get_request = true;
  int ret = EXIT_FAILURE;
  uint32_t i = 0;
  if (device == NULL) {
    iot_log_error(driver->lc, "COAP:Device is empty");
    return successful_get_request;
  }
  end_dev_params *end_dev_params_ptr = (end_dev_params *)device->address;
  iot_log_debug(driver->lc, "COAP:Triggering Get events nreadings=%d\n",
                nreadings);
  /* The following requests and reading parameters are arrays of size nreadings
   */
  for (i = 0; i < nreadings; i++) {
    iot_log_debug(driver->lc, "COAP:Triggering Get events resource name=%s\n",
                  requests[i].resource->name);
    iot_log_debug(driver->lc, "COAP:Triggering Get events req type=%d\n",
                  requests[i].resource->type);
    ret = CoapGetRequestToEndDevice(device->name, requests[i].resource->name,
                                    end_dev_params_ptr, driver);
    if (ret == EXIT_FAILURE) {
      iot_log_error(driver->lc,
                    "COAP:Triggering Get events failed with ret=%d\n", ret);
      successful_get_request = false;
    } else {
      if (coap_resp_data != NULL) {
        readings[i].origin = 0;
        readings[i].value = coap_resp_data;
        iot_log_debug(driver->lc,
                      "COAP:Triggering Get events success with ret=%d\n", ret);
      } else {
        successful_get_request = false;
      }
    }
  }
  pthread_mutex_unlock(&driver->mutex);
  return successful_get_request;
}

static bool coap_put_handler(void *impl, const devsdk_device_t *device,
                             uint32_t nvalues,
                             const devsdk_commandrequest *requests,
                             const iot_data_t *values[],
                             const iot_data_t *options,
                             iot_data_t **exception) {
  (void)requests;

  coap_driver *driver = (coap_driver *)impl;
  pthread_mutex_lock(&driver->mutex);
  int len = 0;
  int ret = EXIT_SUCCESS;
  bool successful_get_request = true;
  char coap_put_data[FLOAT64_STR_MAXLEN + 1] = {0};
  char *coap_str_data = NULL;
  end_dev_params *end_dev_params_ptr = (end_dev_params *)device->address;
  iot_log_debug(driver->lc, "COAP:PUT on device:");
  iot_log_debug(driver->lc, "COAP: nvalues = %d", nvalues);

  for (uint32_t i = 0; i < nvalues; i++) {
    memset(coap_put_data, 0, FLOAT64_STR_MAXLEN + 1);
    switch (iot_data_type(values[i])) {
      case IOT_DATA_STRING: {
        coap_str_data = malloc(strlen(iot_data_string(values[i])) + 1);
        iot_log_debug(driver->lc, "  Value: %s", iot_data_string(values[i]));
        memcpy(coap_str_data, iot_data_string(values[i]),
               strlen(iot_data_string(values[i])) + 1);
        len = strlen(coap_str_data);
        break;
      }
      case IOT_DATA_INT32: {
        iot_log_debug(driver->lc, "  Value: %d", iot_data_i32(values[i]));
        sprintf(coap_put_data, "%d", iot_data_i32(values[i]));
        len = strlen(coap_put_data);
        break;
      }
      case IOT_DATA_FLOAT64: {
        iot_log_debug(driver->lc, "  Value: %0.6f", iot_data_f64(values[i]));
        sprintf(coap_put_data, "%0.6f", iot_data_f64(values[i]));
        len = strlen(coap_put_data);
        break;
      }
        /* etc etc */
      default:
        iot_log_debug(driver->lc, "  Value has unexpected type %s: %s",
                      iot_data_type_name(values[i]),
                      iot_data_to_json(values[i]));
        ret = EXIT_FAILURE;
    }
    if (ret == EXIT_FAILURE) {
      return false;
    }

    if (iot_data_type(values[i]) == IOT_DATA_STRING) {
      ret = CoapSendCommandToEndDevice((uint8_t *)coap_str_data, len,
                                       device->name, requests[i].resource->name,
                                       end_dev_params_ptr, driver);
      free(coap_str_data);
    } else {
      ret = CoapSendCommandToEndDevice((uint8_t *)coap_put_data, len,
                                       device->name, requests[i].resource->name,
                                       end_dev_params_ptr, driver);
    }
    if (ret == -1) {
      iot_log_error(driver->lc, "Sending data to End Device fails=%d\n", ret);
      return false;
    }
    successful_get_request = true;
  }
  pthread_mutex_unlock(&driver->mutex);
  return successful_get_request;
}

static void coap_stop(void *impl, bool force) {
  coap_driver *driver = (coap_driver *)impl;
  pthread_mutex_destroy(&driver->mutex);
}

static devsdk_address_t coap_create_address(void *impl,
                                            const devsdk_protocols *protocols,
                                            iot_data_t **exception) {
  bool res = false;
  coap_driver *driver = (coap_driver *)impl;
  end_dev_params *end_dev_params_ptr =
      (end_dev_params *)malloc(sizeof(end_dev_params));
  if (end_dev_params_ptr != NULL) {
    res = GetEndDeviceProtocolProperties(
        protocols, "COAP", exception, end_dev_params_ptr, (coap_driver *)impl);
    if (res == false) {
      iot_log_error(driver->lc, "COAP: protocol property for device is null");
    }
  }
  return (devsdk_address_t)end_dev_params_ptr;
}

static void coap_free_address(void *impl, devsdk_address_t address) {
  coap_driver *driver = (coap_driver *)impl;
  if (address != NULL) {
    free(address);
  } else {
    iot_log_error(driver->lc, "COAP: protocol address for device is null");
  }
}

static devsdk_resource_attr_t coap_create_resource_attr(
    void *impl, const iot_data_t *attributes, iot_data_t **exception) {
  return (devsdk_resource_attr_t)attributes;
}

static void coap_free_resource_attr(void *impl, devsdk_resource_attr_t attr) {}

int main(int argc, char *argv[]) {
  impl = malloc(sizeof(coap_driver));
  memset(impl, 0, sizeof(coap_driver));

  devsdk_error e;
  e.code = 0;

  /* Device Callbacks */
  devsdk_callbacks *coapImpls =
      devsdk_callbacks_init(coap_init, coap_get_handler, coap_put_handler,
                            coap_stop, coap_create_address, coap_free_address,
                            coap_create_resource_attr, coap_free_resource_attr);

  /* Initialize a new device service */
  devsdk_service_t *service = devsdk_service_new("device-coap", VERSION, impl,
                                                 coapImpls, &argc, argv, &e);
  ERR_CHECK(e);
  impl->service = service;

  int n = 1;
  while (n < argc) {
    if (strcmp(argv[n], "-h") == 0 || strcmp(argv[n], "--help") == 0) {
      printf("Options:\n");
      printf("  -h, --help\t\t\tShow this text\n");
      devsdk_usage();
      return 0;
    } else {
      printf("%s: Unrecognized option %s\n", argv[0], argv[n]);
      return 0;
    }
  }

  /* Create default Driver config and start the device service */
  iot_data_t *driver_map = iot_data_alloc_map(IOT_DATA_STRING);
  iot_data_string_map_add(driver_map, COAP_BIND_ADDR_KEY,
                          iot_data_alloc_string("0.0.0.0", IOT_DATA_REF));
  iot_data_string_map_add(driver_map, SECURITY_MODE_KEY,
                          iot_data_alloc_string("NoSec", IOT_DATA_REF));
  iot_data_string_map_add(driver_map, PSK_KEY_KEY,
                          iot_data_alloc_string("", IOT_DATA_REF));

  devsdk_service_start(service, driver_map, &e);
  ERR_CHECK(e);

  /* Run CoAP server */
  run_server();

  devsdk_service_stop(service, true, &e);
  ERR_CHECK(e);

  devsdk_service_free(service);
  iot_data_free(driver_map);
  iot_data_free(impl->coap_bind_addr);
  iot_data_free(impl->psk_key);
  free(impl);
  puts("Exiting gracefully");
  return 0;
}
