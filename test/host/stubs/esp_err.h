#pragma once
#include <stdint.h>

typedef int32_t esp_err_t;

#define ESP_OK                  0
#define ESP_FAIL               -1
#define ESP_ERR_NO_MEM          0x00000101
#define ESP_ERR_INVALID_ARG     0x00000102
#define ESP_ERR_TIMEOUT         0x00000107
#define ESP_ERR_INVALID_CRC     0x00000108
#define ESP_ERR_INVALID_RESPONSE 0x00000109

static inline const char *esp_err_to_name(esp_err_t code)
{
    switch (code) {
    case ESP_OK:                  return "ESP_OK";
    case ESP_FAIL:                return "ESP_FAIL";
    case ESP_ERR_NO_MEM:          return "ESP_ERR_NO_MEM";
    case ESP_ERR_INVALID_ARG:     return "ESP_ERR_INVALID_ARG";
    case ESP_ERR_TIMEOUT:         return "ESP_ERR_TIMEOUT";
    case ESP_ERR_INVALID_CRC:     return "ESP_ERR_INVALID_CRC";
    case ESP_ERR_INVALID_RESPONSE:return "ESP_ERR_INVALID_RESPONSE";
    default:                      return "UNKNOWN";
    }
}
