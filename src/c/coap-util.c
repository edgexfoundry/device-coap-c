/* CoAP utils for device-coap-c
 *
 * Copyright (C) 2018 Olaf Bergmann <bergmann@tzi.org>
 * Copyright (c) 2020 Ken Bannister
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "coap-util.h"

#include <errno.h>
#include <netdb.h>

#include "device-coap.h"
static coap_driver *sdk_ctx;
/*
 * Builds libcoap address struct from host/port. Presently accepts only
 * internet addresses.
 */
int resolve_address(const char *host, const char *service,
                    coap_address_t *lib_addr) {
  struct addrinfo *res, *ainfo;
  struct addrinfo hints;
  int error, len = -1;
  sdk_ctx = impl;

  memset(&hints, 0, sizeof(hints));
  memset(lib_addr, 0, sizeof(*lib_addr));
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_family = AF_UNSPEC;

  error = getaddrinfo(host, service, &hints, &res);

  if (error != 0) {
    iot_log_info(sdk_ctx->lc, "getaddrinfo: %s\n", gai_strerror(error));
    return error;
  }

  for (ainfo = res; ainfo != NULL; ainfo = ainfo->ai_next) {
    /* Logic here allows for future non-IP addresses, but we don't accept them
     * yet. */
    if (ainfo->ai_addrlen <= sizeof(lib_addr->addr)) {
      switch (ainfo->ai_family) {
        case AF_INET6:
        case AF_INET:
          len = lib_addr->size = ainfo->ai_addrlen;
          memcpy(&lib_addr->addr.sa, ainfo->ai_addr, lib_addr->size);
          goto finish;
        default:;
      }
    }
  }

finish:
  freeaddrinfo(res);
  return len;
}

/* Caller must free returned iot_data_t */
iot_data_t *read_data_float64(uint8_t *data, size_t len) {
  sdk_ctx = impl;
  if (len > FLOAT64_STR_MAXLEN) {
    iot_log_info(sdk_ctx->lc, "invalid float64 of len %u", len);
    return NULL;
  }
  /* data conversion requires a null terminated string */
  uint8_t data_str[FLOAT64_STR_MAXLEN + 1];
  data_str[len] = 0;
  memcpy(data_str, data, len);

  char *endptr;
  errno = 0;
  double dbl_val = strtod((char *)data_str, &endptr);

  if (errno || (*endptr != '\0')) {
    iot_log_info(sdk_ctx->lc, "invalid float64 of len %u", len);
    return NULL;
  }

  return iot_data_alloc_f64(dbl_val);
}

/* Caller must free returned iot_data_t */
iot_data_t *read_data_int32(uint8_t *data, size_t len) {
  sdk_ctx = impl;
  if (len > INT32_STR_MAXLEN) {
    iot_log_info(sdk_ctx->lc, "invalid int32 of len %u", len);
    return NULL;
  }
  /* data conversion requires a null terminated string */
  uint8_t data_str[INT32_STR_MAXLEN + 1];
  data_str[len] = 0;
  memcpy(data_str, data, len);

  char *endptr;
  errno = 0;
  long int_val = strtol((char *)data_str, &endptr, 10);

  /* validate strtol conversion, and ensure within range */
  if (errno || (*endptr != '\0') || (int_val < INT32_MIN) ||
      (int_val > INT32_MAX)) {
    iot_log_info(sdk_ctx->lc, "invalid int32 of len %u", len);
    return NULL;
  }

  return iot_data_alloc_i32((int32_t)int_val);
}

/* Caller must free returned iot_data_t */
iot_data_t *read_data_string(uint8_t *data, size_t len) {
  /* must copy request data to append null terminator */
  char *str_data = malloc(len + 1);
  memcpy(str_data, data, len);
  str_data[len] = '\0';

  // iot_data_t *iot_data = iot_data_alloc_string(str_data, IOT_DATA_TAKE);
  iot_data_t *iot_data = iot_data_alloc_string(str_data, IOT_DATA_COPY);
  free(str_data);
  return iot_data;
}

static size_t uri_decode(const char *src, const size_t len, char *dst) {
  size_t i = 0, j = 0;
  sdk_ctx = impl;
  iot_log_debug(sdk_ctx->lc, "URI to decode = %s", src);
  while (src[i] != '\0') {
    int copy_char = 1;
    if (src[i] == '%' && i + 2 < len) {
      const unsigned char v1 = hexval[(unsigned char)src[i + 1]];
      const unsigned char v2 = hexval[(unsigned char)src[i + 2]];

      /* skip invalid hex sequences */
      if ((v1 | v2) != 0xFF) {
        dst[j] = (v1 << 4) | v2;
        j++;
        i += 3;
        copy_char = 0;
      }
    }
    if (copy_char) {
      dst[j] = src[i];
      i++;
      j++;
    }
  }
  dst[j] = '\0';
  iot_log_debug(sdk_ctx->lc, "URI is decoded as= %s", dst);
  return j;
}

/*
 * Parse URI path, expect 3 segments: /a1r/{device-name}/{resource-name}
 *
 * @param[in] request For path to parse
 * @param[out] device Found device
 * @param[out] resource Found resource for device
 * @return true if URI format OK, and device and resource found
 */
bool parse_path(coap_pdu_t *request, edgex_device **device_ptr,
                edgex_deviceresource **resource_ptr) {
  sdk_ctx = impl;
  coap_string_t *uri_path = coap_get_uri_path(request);
  iot_log_debug(sdk_ctx->lc, "URI %s", uri_path->s);
  char path_tmp[32] = {0};
  char path[32] = {0};
  strcpy(path_tmp, (char *)uri_path->s);
  iot_log_debug(sdk_ctx->lc, "URI %s", uri_path->s);
  edgex_device *device = NULL;
  edgex_deviceprofile *profile = NULL;
  edgex_deviceresource *resource = NULL;
  uri_decode(path_tmp, 16, path);
  iot_log_debug(sdk_ctx->lc, "URI decode%s", path);
  char *seg = strtok(path, "/");
  bool res = false;
  for (int i = 0; i < 3; i++) {
    if (!seg) {
      iot_log_info(sdk_ctx->lc, "missing URI segment %u", i);
      break;
    }

    switch (i) {
      case 0:
        if (strcmp(seg, RESOURCE_SEG1)) {
          iot_log_info(sdk_ctx->lc, "invalid URI; segment %u", i);
          goto end_for;
        }
        break;
      case 1:
        if (!(device = edgex_get_device_byname(sdk_ctx->service, seg))) {
          iot_log_info(sdk_ctx->lc, "device not found: %s", seg);
          goto end_for;
        }
        profile = device->profile;
        break;
      case 2:
        for (; profile; profile = profile->next) {
          resource = profile->device_resources;
          for (; resource; resource = resource->next) {
            if (!strcmp(resource->name, seg)) {
              break;
            }
          }
        }
        if (!resource) {
          iot_log_info(sdk_ctx->lc, "resource not found: %s", seg);
          goto end_for;
        }
        res = true;
        break;
    }
    seg = strtok(NULL, "/");
  }

end_for:
  if (res && seg) {
    iot_log_info(sdk_ctx->lc, "extra URI segment");
    res = false;
  }
  coap_delete_string(uri_path);

  if (res) {
    *device_ptr = device;
    *resource_ptr = resource;
  } else {
    edgex_free_device(sdk_ctx->service, device);
  }
  return res;
}

