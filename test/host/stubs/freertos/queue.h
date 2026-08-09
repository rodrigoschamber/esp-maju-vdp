#pragma once
#include "FreeRTOS.h"
#include <stdlib.h>
#include <string.h>

typedef void *QueueHandle_t;
typedef int   BaseType_t;
typedef unsigned int UBaseType_t;

#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  pdTRUE
#define portMAX_DELAY ((TickType_t)0xffffffffUL)

#define STUB_QUEUE_SLOTS 16

typedef struct {
    uint8_t *buf;
    size_t   item_size;
    size_t   capacity;
    size_t   count;
    size_t   head;
    size_t   tail;
} stub_queue_t;

static inline QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t item_size)
{
    stub_queue_t *q = calloc(1, sizeof(stub_queue_t));
    if (!q) return NULL;
    q->buf       = calloc(len, item_size);
    q->item_size = item_size;
    q->capacity  = len;
    return q;
}

/* timeout ignorado em host — retorna imediatamente. */
static inline BaseType_t xQueueReceive(QueueHandle_t xq, void *out, TickType_t timeout)
{
    (void)timeout;
    stub_queue_t *q = xq;
    if (!q || q->count == 0) return pdFALSE;
    memcpy(out, q->buf + q->head * q->item_size, q->item_size);
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    return pdTRUE;
}

static inline BaseType_t xQueueSend(QueueHandle_t xq, const void *item, TickType_t timeout)
{
    (void)timeout;
    stub_queue_t *q = xq;
    if (!q || q->count >= q->capacity) return pdFALSE;
    memcpy(q->buf + q->tail * q->item_size, item, q->item_size);
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    return pdTRUE;
}

static inline void vQueueDelete(QueueHandle_t xq)
{
    stub_queue_t *q = xq;
    if (q) {
        free(q->buf);
        free(q);
    }
}
