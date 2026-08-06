#pragma once
#include "stubs/esp_err.h"
#include <stdint.h>
#include <stddef.h>

/* Injeta os bytes que o proximo i2c_master_receive/_transmit_receive vai retornar. */
void i2c_stub_set_rx(const uint8_t *data, size_t len);

/* Forca o proximo retorno de erro em qualquer chamada I2C. */
void i2c_stub_set_error(esp_err_t err);

/* Reseta o estado do stub (erro = ESP_OK, buffer vazio). */
void i2c_stub_reset(void);
