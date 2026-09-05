/*
 * Driver minimo para o Panasonic AMG8833 (Grid-EYE) sobre o driver i2c_master do ESP-IDF.
 *
 * Referencia: Grid-EYE Reference Specification (Panasonic AMG88xx), mapa de registradores
 * e secao "Temperature register".
 */
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "amg8833.h"

static const char *TAG = "amg8833";

#define AMG8833_TIMEOUT_MS          1000

/* Registradores */
#define AMG8833_REG_PCTL            0x00  /* Power control */
#define AMG8833_REG_RST             0x01  /* Reset */
#define AMG8833_REG_FPSC            0x02  /* Frame rate */
#define AMG8833_REG_INTC            0x03  /* Interrupt control */
#define AMG8833_REG_TTHL            0x0E  /* Termistor LSB (0x0F = MSB) */
#define AMG8833_REG_T01L            0x80  /* Primeiro pixel (LSB); auto-incremento ate 0xFF */

/* Valores */
#define AMG8833_PCTL_NORMAL         0x00
#define AMG8833_RST_INITIAL         0x3F
#define AMG8833_FPSC_10FPS          0x00
#define AMG8833_FPSC_1FPS           0x01
#define AMG8833_INTC_DISABLED       0x00

/* Conversao */
#define AMG8833_PIXEL_LSB_C         0.25f
#define AMG8833_THERM_LSB_C         0.0625f
#define AMG8833_RAW_MASK            0x0FFF
#define AMG8833_RAW_SIGN_BIT        0x0800
#define AMG8833_FRAME_BYTES         (AMG8833_PIXELS * 2)   /* 128 */

/* Temporizacao (secao "Power-on sequence") */
#define AMG8833_POWER_UP_MS         50
#define AMG8833_RESET_SETTLE_MS     2
#define AMG8833_FIRST_FRAME_MS      200   /* 2 periodos de quadro a 10 fps */

struct amg8833_dev_t {
    i2c_master_dev_handle_t i2c_dev;
};

/* Pixel: 12 bits em complemento de dois, 0,25 C por LSB. Bits 15..12 sao ignorados. */
static float amg8833_pixel_to_c(uint16_t raw)
{
    int16_t v = (int16_t)(raw & AMG8833_RAW_MASK);
    if (v & AMG8833_RAW_SIGN_BIT) {
        v -= 0x1000;
    }
    return (float)v * AMG8833_PIXEL_LSB_C;
}

/* Termistor: 11 bits de magnitude + bit 11 de sinal, 0,0625 C por LSB. */
static float amg8833_therm_to_c(uint16_t raw)
{
    float mag = (float)(raw & 0x07FF) * AMG8833_THERM_LSB_C;
    return (raw & AMG8833_RAW_SIGN_BIT) ? -mag : mag;
}

static esp_err_t amg8833_write_reg(amg8833_handle_t dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(dev->i2c_dev, buf, sizeof(buf), AMG8833_TIMEOUT_MS);
}

static esp_err_t amg8833_read_regs(amg8833_handle_t dev, uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(dev->i2c_dev, &reg, 1, buf, len, AMG8833_TIMEOUT_MS);
}

esp_err_t amg8833_create(i2c_master_bus_handle_t bus, uint8_t dev_addr,
                         uint32_t scl_speed_hz, amg8833_handle_t *out)
{
    ESP_RETURN_ON_FALSE(bus && out, ESP_ERR_INVALID_ARG, TAG, "argumento invalido");

    amg8833_handle_t dev = calloc(1, sizeof(struct amg8833_dev_t));
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

esp_err_t amg8833_delete(amg8833_handle_t dev)
{
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "handle nulo");
    esp_err_t err = i2c_master_bus_rm_device(dev->i2c_dev);
    free(dev);
    return err;
}

esp_err_t amg8833_init(amg8833_handle_t dev)
{
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "handle nulo");

    vTaskDelay(pdMS_TO_TICKS(AMG8833_POWER_UP_MS));

    ESP_RETURN_ON_ERROR(amg8833_write_reg(dev, AMG8833_REG_PCTL, AMG8833_PCTL_NORMAL),
                        TAG, "falha ao entrar em modo normal");
    ESP_RETURN_ON_ERROR(amg8833_write_reg(dev, AMG8833_REG_RST, AMG8833_RST_INITIAL),
                        TAG, "falha no reset inicial");
    vTaskDelay(pdMS_TO_TICKS(AMG8833_RESET_SETTLE_MS));

    ESP_RETURN_ON_ERROR(amg8833_write_reg(dev, AMG8833_REG_FPSC, AMG8833_FPSC_10FPS),
                        TAG, "falha ao configurar frame rate");
    ESP_RETURN_ON_ERROR(amg8833_write_reg(dev, AMG8833_REG_INTC, AMG8833_INTC_DISABLED),
                        TAG, "falha ao desligar interrupcao");

    /* Os primeiros quadros apos o reset saem zerados; aguarda dois periodos. */
    vTaskDelay(pdMS_TO_TICKS(AMG8833_FIRST_FRAME_MS));
    return ESP_OK;
}

esp_err_t amg8833_set_frame_rate(amg8833_handle_t dev, bool fps_1)
{
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "handle nulo");
    return amg8833_write_reg(dev, AMG8833_REG_FPSC,
                             fps_1 ? AMG8833_FPSC_1FPS : AMG8833_FPSC_10FPS);
}

esp_err_t amg8833_read_thermistor(amg8833_handle_t dev, float *c)
{
    ESP_RETURN_ON_FALSE(dev && c, ESP_ERR_INVALID_ARG, TAG, "argumento invalido");

    uint8_t rx[2] = { 0 };
    ESP_RETURN_ON_ERROR(amg8833_read_regs(dev, AMG8833_REG_TTHL, rx, sizeof(rx)),
                        TAG, "falha ao ler o termistor");

    uint16_t raw = (uint16_t)rx[0] | ((uint16_t)rx[1] << 8);
    *c = amg8833_therm_to_c(raw);
    return ESP_OK;
}

esp_err_t amg8833_read_frame(amg8833_handle_t dev, amg8833_frame_t *frame)
{
    ESP_RETURN_ON_FALSE(dev && frame, ESP_ERR_INVALID_ARG, TAG, "argumento invalido");

    /* 128 bytes em uma transacao: o driver do IDF fatia em recargas da FIFO sem STOP. */
    uint8_t raw[AMG8833_FRAME_BYTES] = { 0 };
    ESP_RETURN_ON_ERROR(amg8833_read_regs(dev, AMG8833_REG_T01L, raw, sizeof(raw)),
                        TAG, "falha ao ler o frame");

    for (int i = 0; i < AMG8833_PIXELS; i++) {
        uint16_t px = (uint16_t)raw[2 * i] | ((uint16_t)raw[2 * i + 1] << 8);  /* LSB primeiro */
        frame->pixels[i] = amg8833_pixel_to_c(px);
    }

    return amg8833_read_thermistor(dev, &frame->thermistor_c);
}
