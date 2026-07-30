/*
 * esp-maju-vdp — Sensor de VPD para estufa
 *
 * Etapa atual (MVP fase 3, parte local): le temperatura e umidade do SHT35 via
 * I2C, calcula o VPD do ar e o VPD da folha e exibe tudo no monitor serial.
 * O envio ao ThingSpeak entra em uma etapa posterior.
 *
 * Placa: Espressif ESP32-DevKitC V4 (modulo ESP32-WROOM-32UE)
 * Sensor: Sensirion SHT35 em I2C — SDA=GPIO21, SCL=GPIO22, ADDR=GND (0x44)
 */
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#include "sht3x.h"
#include "vpd.h"

static const char *TAG = "estufa";

#define I2C_PORT            I2C_NUM_0
#define I2C_SDA_GPIO        CONFIG_MAJU_I2C_SDA_GPIO
#define I2C_SCL_GPIO        CONFIG_MAJU_I2C_SCL_GPIO
#define I2C_FREQ_HZ         CONFIG_MAJU_I2C_FREQ_HZ
#define SAMPLE_INTERVAL_MS  CONFIG_MAJU_SAMPLE_INTERVAL_MS
#define LEAF_OFFSET_C       (CONFIG_MAJU_LEAF_OFFSET_DECI_C / 10.0f)

#if CONFIG_MAJU_SHT35_ADDR_0X45
#define SHT35_ADDR          SHT3X_ADDR_HIGH
#else
#define SHT35_ADDR          SHT3X_ADDR_LOW
#endif

static i2c_master_bus_handle_t s_bus;
static sht3x_handle_t s_sensor;

static void i2c_bus_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_PORT,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_bus));
    ESP_LOGI(TAG, "Barramento I2C pronto (SDA=GPIO%d, SCL=GPIO%d, %d Hz)",
             I2C_SDA_GPIO, I2C_SCL_GPIO, I2C_FREQ_HZ);
}

static void sensor_init(void)
{
    esp_err_t err = i2c_master_probe(s_bus, SHT35_ADDR, 200);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Nenhum sensor respondeu em 0x%02X (%s).", SHT35_ADDR, esp_err_to_name(err));
        ESP_LOGE(TAG, "Confira a fiacao (VDD=3V3, GND, SDA=GPIO%d, SCL=GPIO%d) e o pino ADDR.",
                 I2C_SDA_GPIO, I2C_SCL_GPIO);
    } else {
        ESP_LOGI(TAG, "SHT35 encontrado no endereco 0x%02X", SHT35_ADDR);
    }

    ESP_ERROR_CHECK(sht3x_create(s_bus, SHT35_ADDR, I2C_FREQ_HZ, &s_sensor));
    ESP_ERROR_CHECK(sht3x_soft_reset(s_sensor));
    ESP_ERROR_CHECK(sht3x_heater_off(s_sensor));

    uint16_t status = 0;
    if (sht3x_read_status(s_sensor, &status) == ESP_OK) {
        ESP_LOGI(TAG, "Status do sensor: 0x%04X", status);
    }
}

static void print_reading(float t, float rh, const vpd_result_t *v)
{
    vpd_faixa_t faixa = vpd_classificar(v->vpd_folha);

    printf("\n");
    printf("+--------------------------------------------------------------+\n");
    printf("| ESTUFA - LEITURA                                             |\n");
    printf("+--------------------------------------------------------------+\n");
    printf("| Temperatura do ar .......... %6.2f  C                       |\n", t);
    printf("| Umidade relativa ........... %6.2f  %%                       |\n", rh);
    printf("|                                                              |\n");
    printf("| SVP (ar) ................... %6.3f kPa                      |\n", v->svp_ar);
    printf("| AVP (vapor atual) .......... %6.3f kPa                      |\n", v->avp);
    printf("| VPD do ar .................. %6.3f kPa                      |\n", v->vpd_ar);
    printf("|                                                              |\n");
    printf("| Temp. da folha (-%.1f C) .... %6.2f  C                       |\n", LEAF_OFFSET_C, v->t_folha_c);
    printf("| VPD da folha ............... %6.3f kPa                      |\n", v->vpd_folha);
    printf("+--------------------------------------------------------------+\n");
    printf("  Faixa: %s\n", vpd_faixa_str(faixa));

    /* Linha compacta, no formato dos campos do ThingSpeak (proxima etapa). */
    ESP_LOGI(TAG, "field1=%.2f field2=%.2f field3=%.3f field4=%.3f",
             t, rh, v->vpd_ar, v->vpd_folha);
}

void app_main(void)
{
    ESP_LOGI(TAG, "esp-maju-vdp - sensor de VPD para estufa");
    ESP_LOGI(TAG, "Intervalo de leitura: %d ms | offset de folha: %.1f C",
             SAMPLE_INTERVAL_MS, LEAF_OFFSET_C);

    i2c_bus_init();
    sensor_init();

    while (true) {
        float t = 0.0f;
        float rh = 0.0f;

        esp_err_t err = sht3x_measure_single(s_sensor, SHT3X_REP_HIGH, &t, &rh);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Falha na leitura do SHT35: %s", esp_err_to_name(err));
        } else {
            vpd_result_t v;
            vpd_calculate(t, rh, LEAF_OFFSET_C, &v);
            print_reading(t, rh, &v);
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
    }
}
