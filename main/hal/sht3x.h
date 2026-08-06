/*
 * Driver minimo para o sensor Sensirion SHT3x-DIS (SHT30/SHT31/SHT35) em I2C,
 * usando o driver i2c_master do ESP-IDF.
 */
#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHT3X_ADDR_LOW   0x44   /*!< Pino ADDR em GND (padrao do planejamento) */
#define SHT3X_ADDR_HIGH  0x45   /*!< Pino ADDR em VDD */

/** Repetibilidade da medicao — alta para o melhor ruido/precisao. */
typedef enum {
    SHT3X_REP_LOW,
    SHT3X_REP_MEDIUM,
    SHT3X_REP_HIGH,
} sht3x_repeatability_t;

/** Handle opaco do sensor. */
typedef struct sht3x_dev_t *sht3x_handle_t;

/**
 * @brief Adiciona o SHT3x ao barramento I2C ja inicializado.
 *
 * @param bus          Handle do barramento I2C master.
 * @param dev_addr     SHT3X_ADDR_LOW (0x44) ou SHT3X_ADDR_HIGH (0x45).
 * @param scl_speed_hz Velocidade do barramento para este dispositivo.
 * @param[out] out     Handle criado.
 */
esp_err_t sht3x_create(i2c_master_bus_handle_t bus, uint8_t dev_addr,
                       uint32_t scl_speed_hz, sht3x_handle_t *out);

/** @brief Remove o dispositivo do barramento e libera o handle. */
esp_err_t sht3x_delete(sht3x_handle_t dev);

/** @brief Soft reset do sensor (comando 0x30A2). */
esp_err_t sht3x_soft_reset(sht3x_handle_t dev);

/** @brief Desliga o aquecedor interno (comando 0x3066). */
esp_err_t sht3x_heater_off(sht3x_handle_t dev);

/** @brief Le o registrador de status (comando 0xF32D), com verificacao de CRC. */
esp_err_t sht3x_read_status(sht3x_handle_t dev, uint16_t *status);

/**
 * @brief Faz uma medicao single-shot (sem clock stretching) e converte os valores.
 *
 * @param dev              Handle do sensor.
 * @param rep              Repetibilidade desejada.
 * @param[out] temperature Temperatura em °C.
 * @param[out] humidity    Umidade relativa em %.
 *
 * @return ESP_OK, ESP_ERR_INVALID_CRC se o CRC dos dados falhar, ou erro do I2C.
 */
esp_err_t sht3x_measure_single(sht3x_handle_t dev, sht3x_repeatability_t rep,
                               float *temperature, float *humidity);

#ifdef __cplusplus
}
#endif
