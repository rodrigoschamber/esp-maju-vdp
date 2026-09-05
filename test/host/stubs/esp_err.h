#pragma once
#include <stdint.h>

typedef int32_t esp_err_t;

/* Valores identicos aos de components/esp_common/include/esp_err.h do ESP-IDF. */
#define ESP_OK                   0
#define ESP_FAIL                -1
#define ESP_ERR_NO_MEM           0x00000101
#define ESP_ERR_INVALID_ARG      0x00000102
#define ESP_ERR_INVALID_STATE    0x00000103
#define ESP_ERR_INVALID_SIZE     0x00000104
#define ESP_ERR_NOT_FOUND        0x00000105
#define ESP_ERR_TIMEOUT          0x00000107
#define ESP_ERR_INVALID_RESPONSE 0x00000108
#define ESP_ERR_INVALID_CRC      0x00000109

static inline const char *esp_err_to_name(esp_err_t code)
{
    switch (code) {
    case ESP_OK:                  return "ESP_OK";
    case ESP_FAIL:                return "ESP_FAIL";
    case ESP_ERR_NO_MEM:          return "ESP_ERR_NO_MEM";
    case ESP_ERR_INVALID_ARG:     return "ESP_ERR_INVALID_ARG";
    case ESP_ERR_INVALID_STATE:   return "ESP_ERR_INVALID_STATE";
    case ESP_ERR_INVALID_SIZE:    return "ESP_ERR_INVALID_SIZE";
    case ESP_ERR_NOT_FOUND:       return "ESP_ERR_NOT_FOUND";
    case ESP_ERR_TIMEOUT:         return "ESP_ERR_TIMEOUT";
    case ESP_ERR_INVALID_RESPONSE:return "ESP_ERR_INVALID_RESPONSE";
    case ESP_ERR_INVALID_CRC:     return "ESP_ERR_INVALID_CRC";
    default:                      return "UNKNOWN";
    }
}
