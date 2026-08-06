/*
 * Driver minimo para o Sensirion SHT3x-DIS sobre o driver i2c_master do ESP-IDF.
 *
 * Referencia: datasheet SHT3x-DIS (Sensirion), secoes de comandos e conversao.
 */
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "sht3x.h"

static const char *TAG = "sht3x";

#define SHT3X_TIMEOUT_MS        1000

/* Comandos de 16 bits (MSB primeiro) */
#define SHT3X_CMD_SOFT_RESET    0x30A2
#define SHT3X_CMD_HEATER_OFF    0x3066
#define SHT3X_CMD_READ_STATUS   0xF32D

/* Single shot, clock stretching desabilitado */
#define SHT3X_CMD_MEAS_HIGH     0x2400
#define SHT3X_CMD_MEAS_MEDIUM   0x240B
#define SHT3X_CMD_MEAS_LOW      0x2416

/* Tempo maximo de medicao por repetibilidade (datasheet: 15 / 6 / 4 ms) */
#define SHT3X_DURATION_HIGH_MS      15
#define SHT3X_DURATION_MEDIUM_MS    6
#define SHT3X_DURATION_LOW_MS       4

struct sht3x_dev_t {
    i2c_master_dev_handle_t i2c_dev;
};

/* CRC-8 da Sensirion: polinomio 0x31, valor inicial 0xFF. */
static uint8_t sht3x_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static esp_err_t sht3x_send_cmd(sht3x_handle_t dev, uint16_t cmd)
{
    uint8_t buf[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF) };
    return i2c_master_transmit(dev->i2c_dev, buf, sizeof(buf), SHT3X_TIMEOUT_MS);
}

esp_err_t sht3x_create(i2c_master_bus_handle_t bus, uint8_t dev_addr,
                       uint32_t scl_speed_hz, sht3x_handle_t *out)
{
    ESP_RETURN_ON_FALSE(bus && out, ESP_ERR_INVALID_ARG, TAG, "argumento invalido");

    sht3x_handle_t dev = calloc(1, sizeof(struct sht3x_dev_t));
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_NO_MEM, TAG, "sem memoria para o handle");

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = scl_speed_hz,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &dev->i2c_dev);
    if (err != ESP_OK) {
        free(dev);
        return err;
    }

    *out = dev;
    return ESP_OK;
}

esp_err_t sht3x_delete(sht3x_handle_t dev)
{
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "handle nulo");
    esp_err_t err = i2c_master_bus_rm_device(dev->i2c_dev);
    free(dev);
    return err;
}

esp_err_t sht3x_soft_reset(sht3x_handle_t dev)
{
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "handle nulo");
    esp_err_t err = sht3x_send_cmd(dev, SHT3X_CMD_SOFT_RESET);
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(5));   /* datasheet: reinicializacao em ate 1,5 ms */
    }
    return err;
}

esp_err_t sht3x_heater_off(sht3x_handle_t dev)
{
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "handle nulo");
    return sht3x_send_cmd(dev, SHT3X_CMD_HEATER_OFF);
}

esp_err_t sht3x_read_status(sht3x_handle_t dev, uint16_t *status)
{
    ESP_RETURN_ON_FALSE(dev && status, ESP_ERR_INVALID_ARG, TAG, "argumento invalido");

    uint8_t cmd[2] = { SHT3X_CMD_READ_STATUS >> 8, SHT3X_CMD_READ_STATUS & 0xFF };
    uint8_t rx[3] = { 0 };

    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(dev->i2c_dev, cmd, sizeof(cmd),
                                                    rx, sizeof(rx), SHT3X_TIMEOUT_MS),
                        TAG, "falha ao ler o status");

    ESP_RETURN_ON_FALSE(sht3x_crc8(rx, 2) == rx[2], ESP_ERR_INVALID_CRC, TAG,
                        "CRC do status invalido");

    *status = ((uint16_t)rx[0] << 8) | rx[1];
    return ESP_OK;
}

esp_err_t sht3x_measure_single(sht3x_handle_t dev, sht3x_repeatability_t rep,
                               float *temperature, float *humidity)
{
    ESP_RETURN_ON_FALSE(dev && temperature && humidity, ESP_ERR_INVALID_ARG, TAG,
                        "argumento invalido");

    uint16_t cmd;
    uint32_t wait_ms;
    switch (rep) {
    case SHT3X_REP_LOW:
        cmd = SHT3X_CMD_MEAS_LOW;
        wait_ms = SHT3X_DURATION_LOW_MS;
        break;
    case SHT3X_REP_MEDIUM:
        cmd = SHT3X_CMD_MEAS_MEDIUM;
        wait_ms = SHT3X_DURATION_MEDIUM_MS;
        break;
    case SHT3X_REP_HIGH:
    default:
        cmd = SHT3X_CMD_MEAS_HIGH;
        wait_ms = SHT3X_DURATION_HIGH_MS;
        break;
    }

    ESP_RETURN_ON_ERROR(sht3x_send_cmd(dev, cmd), TAG, "falha ao disparar a medicao");

    /* Sem clock stretching: espera a conversao terminar antes de ler. */
    vTaskDelay(pdMS_TO_TICKS(wait_ms + 5));

    uint8_t rx[6] = { 0 };
    ESP_RETURN_ON_ERROR(i2c_master_receive(dev->i2c_dev, rx, sizeof(rx), SHT3X_TIMEOUT_MS),
                        TAG, "falha ao ler a medicao");

    ESP_RETURN_ON_FALSE(sht3x_crc8(&rx[0], 2) == rx[2], ESP_ERR_INVALID_CRC, TAG,
                        "CRC da temperatura invalido");
    ESP_RETURN_ON_FALSE(sht3x_crc8(&rx[3], 2) == rx[5], ESP_ERR_INVALID_CRC, TAG,
                        "CRC da umidade invalido");

    uint16_t raw_t  = ((uint16_t)rx[0] << 8) | rx[1];
    uint16_t raw_rh = ((uint16_t)rx[3] << 8) | rx[4];

    /* Conversao do datasheet (SHT3x-DIS, secao 4.13) */
    *temperature = -45.0f + 175.0f * ((float)raw_t / 65535.0f);
    *humidity    = 100.0f * ((float)raw_rh / 65535.0f);

    return ESP_OK;
}
