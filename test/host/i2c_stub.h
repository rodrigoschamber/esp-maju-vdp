#pragma once
#include "stubs/esp_err.h"
#include <stdint.h>
#include <stddef.h>

#define I2C_STUB_RX_MAX      256   /* comporta o frame de 128 bytes do AMG8833 */
#define I2C_STUB_FIFO_DEPTH  8
#define I2C_STUB_TX_MAX      32

/* Injeta os bytes "fixos" que receive/transmit_receive devolvem quando a FIFO esta vazia. */
void i2c_stub_set_rx(const uint8_t *data, size_t len);

/* Enfileira uma resposta; consumida em ordem pelas proximas leituras (antes do buffer fixo). */
void i2c_stub_push_rx(const uint8_t *data, size_t len);

/* Forca o proximo retorno de erro em qualquer chamada I2C. */
void i2c_stub_set_error(esp_err_t err);

/* Reseta o estado do stub (erro = ESP_OK, buffers e historico vazios). */
void i2c_stub_reset(void);

/* Ultimos bytes escritos por transmit/transmit_receive (registrador/comando pedido). */
extern uint8_t i2c_stub_last_tx[I2C_STUB_TX_MAX];
extern size_t  i2c_stub_last_tx_len;

/* Historico das escritas desde o reset (para assertar sequencias de init). */
extern uint8_t i2c_stub_tx_log[I2C_STUB_FIFO_DEPTH][I2C_STUB_TX_MAX];
extern size_t  i2c_stub_tx_log_len[I2C_STUB_FIFO_DEPTH];
extern size_t  i2c_stub_tx_count;
