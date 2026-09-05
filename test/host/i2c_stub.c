#include "i2c_stub.h"
#include "stubs/driver/i2c_master.h"
#include <string.h>
#include <stdlib.h>

/* Buffer fixo (sticky): usado quando a FIFO esta vazia. */
static uint8_t   s_rx_buf[I2C_STUB_RX_MAX];
static size_t    s_rx_len;
static esp_err_t s_next_err;

/* FIFO de respostas enfileiradas. */
static uint8_t   s_fifo[I2C_STUB_FIFO_DEPTH][I2C_STUB_RX_MAX];
static size_t    s_fifo_len[I2C_STUB_FIFO_DEPTH];
static size_t    s_fifo_head;
static size_t    s_fifo_count;

/* Captura de escritas. */
uint8_t i2c_stub_last_tx[I2C_STUB_TX_MAX];
size_t  i2c_stub_last_tx_len;
uint8_t i2c_stub_tx_log[I2C_STUB_FIFO_DEPTH][I2C_STUB_TX_MAX];
size_t  i2c_stub_tx_log_len[I2C_STUB_FIFO_DEPTH];
size_t  i2c_stub_tx_count;

void i2c_stub_set_rx(const uint8_t *data, size_t len)
{
    if (len > sizeof(s_rx_buf)) len = sizeof(s_rx_buf);
    memcpy(s_rx_buf, data, len);
    s_rx_len = len;
}

void i2c_stub_push_rx(const uint8_t *data, size_t len)
{
    if (s_fifo_count >= I2C_STUB_FIFO_DEPTH) return; /* descarta silenciosamente */
    if (len > I2C_STUB_RX_MAX) len = I2C_STUB_RX_MAX;
    size_t slot = (s_fifo_head + s_fifo_count) % I2C_STUB_FIFO_DEPTH;
    memcpy(s_fifo[slot], data, len);
    s_fifo_len[slot] = len;
    s_fifo_count++;
}

void i2c_stub_set_error(esp_err_t err)
{
    s_next_err = err;
}

void i2c_stub_reset(void)
{
    memset(s_rx_buf, 0, sizeof(s_rx_buf));
    s_rx_len     = 0;
    s_next_err   = ESP_OK;
    s_fifo_head  = 0;
    s_fifo_count = 0;
    memset(s_fifo_len, 0, sizeof(s_fifo_len));
    memset(i2c_stub_last_tx, 0, sizeof(i2c_stub_last_tx));
    i2c_stub_last_tx_len = 0;
    memset(i2c_stub_tx_log, 0, sizeof(i2c_stub_tx_log));
    memset(i2c_stub_tx_log_len, 0, sizeof(i2c_stub_tx_log_len));
    i2c_stub_tx_count = 0;
}

/* --- helpers internos ------------------------------------------------------- */

static void capture_tx(const uint8_t *data, size_t len)
{
    if (len > I2C_STUB_TX_MAX) len = I2C_STUB_TX_MAX;
    memcpy(i2c_stub_last_tx, data, len);
    i2c_stub_last_tx_len = len;
    if (i2c_stub_tx_count < I2C_STUB_FIFO_DEPTH) {
        memcpy(i2c_stub_tx_log[i2c_stub_tx_count], data, len);
        i2c_stub_tx_log_len[i2c_stub_tx_count] = len;
    }
    i2c_stub_tx_count++;
}

static void deliver_rx(uint8_t *rx, size_t rx_len)
{
    const uint8_t *src;
    size_t src_len;
    if (s_fifo_count > 0) {
        src     = s_fifo[s_fifo_head];
        src_len = s_fifo_len[s_fifo_head];
        s_fifo_head = (s_fifo_head + 1) % I2C_STUB_FIFO_DEPTH;
        s_fifo_count--;
    } else {
        src     = s_rx_buf;
        src_len = s_rx_len;
    }
    size_t n = rx_len < src_len ? rx_len : src_len;
    memcpy(rx, src, n);
}

/* --- implementacoes das funcoes I2C stubadas -------------------------------- */

esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus,
                                    const i2c_device_config_t *cfg,
                                    i2c_master_dev_handle_t *dev)
{
    (void)bus; (void)cfg;
    *dev = (void *)0x1; /* handle ficticio, nao-nulo */
    return ESP_OK;
}

esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t dev)
{
    (void)dev;
    return ESP_OK;
}

esp_err_t i2c_master_transmit(i2c_master_dev_handle_t dev,
                              const uint8_t *data, size_t len, int timeout_ms)
{
    (void)dev; (void)timeout_ms;
    capture_tx(data, len);
    return s_next_err;
}

esp_err_t i2c_master_receive(i2c_master_dev_handle_t dev,
                             uint8_t *data, size_t len, int timeout_ms)
{
    (void)dev; (void)timeout_ms;
    if (s_next_err != ESP_OK) return s_next_err;
    deliver_rx(data, len);
    return ESP_OK;
}

esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t dev,
                                      const uint8_t *tx, size_t tx_len,
                                      uint8_t *rx, size_t rx_len, int timeout_ms)
{
    (void)dev; (void)timeout_ms;
    capture_tx(tx, tx_len);
    if (s_next_err != ESP_OK) return s_next_err;
    deliver_rx(rx, rx_len);
    return ESP_OK;
}
