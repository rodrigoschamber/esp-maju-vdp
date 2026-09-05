#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#include "mlx90614.h"
#include "amg8833.h"
#include "vpd.h"
#include "thermal.h"
#include "wifi_env.h"
#include "telemetry.h"
#include "telemetry_dispatch.h"
#include "telemetry_thingspeak.h"
#include "telemetry_mqtt.h"

static const char *TAG = "maju";

#define I2C_PORT            I2C_NUM_0
#define I2C_SDA_GPIO        CONFIG_MAJU_I2C_SDA_GPIO
#define I2C_SCL_GPIO        CONFIG_MAJU_I2C_SCL_GPIO
#define I2C_FREQ_HZ         CONFIG_MAJU_I2C_FREQ_HZ
#define I2C_PROBE_TIMEOUT_MS 200
#define SAMPLE_INTERVAL_MS  CONFIG_MAJU_SAMPLE_INTERVAL_MS
#define WIFI_SSID           MAJU_WIFI_SSID_ENV
#define WIFI_PASSWORD       MAJU_WIFI_PASSWORD_ENV

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define WIFI_MAX_RETRIES    10

#if CONFIG_MAJU_SHT35_ADDR_0X45
#define SHT35_ADDR          SHT3X_ADDR_HIGH
#define SHT35_ADDR_ALT      SHT3X_ADDR_LOW
#else
#define SHT35_ADDR          SHT3X_ADDR_LOW
#define SHT35_ADDR_ALT      SHT3X_ADDR_HIGH
#endif

#define MLX90614_ADDR       MLX90614_ADDR_DEFAULT

#if CONFIG_MAJU_AMG8833_ADDR_0X68
#define AMG8833_ADDR        AMG8833_ADDR_LOW
#define AMG8833_ADDR_ALT    AMG8833_ADDR_HIGH
#else
#define AMG8833_ADDR        AMG8833_ADDR_HIGH
#define AMG8833_ADDR_ALT    AMG8833_ADDR_LOW
#endif

static i2c_master_bus_handle_t s_bus;
static sht3x_handle_t          s_sht;
static mlx90614_handle_t       s_mlx;
static amg8833_handle_t        s_amg;
static EventGroupHandle_t      s_wifi_event_group;
static int                     s_wifi_retries;

/* Estaticos para nao pesar na stack da task principal (~330 B + ~260 B). */
static maju_reading_t  s_reading;
static amg8833_frame_t s_frame;

static const telemetry_backend_t *const s_backends[] = {
    &mqtt_backend,
    &thingspeak_backend,
    NULL,
};

/* ---------------------------------------------------------------------------
 * Wi-Fi
 * ------------------------------------------------------------------------- */

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

/* ---------------------------------------------------------------------------
 * I2C e sensores
 * ------------------------------------------------------------------------- */

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

/* Procura o sensor no endereco configurado e, se diferente, no alternativo. */
static esp_err_t probe_with_alt(const char *name, uint8_t addr, uint8_t alt, uint8_t *found)
{
    esp_err_t err = i2c_master_probe(s_bus, addr, I2C_PROBE_TIMEOUT_MS);
    if (err == ESP_OK) {
        *found = addr;
        return ESP_OK;
    }

    if (alt != addr) {
        esp_err_t alt_err = i2c_master_probe(s_bus, alt, I2C_PROBE_TIMEOUT_MS);
        if (alt_err == ESP_OK) {
            ESP_LOGW(TAG, "%s respondeu em 0x%02X (config atual usa 0x%02X).", name, alt, addr);
            *found = alt;
            return ESP_OK;
        }
        ESP_LOGE(TAG, "%s nao respondeu em 0x%02X nem 0x%02X (%s / %s).",
                 name, addr, alt, esp_err_to_name(err), esp_err_to_name(alt_err));
    } else {
        ESP_LOGE(TAG, "%s nao respondeu em 0x%02X (%s).", name, addr, esp_err_to_name(err));
    }

    ESP_LOGE(TAG, "Confira a fiacao (VDD=3V3, GND, SDA=GPIO%d, SCL=GPIO%d).",
             I2C_SDA_GPIO, I2C_SCL_GPIO);
    return err;
}

static esp_err_t sht_init(void)
{
    if (s_sht != NULL) {
        return ESP_OK;
    }

    uint8_t addr = SHT35_ADDR;
    esp_err_t err = probe_with_alt("SHT35", SHT35_ADDR, SHT35_ADDR_ALT, &addr);
    if (err != ESP_OK) {
        return err;
    }
    ESP_LOGI(TAG, "SHT35 encontrado no endereco 0x%02X", addr);

    err = sht3x_create(s_bus, addr, I2C_FREQ_HZ, &s_sht);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar handle do SHT35: %s", esp_err_to_name(err));
        return err;
    }

    err = sht3x_soft_reset(s_sht);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha no soft reset do SHT35: %s", esp_err_to_name(err));
        sht3x_delete(s_sht);
        s_sht = NULL;
        return err;
    }

    if (sht3x_heater_off(s_sht) != ESP_OK) {
        ESP_LOGW(TAG, "Falha ao desligar heater do SHT35 (seguindo mesmo assim)");
    }

    uint16_t status = 0;
    if (sht3x_read_status(s_sht, &status) == ESP_OK) {
        ESP_LOGI(TAG, "Status do SHT35: 0x%04X", status);
    }
    return ESP_OK;
}

static esp_err_t mlx_init(void)
{
    if (s_mlx != NULL) {
        return ESP_OK;
    }

    uint8_t addr = MLX90614_ADDR;
    esp_err_t err = probe_with_alt("MLX90614", MLX90614_ADDR, MLX90614_ADDR, &addr);
    if (err != ESP_OK) {
        return err;
    }
    ESP_LOGI(TAG, "MLX90614 encontrado no endereco 0x%02X", addr);

    err = mlx90614_create(s_bus, addr, I2C_FREQ_HZ, &s_mlx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar handle do MLX90614: %s", esp_err_to_name(err));
        return err;
    }

    float e = 0.0f;
    if (mlx90614_read_emissivity(s_mlx, &e) == ESP_OK) {
        ESP_LOGI(TAG, "Emissividade configurada no MLX90614: %.3f", e);
    } else {
        ESP_LOGW(TAG, "Nao foi possivel ler a emissividade do MLX90614 (seguindo mesmo assim)");
    }
    return ESP_OK;
}

static esp_err_t amg_init(void)
{
    if (s_amg != NULL) {
        return ESP_OK;
    }

    uint8_t addr = AMG8833_ADDR;
    esp_err_t err = probe_with_alt("AMG8833", AMG8833_ADDR, AMG8833_ADDR_ALT, &addr);
    if (err != ESP_OK) {
        return err;
    }
    ESP_LOGI(TAG, "AMG8833 encontrado no endereco 0x%02X (aguardando primeiro frame)", addr);

    err = amg8833_create(s_bus, addr, I2C_FREQ_HZ, &s_amg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar handle do AMG8833: %s", esp_err_to_name(err));
        return err;
    }

    err = amg8833_init(s_amg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha na inicializacao do AMG8833: %s", esp_err_to_name(err));
        amg8833_delete(s_amg);
        s_amg = NULL;
        return err;
    }
    return ESP_OK;
}

/* Os tres sensores sao obrigatorios: so retorna ESP_OK com todos os handles criados. */
static esp_err_t sensors_init_all(void)
{
    esp_err_t sht = sht_init();
    esp_err_t mlx = mlx_init();
    esp_err_t amg = amg_init();

    if (sht != ESP_OK || mlx != ESP_OK || amg != ESP_OK) {
        ESP_LOGW(TAG, "Sensores pendentes: %s%s%s- tentando novamente em %d ms.",
                 sht != ESP_OK ? "SHT35 " : "",
                 mlx != ESP_OK ? "MLX90614 " : "",
                 amg != ESP_OK ? "AMG8833 " : "",
                 SAMPLE_INTERVAL_MS);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t sht_read(float *t, float *rh)
{
    esp_err_t err = sht3x_measure_single(s_sht, SHT3X_REP_HIGH, t, rh);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha na leitura do SHT35: %s", esp_err_to_name(err));
        if (err == ESP_ERR_INVALID_RESPONSE) {
            ESP_LOGW(TAG, "Resposta I2C invalida; removendo handle do SHT35 para reinicializar.");
            sht3x_delete(s_sht);
            s_sht = NULL;
        }
    }
    return err;
}

/* Le MLX90614 e AMG8833, preenche th e escolhe a fonte da temperatura da folha. */
static void ir_sensors_read(thermal_t *th)
{
    thermal_reset(th);

    if (s_mlx != NULL) {
        esp_err_t err = mlx90614_read_object(s_mlx, &th->mlx_tobj_c);
        if (err == ESP_OK) {
            err = mlx90614_read_ambient(s_mlx, &th->mlx_ta_c);
        }
        if (err == ESP_OK) {
            th->mlx_ok = true;
        } else {
            ESP_LOGE(TAG, "Falha na leitura do MLX90614: %s", esp_err_to_name(err));
            if (err == ESP_ERR_INVALID_RESPONSE) {
                ESP_LOGW(TAG, "Resposta invalida; removendo handle do MLX90614 para reinicializar.");
                mlx90614_delete(s_mlx);
                s_mlx = NULL;
            }
        }
    }

    if (s_amg != NULL) {
        esp_err_t err = amg8833_read_frame(s_amg, &s_frame);
        if (err == ESP_OK) {
            memcpy(th->amg_px, s_frame.pixels, sizeof(th->amg_px));
            th->amg_therm_c = s_frame.thermistor_c;
            thermal_stats(th->amg_px, &th->amg_min_c, &th->amg_max_c, &th->amg_avg_c);
            th->amg_ok = true;
        } else {
            ESP_LOGE(TAG, "Falha na leitura do AMG8833: %s", esp_err_to_name(err));
            if (err == ESP_ERR_INVALID_RESPONSE) {
                ESP_LOGW(TAG, "Resposta invalida; removendo handle do AMG8833 para reinicializar.");
                amg8833_delete(s_amg);
                s_amg = NULL;
            }
        }
    }

    thermal_select_leaf(th);
}

/* ---------------------------------------------------------------------------
 * Saida no monitor serial
 * ------------------------------------------------------------------------- */

#if CONFIG_MAJU_PRINT_THERMAL_MAP
static void print_thermal_map(const float px[THERMAL_PIXELS])
{
    printf("  Mapa termico AMG8833 (C):\n");
    for (int row = 0; row < 8; row++) {
        printf("   ");
        for (int col = 0; col < 8; col++) {
            printf(" %5.1f", px[row * 8 + col]);
        }
        printf("\n");
    }
}
#endif

static void print_reading(const maju_reading_t *r)
{
    const thermal_t *th = &r->th;
    vpd_faixa_t faixa = vpd_classificar(r->v.vpd_folha);

    printf("\n");
    printf("+--------------------------------------------------------------+\n");
    printf("| LEITURA                                                      |\n");
    printf("+--------------------------------------------------------------+\n");
    printf("| Temperatura do ar .......... %6.2f  C                       |\n", r->t_ar);
    printf("| Umidade relativa ........... %6.2f  %%                       |\n", r->rh);
    printf("|                                                              |\n");
    printf("| SVP (ar) ................... %6.3f kPa                      |\n", r->v.svp_ar);
    printf("| AVP (vapor atual) .......... %6.3f kPa                      |\n", r->v.avp);
    printf("| VPD do ar .................. %6.3f kPa                      |\n", r->v.vpd_ar);
    printf("|                                                              |\n");
    printf("| Temp. da folha (IR %-4s) ... %6.2f  C                       |\n",
           thermal_source_str(th->fonte), r->v.t_folha_c);
    printf("| VPD da folha ............... %6.3f kPa                      |\n", r->v.vpd_folha);
    printf("|                                                              |\n");
    if (th->mlx_ok) {
        printf("| MLX90614 Tobj / Ta ......... %6.2f / %6.2f C               |\n",
               th->mlx_tobj_c, th->mlx_ta_c);
    } else {
        printf("| MLX90614 Tobj / Ta .........    ---  /    --- C               |\n");
    }
    if (th->amg_ok) {
        printf("| AMG8833 min/med/max ........ %6.2f/%6.2f/%6.2f C  th %5.1f |\n",
               th->amg_min_c, th->amg_avg_c, th->amg_max_c, th->amg_therm_c);
    } else {
        printf("| AMG8833 min/med/max ........    ---  /   ---  /   ---  C     |\n");
    }
    printf("+--------------------------------------------------------------+\n");
    printf("  Faixa: %s\n", vpd_faixa_str(faixa));

#if CONFIG_MAJU_PRINT_THERMAL_MAP
    if (th->amg_ok) {
        print_thermal_map(th->amg_px);
    }
#endif

    /* Linha compacta, no formato dos campos do ThingSpeak. */
    if (th->amg_ok) {
        ESP_LOGI(TAG, "field1=%.2f field2=%.2f field3=%.3f field4=%.3f field5=%.2f "
                      "field6=%.2f field7=%.2f field8=%.2f src=%s",
                 r->t_ar, r->rh, r->v.vpd_ar, r->v.vpd_folha, r->v.t_folha_c,
                 th->amg_min_c, th->amg_avg_c, th->amg_max_c, thermal_source_str(th->fonte));
    } else {
        ESP_LOGI(TAG, "field1=%.2f field2=%.2f field3=%.3f field4=%.3f field5=%.2f src=%s",
                 r->t_ar, r->rh, r->v.vpd_ar, r->v.vpd_folha, r->v.t_folha_c,
                 thermal_source_str(th->fonte));
    }
}

/* ---------------------------------------------------------------------------
 * app_main
 * ------------------------------------------------------------------------- */

void app_main(void)
{
    ESP_LOGI(TAG, "maju - sensor de VPD");
    ESP_LOGI(TAG, "Intervalo de leitura: %d ms | T folha: IR (MLX90614; fallback AMG8833)",
             SAMPLE_INTERVAL_MS);

    if (MAJU_THINGSPEAK_ENABLE_ENV) {
        ESP_LOGI(TAG, "ThingSpeak ativo: enviando para %s", MAJU_THINGSPEAK_URL_ENV);
    } else {
        ESP_LOGI(TAG, "ThingSpeak desativado (MAJU_THINGSPEAK_ENABLE=0 no .env).");
    }
    if (MAJU_MQTT_ENABLE_ENV) {
        ESP_LOGI(TAG, "MQTT ativo: broker=%s, topico=%s, termico=%s",
                 MAJU_MQTT_BROKER_URI_ENV, MAJU_MQTT_TOPIC_ENV, MAJU_MQTT_THERMAL_TOPIC_ENV);
    } else {
        ESP_LOGI(TAG, "MQTT desativado (MAJU_MQTT_ENABLE=0 no .env).");
    }

    wifi_init_sta();

    telemetry_dispatch_init(s_backends);

    i2c_bus_init();
    if (sensors_init_all() != ESP_OK) {
        ESP_LOGW(TAG, "Inicializacao dos sensores incompleta; tentando novamente no loop principal.");
    }

    while (true) {
        if (sensors_init_all() != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
            continue;
        }

        float t = 0.0f;
        float rh = 0.0f;
        if (sht_read(&t, &rh) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
            continue;
        }

        ir_sensors_read(&s_reading.th);

        if (s_reading.th.fonte == LEAF_SRC_NONE) {
            ESP_LOGE(TAG, "Sem temperatura de folha (MLX90614 e AMG8833 falharam); "
                          "leitura descartada neste ciclo.");
            vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
            continue;
        }
        if (s_reading.th.fonte == LEAF_SRC_AMG) {
            ESP_LOGW(TAG, "MLX90614 indisponivel; usando media do AMG8833 (%.2f C) como T folha.",
                     s_reading.th.amg_avg_c);
        }

        s_reading.ts_ms = esp_log_timestamp();
        s_reading.t_ar  = t;
        s_reading.rh    = rh;
        vpd_calculate_leaf(t, rh, s_reading.th.t_folha_c, &s_reading.v);

        print_reading(&s_reading);
        telemetry_dispatch_send(&s_reading);

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
    }
}
