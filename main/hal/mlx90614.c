/*
 * Driver minimo para o Melexis MLX90614 sobre o driver i2c_master do ESP-IDF.
 *
 * Referencia: datasheet MLX90614 (Melexis, 3901090614), secoes 8.4 (SMBus) e 8.7
 * (conversao de temperatura).
 *
 * Protocolo Read Word do SMBus: S, addr+W, comando, Sr, addr+R, LSB, MSB, PEC, P.
 * O i2c_master_transmit_receive emite o repeated start (Sr) entre a escrita e a leitura.
 */
#include <stdlib.h>
#include <string.h>
#include "esp_check.h"
#include "mlx90614.h"

static const char *TAG = "mlx90614";

#define MLX90614_TIMEOUT_MS         1000

/* Opcodes (datasheet sec. 8.4.5) */
#define MLX90614_CMD_RAM            0x00   /* 000x xxxx: acesso a RAM */
#define MLX90614_CMD_EEPROM         0x20   /* 001x xxxx: acesso a EEPROM */

/* RAM (datasheet sec. 8.4.6) */
#define MLX90614_REG_TA             (MLX90614_CMD_RAM | 0x06)     /* temperatura ambiente */
#define MLX90614_REG_TOBJ1          (MLX90614_CMD_RAM | 0x07)     /* temperatura do objeto 1 */

/* EEPROM (datasheet sec. 8.4.7) */
#define MLX90614_REG_EMISSIVITY     (MLX90614_CMD_EEPROM | 0x04)  /* 0x24, padrao 0xFFFF = 1.0 */

/* Conversao (datasheet sec. 8.7.1): T[K] = raw * 0.02; bit 15 = flag de erro */
#define MLX90614_TEMP_LSB_K         0.02f
#define MLX90614_KELVIN_OFFSET      273.15f
#define MLX90614_ERROR_FLAG         0x8000
#define MLX90614_EMISSIVITY_FULL    65535.0f

struct mlx90614_dev_t {
    i2c_master_dev_handle_t i2c_dev;
    uint8_t                 addr;   /* entra no calculo do PEC */
};

/* PEC do SMBus: CRC-8, polinomio 0x07, valor inicial 0x00, sem reflexao (sec. 8.4.4.1). */
static uint8_t mlx90614_pec(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* Read Word: devolve o valor de 16 bits (LSB primeiro no fio) apos validar o PEC. */
static esp_err_t mlx90614_read_word(mlx90614_handle_t dev, uint8_t cmd, uint16_t *word)
{
    uint8_t rx[3] = { 0 };

    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(dev->i2c_dev, &cmd, 1,
                                                    rx, sizeof(rx), MLX90614_TIMEOUT_MS),
                        TAG, "falha na leitura SMBus");

    /* O PEC cobre todos os bytes do quadro, inclusive os dois bytes de endereco. */
    uint8_t pec_in[5] = {
        (uint8_t)(dev->addr << 1),          /* addr + W */
        cmd,
        (uint8_t)((dev->addr << 1) | 1),    /* addr + R */
        rx[0],                              /* LSB */
        rx[1],                              /* MSB */
    };
    ESP_RETURN_ON_FALSE(mlx90614_pec(pec_in, sizeof(pec_in)) == rx[2], ESP_ERR_INVALID_CRC,
                        TAG, "PEC invalido");

    *word = (uint16_t)rx[0] | ((uint16_t)rx[1] << 8);
    return ESP_OK;
}

static esp_err_t mlx90614_read_temp(mlx90614_handle_t dev, uint8_t reg, float *c)
{
    ESP_RETURN_ON_FALSE(dev && c, ESP_ERR_INVALID_ARG, TAG, "argumento invalido");

    uint16_t raw = 0;
    ESP_RETURN_ON_ERROR(mlx90614_read_word(dev, reg, &raw), TAG, "falha ao ler temperatura");

    ESP_RETURN_ON_FALSE(!(raw & MLX90614_ERROR_FLAG), ESP_ERR_INVALID_RESPONSE, TAG,
                        "sensor sinalizou erro (bit 15)");

    *c = (float)raw * MLX90614_TEMP_LSB_K - MLX90614_KELVIN_OFFSET;
    return ESP_OK;
}

esp_err_t mlx90614_create(i2c_master_bus_handle_t bus, uint8_t dev_addr,
                          uint32_t scl_speed_hz, mlx90614_handle_t *out)
{
    ESP_RETURN_ON_FALSE(bus && out, ESP_ERR_INVALID_ARG, TAG, "argumento invalido");

    mlx90614_handle_t dev = calloc(1, sizeof(struct mlx90614_dev_t));
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

    dev->addr = dev_addr;
    *out = dev;
    return ESP_OK;
}

esp_err_t mlx90614_delete(mlx90614_handle_t dev)
{
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "handle nulo");
    esp_err_t err = i2c_master_bus_rm_device(dev->i2c_dev);
    free(dev);
    return err;
}

esp_err_t mlx90614_read_ambient(mlx90614_handle_t dev, float *c)
{
    return mlx90614_read_temp(dev, MLX90614_REG_TA, c);
}

esp_err_t mlx90614_read_object(mlx90614_handle_t dev, float *c)
{
    return mlx90614_read_temp(dev, MLX90614_REG_TOBJ1, c);
}

esp_err_t mlx90614_read_emissivity(mlx90614_handle_t dev, float *e)
{
    ESP_RETURN_ON_FALSE(dev && e, ESP_ERR_INVALID_ARG, TAG, "argumento invalido");

    uint16_t raw = 0;
    ESP_RETURN_ON_ERROR(mlx90614_read_word(dev, MLX90614_REG_EMISSIVITY, &raw), TAG,
                        "falha ao ler emissividade");

    *e = (float)raw / MLX90614_EMISSIVITY_FULL;
    return ESP_OK;
}
