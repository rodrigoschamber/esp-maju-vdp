/*
 * Driver minimo para o sensor termico Panasonic AMG8833 (Grid-EYE, matriz 8x8) em I2C,
 * usando o driver i2c_master do ESP-IDF.
 */
#pragma once

#include <stdbool.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AMG8833_ADDR_HIGH  0x69   /*!< AD_SELECT em VDD (padrao dos breakouts Adafruit/GY) */
#define AMG8833_ADDR_LOW   0x68   /*!< AD_SELECT em GND */
#define AMG8833_PIXELS     64     /*!< 8 linhas x 8 colunas */

/** Handle opaco do sensor. */
typedef struct amg8833_dev_t *amg8833_handle_t;

/**
 * Frame convertido para graus Celsius.
 *
 * pixels[] segue a ordem dos registradores (T01..T64): indice = linha * 8 + coluna.
 * Pixels: 12 bits em complemento de dois, 0,25 C/LSB (Grid-EYE Reference Specification).
 * Termistor: 12 bits em sinal-magnitude (bit 11 = sinal), 0,0625 C/LSB.
 * As duas convencoes coincidem para valores positivos, que e a faixa de uso.
 */
typedef struct {
    float pixels[AMG8833_PIXELS];
    float thermistor_c;
} amg8833_frame_t;

/**
 * @brief Adiciona o AMG8833 ao barramento I2C ja inicializado.
 *
 * @param bus          Handle do barramento I2C master.
 * @param dev_addr     AMG8833_ADDR_HIGH (0x69) ou AMG8833_ADDR_LOW (0x68).
 * @param scl_speed_hz Velocidade do barramento para este dispositivo.
 * @param[out] out     Handle criado.
 */
esp_err_t amg8833_create(i2c_master_bus_handle_t bus, uint8_t dev_addr,
                         uint32_t scl_speed_hz, amg8833_handle_t *out);

/** @brief Remove o dispositivo do barramento e libera o handle. */
esp_err_t amg8833_delete(amg8833_handle_t dev);

/**
 * @brief Sequencia de inicializacao: modo normal, reset inicial, 10 fps, interrupcao
 *        desligada. Bloqueia ~250 ms (power-up + primeiro frame valido).
 */
esp_err_t amg8833_init(amg8833_handle_t dev);

/**
 * @brief Seleciona a taxa de quadros.
 *
 * @param dev   Handle do sensor.
 * @param fps_1 true para 1 fps (menos ruido), false para 10 fps (padrao).
 */
esp_err_t amg8833_set_frame_rate(amg8833_handle_t dev, bool fps_1);

/**
 * @brief Le o termistor interno (registradores 0x0E/0x0F).
 *
 * @param dev    Handle do sensor.
 * @param[out] c Temperatura em graus Celsius.
 */
esp_err_t amg8833_read_thermistor(amg8833_handle_t dev, float *c);

/**
 * @brief Le os 64 pixels (0x80..0xFF, 128 bytes em uma unica transacao) e o termistor.
 *
 * @param dev        Handle do sensor.
 * @param[out] frame Frame convertido para graus Celsius.
 * @return ESP_OK ou erro do I2C.
 */
esp_err_t amg8833_read_frame(amg8833_handle_t dev, amg8833_frame_t *frame);

#ifdef __cplusplus
}
#endif
