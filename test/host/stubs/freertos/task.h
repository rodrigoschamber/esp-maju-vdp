#pragma once
#include "FreeRTOS.h"
#include <stdint.h>

typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);
typedef uint32_t configSTACK_DEPTH_TYPE;
typedef unsigned int UBaseType_t;

#ifndef pdTRUE
#define pdTRUE  1
#define pdFALSE 0
#endif

#define tskIDLE_PRIORITY ((UBaseType_t)0)

/* No-op: testes de host nao esperam por hardware. */
static inline void vTaskDelay(TickType_t ticks) { (void)ticks; }

/* Task creation e deletion sao no-op em host; tasks sao drenadas via process_pending. */
static inline int xTaskCreate(TaskFunction_t fn, const char *name,
                               configSTACK_DEPTH_TYPE stack,
                               void *param, UBaseType_t prio,
                               TaskHandle_t *handle)
{
    (void)fn; (void)name; (void)stack; (void)param; (void)prio;
    if (handle) *handle = NULL;
    return 1; /* pdTRUE */
}

static inline void vTaskDelete(TaskHandle_t h) { (void)h; }
