#include "i2c_stub.h"
#include "stubs/driver/i2c_master.h"
#include <string.h>
#include <stdlib.h>

static uint8_t   s_rx_buf[64];
static size_t    s_rx_len;
static esp_err_t s_next_err;

void i2c_stub_set_rx(const uint8_t *data, size_t len)
{
    memcpy(s_rx_buf, data, len);
    s_rx_len = len;
}

void i2c_stub_set_error(esp_err_t err)
{
    s_next_err = err;
}

void i2c_stub_reset(void)
{
    memset(s_rx_buf, 0, sizeof(s_rx_buf));
    s_rx_len   = 0;
    s_next_err = ESP_OK;
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
    (void)dev; (void)data; (void)len; (void)timeout_ms;
    return s_next_err;
}

esp_err_t i2c_master_receive(i2c_master_dev_handle_t dev,
                             uint8_t *data, size_t len, int timeout_ms)
{
    (void)dev; (void)timeout_ms;
    if (s_next_err != ESP_OK) return s_next_err;
    size_t n = len < s_rx_len ? len : s_rx_len;
    memcpy(data, s_rx_buf, n);
    return ESP_OK;
}

esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t dev,
                                      const uint8_t *tx, size_t tx_len,
                                      uint8_t *rx, size_t rx_len, int timeout_ms)
{
    (void)dev; (void)tx; (void)tx_len; (void)timeout_ms;
    if (s_next_err != ESP_OK) return s_next_err;
    size_t n = rx_len < s_rx_len ? rx_len : s_rx_len;
    memcpy(rx, s_rx_buf, n);
    return ESP_OK;
}
