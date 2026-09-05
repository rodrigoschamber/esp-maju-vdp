/*
 * Driver minimo para o termometro infravermelho Melexis MLX90614 (breakout GY-614V3)
 * em SMBus, usando o driver i2c_master do ESP-IDF.
 *
 * O sensor precisa de ~250 ms apos a alimentacao antes da primeira leitura (datasheet
 * sec. 8.3); o chamador deve garantir esse intervalo. Frequencia maxima do SMBus: 100 kHz.
 */
#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MLX90614_ADDR_DEFAULT  0x5A   /*!< Endereco de fabrica (EEPROM SMBus address = 0x5A) */

/** Handle opaco do sensor. */
typedef struct mlx90614_dev_t *mlx90614_handle_t;

/**
 * @brief Adiciona o MLX90614 ao barramento I2C ja inicializado.
 *
 * @param bus          Handle do barramento I2C master.
 * @param dev_addr     Endereco de 7 bits (MLX90614_ADDR_DEFAULT = 0x5A).
 * @param scl_speed_hz Velocidade do barramento para este dispositivo (<= 100 kHz).
 * @param[out] out     Handle criado.
 */
esp_err_t mlx90614_create(i2c_master_bus_handle_t bus, uint8_t dev_addr,
                          uint32_t scl_speed_hz, mlx90614_handle_t *out);

/** @brief Remove o dispositivo do barramento e libera o handle. */
esp_err_t mlx90614_delete(mlx90614_handle_t dev);

/**
 * @brief Le a temperatura ambiente do encapsulamento (RAM 0x06, Ta).
 *
 * @param dev    Handle do sensor.
 * @param[out] c Temperatura em graus Celsius.
 * @return ESP_OK, ESP_ERR_INVALID_CRC se o PEC falhar, ESP_ERR_INVALID_RESPONSE se o
 *         sensor sinalizar erro (bit 15), ou erro do I2C.
 */
esp_err_t mlx90614_read_ambient(mlx90614_handle_t dev, float *c);

/**
 * @brief Le a temperatura do objeto no campo de visao (RAM 0x07, Tobj1).
 *
 * @param dev    Handle do sensor.
 * @param[out] c Temperatura em graus Celsius.
 * @return ESP_OK, ESP_ERR_INVALID_CRC se o PEC falhar, ESP_ERR_INVALID_RESPONSE se o
 *         sensor sinalizar erro (bit 15), ou erro do I2C.
 */
esp_err_t mlx90614_read_object(mlx90614_handle_t dev, float *c);

/**
 * @brief Le a emissividade configurada na EEPROM (0x24). Padrao de fabrica: 1.0.
 *
 * @param dev    Handle do sensor.
 * @param[out] e Emissividade em [0, 1] (raw / 65535).
 * @return ESP_OK, ESP_ERR_INVALID_CRC se o PEC falhar, ou erro do I2C.
 */
esp_err_t mlx90614_read_emissivity(mlx90614_handle_t dev, float *e);

#ifdef __cplusplus
}
#endif
