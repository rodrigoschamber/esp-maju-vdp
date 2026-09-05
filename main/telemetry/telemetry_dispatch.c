#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "telemetry_dispatch.h"

static const char *TAG = "dispatch";

#define DISPATCH_QUEUE_SIZE    4
#define DISPATCH_TASK_STACK    8192
#define DISPATCH_TASK_PRIORITY (tskIDLE_PRIORITY + 2)
#define DISPATCH_MAX_BACKENDS  8

/* Item da fila: copia completa da leitura (inclui o frame termico de 64 pixels). */
typedef maju_reading_t dispatch_msg_t;

typedef struct {
    const telemetry_backend_t *backend;
    QueueHandle_t              queue;
    TaskHandle_t               task;
} backend_slot_t;

static backend_slot_t s_slots[DISPATCH_MAX_BACKENDS];
static int            s_count;

static void backend_task(void *arg)
{
    backend_slot_t *slot = (backend_slot_t *)arg;
    dispatch_msg_t  msg;
    for (;;) {
        if (xQueueReceive(slot->queue, &msg, portMAX_DELAY) == pdTRUE) {
            slot->backend->send(&msg);
        }
    }
}

esp_err_t telemetry_dispatch_init(const telemetry_backend_t *const *backends)
{
    s_count = 0;
    for (int i = 0; backends[i] != NULL && i < DISPATCH_MAX_BACKENDS; i++) {
        esp_err_t err = backends[i]->init();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "backend[%d] init falhou: %s", i, esp_err_to_name(err));
        }

        s_slots[i].backend = backends[i];
        s_slots[i].queue   = xQueueCreate(DISPATCH_QUEUE_SIZE, sizeof(dispatch_msg_t));
        if (s_slots[i].queue == NULL) {
            ESP_LOGE(TAG, "Falha ao criar fila para backend[%d].", i);
            return ESP_ERR_NO_MEM;
        }

        BaseType_t ret = xTaskCreate(backend_task, "telm_bk",
                                     DISPATCH_TASK_STACK, &s_slots[i],
                                     DISPATCH_TASK_PRIORITY, &s_slots[i].task);
        if (ret != pdTRUE) {
            ESP_LOGE(TAG, "Falha ao criar task para backend[%d].", i);
            vQueueDelete(s_slots[i].queue);
            s_slots[i].queue = NULL;
            return ESP_ERR_NO_MEM;
        }

        s_count++;
    }
    return ESP_OK;
}

void telemetry_dispatch_send(const maju_reading_t *r)
{
    if (r == NULL) {
        return;
    }
    for (int i = 0; i < s_count; i++) {
        if (xQueueSend(s_slots[i].queue, r, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Fila do backend[%d] cheia; leitura descartada.", i);
        }
    }
}

void telemetry_dispatch_process_pending(void)
{
    dispatch_msg_t msg;
    for (int i = 0; i < s_count; i++) {
        while (xQueueReceive(s_slots[i].queue, &msg, 0) == pdTRUE) {
            s_slots[i].backend->send(&msg);
        }
    }
}

void telemetry_dispatch_deinit(void)
{
    for (int i = 0; i < s_count; i++) {
        if (s_slots[i].task != NULL) {
            vTaskDelete(s_slots[i].task);
            s_slots[i].task = NULL;
        }
        if (s_slots[i].queue != NULL) {
            telemetry_dispatch_process_pending(); /* drena o que restar */
            vQueueDelete(s_slots[i].queue);
            s_slots[i].queue = NULL;
        }
        s_slots[i].backend->deinit();
    }
    s_count = 0;
}
