#include <stdio.h>
#include <stdlib.h>

/* Evita que o IntelliSense no macOS siga headers Mach-O no contexto ESP32. */
#if defined(__INTELLISENSE__)
#ifdef __APPLE__
#undef __APPLE__
#endif
#ifdef __MACH__
#undef __MACH__
#endif
#endif

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "driver/i2c_master.h"

#include "sht3x.h"
#include "vpd.h"
#include "wifi_env.h"
#include "telemetry_thingspeak.h"

static const char *TAG = "maju";

#define I2C_PORT            I2C_NUM_0
#define I2C_SDA_GPIO        CONFIG_MAJU_I2C_SDA_GPIO
#define I2C_SCL_GPIO        CONFIG_MAJU_I2C_SCL_GPIO
#define I2C_FREQ_HZ         CONFIG_MAJU_I2C_FREQ_HZ
#define SAMPLE_INTERVAL_MS  CONFIG_MAJU_SAMPLE_INTERVAL_MS
#define LEAF_OFFSET_C       MAJU_LEAF_OFFSET_C_ENV
#define WIFI_SSID           MAJU_WIFI_SSID_ENV
#define WIFI_PASSWORD       MAJU_WIFI_PASSWORD_ENV

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define WIFI_MAX_RETRIES    10

#if CONFIG_MAJU_SHT35_ADDR_0X45
#define SHT35_ADDR          SHT3X_ADDR_HIGH
#else
#define SHT35_ADDR          SHT3X_ADDR_LOW
#endif

static i2c_master_bus_handle_t s_bus;
static sht3x_handle_t s_sensor;
static uint8_t s_sensor_addr = SHT35_ADDR;
static EventGroupHandle_t s_wifi_event_group;
static int s_wifi_retries;
static const telemetry_backend_t *s_telemetry = &thingspeak_backend;

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_wifi_retries < WIFI_MAX_RETRIES) {
            s_wifi_retries++;
            ESP_LOGW(TAG, "Wi-Fi desconectou; tentando reconectar (%d/%d)",
                     s_wifi_retries, WIFI_MAX_RETRIES);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Wi-Fi conectado. IP=" IPSTR, IP2STR(&event->ip_info.ip));
        s_wifi_retries = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", WIFI_SSID);
    snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", WIFI_PASSWORD);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Conectando no Wi-Fi SSID: %s", WIFI_SSID);

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(15000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi pronto para uso.");
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGW(TAG, "Nao foi possivel conectar no Wi-Fi apos %d tentativas.", WIFI_MAX_RETRIES);
    } else {
        ESP_LOGW(TAG, "Timeout na conexao Wi-Fi; seguindo e deixando reconexao em background.");
    }
}

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

static esp_err_t sensor_probe_addr(uint8_t *addr)
{
    esp_err_t err = i2c_master_probe(s_bus, SHT35_ADDR, 200);
    if (err == ESP_OK) {
        *addr = SHT35_ADDR;
        return ESP_OK;
    }

    uint8_t alt_addr = (SHT35_ADDR == SHT3X_ADDR_LOW) ? SHT3X_ADDR_HIGH : SHT3X_ADDR_LOW;
    esp_err_t alt_err = i2c_master_probe(s_bus, alt_addr, 200);
    if (alt_err == ESP_OK) {
        ESP_LOGW(TAG, "Sensor respondeu em 0x%02X (config atual usa 0x%02X).", alt_addr, SHT35_ADDR);
        *addr = alt_addr;
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Nenhum sensor respondeu em 0x%02X nem 0x%02X (%s / %s).",
             SHT35_ADDR, alt_addr, esp_err_to_name(err), esp_err_to_name(alt_err));
    ESP_LOGE(TAG, "Confira a fiacao (VDD=3V3, GND, SDA=GPIO%d, SCL=GPIO%d) e o pino ADDR.",
             I2C_SDA_GPIO, I2C_SCL_GPIO);

    return err;
}

static esp_err_t sensor_init(void)
{
    if (s_sensor != NULL) {
        return ESP_OK;
    }

    esp_err_t err = sensor_probe_addr(&s_sensor_addr);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "SHT35 encontrado no endereco 0x%02X", s_sensor_addr);

    err = sht3x_create(s_bus, s_sensor_addr, I2C_FREQ_HZ, &s_sensor);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar handle do SHT35: %s", esp_err_to_name(err));
        return err;
    }

    err = sht3x_soft_reset(s_sensor);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha no soft reset do SHT35: %s", esp_err_to_name(err));
        sht3x_delete(s_sensor);
        s_sensor = NULL;
        return err;
    }

    err = sht3x_heater_off(s_sensor);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Falha ao desligar heater do SHT35: %s (seguindo mesmo assim)",
                 esp_err_to_name(err));
    }

    uint16_t status = 0;
    if (sht3x_read_status(s_sensor, &status) == ESP_OK) {
        ESP_LOGI(TAG, "Status do sensor: 0x%04X", status);
    } else {
        ESP_LOGW(TAG, "Nao foi possivel ler o status do sensor na inicializacao.");
    }

    return ESP_OK;
}

static void print_reading(float t, float rh, const vpd_result_t *v)
{
    vpd_faixa_t faixa = vpd_classificar(v->vpd_folha);

    printf("\n");
    printf("+--------------------------------------------------------------+\n");
    printf("| LEITURA                                             |\n");
    printf("+--------------------------------------------------------------+\n");
    printf("| Temperatura do ar .......... %6.2f  C                       |\n", t);
    printf("| Umidade relativa ........... %6.2f  %%                       |\n", rh);
    printf("|                                                              |\n");
    printf("| SVP (ar) ................... %6.3f kPa                      |\n", v->svp_ar);
    printf("| AVP (vapor atual) .......... %6.3f kPa                      |\n", v->avp);
    printf("| VPD do ar .................. %6.3f kPa                      |\n", v->vpd_ar);
    printf("|                                                              |\n");
    printf("| Temp. da folha (%+.1f C) .... %6.2f  C                       |\n", LEAF_OFFSET_C, v->t_folha_c);
    printf("| VPD da folha ............... %6.3f kPa                      |\n", v->vpd_folha);
    printf("+--------------------------------------------------------------+\n");
    printf("  Faixa: %s\n", vpd_faixa_str(faixa));

    /* Linha compacta, no formato dos campos do ThingSpeak (proxima etapa). */
    ESP_LOGI(TAG, "field1=%.2f field2=%.2f field3=%.3f field4=%.3f",
             t, rh, v->vpd_ar, v->vpd_folha);
}

void app_main(void)
{
    ESP_LOGI(TAG, "maju - sensor de VPD");
    ESP_LOGI(TAG, "Intervalo de leitura: %d ms | offset de folha: %+.1f C",
             SAMPLE_INTERVAL_MS, LEAF_OFFSET_C);

    if (MAJU_THINGSPEAK_ENABLE_ENV) {
        ESP_LOGI(TAG, "ThingSpeak ativo: enviando para %s", MAJU_THINGSPEAK_URL_ENV);
    } else {
        ESP_LOGI(TAG, "ThingSpeak desativado (MAJU_THINGSPEAK_ENABLE=0 no .env).");
    }

    wifi_init_sta();

    i2c_bus_init();
    if (sensor_init() != ESP_OK) {
        ESP_LOGW(TAG, "Inicializacao do sensor falhou; tentando novamente no loop principal.");
    }

    while (true) {
        if (s_sensor == NULL) {
            if (sensor_init() != ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
                continue;
            }
        }

        float t = 0.0f;
        float rh = 0.0f;

        esp_err_t err = sht3x_measure_single(s_sensor, SHT3X_REP_HIGH, &t, &rh);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Falha na leitura do SHT35: %s", esp_err_to_name(err));
            if (err == ESP_ERR_INVALID_RESPONSE) {
                ESP_LOGW(TAG, "Resposta I2C invalida; removendo handle para tentar reinicializar.");
                sht3x_delete(s_sensor);
                s_sensor = NULL;
            }
        } else {
            vpd_result_t v;
            vpd_calculate(t, rh, LEAF_OFFSET_C, &v);
            print_reading(t, rh, &v);
            s_telemetry->send(t, rh, &v);
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
    }
}
