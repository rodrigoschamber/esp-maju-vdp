/*
 * Leituras infravermelhas (MLX90614 e AMG8833) e escolha da temperatura da folha.
 *
 * Codigo de dominio puro: nao depende do HAL nem do ESP-IDF, para ser testavel no host.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define THERMAL_PIXELS      64          /*!< Matriz 8x8 do AMG8833 */
#define THERMAL_LEAF_MIN_C  (-10.0f)    /*!< Abaixo disso a leitura e tratada como invalida */
#define THERMAL_LEAF_MAX_C  (70.0f)     /*!< Acima disso a leitura e tratada como invalida */

/** Fonte usada para a temperatura da folha neste ciclo. */
typedef enum {
    LEAF_SRC_NONE = 0,  /*!< nenhuma fonte valida */
    LEAF_SRC_MLX,       /*!< MLX90614 Tobj (primaria) */
    LEAF_SRC_AMG,       /*!< media do frame AMG8833 (fallback) */
} leaf_source_t;

/** Leituras IR de um ciclo. Campos *_ok indicam se os valores correspondentes sao validos. */
typedef struct {
    bool  mlx_ok;
    float mlx_tobj_c;               /*!< Temperatura do objeto (folha) em C */
    float mlx_ta_c;                 /*!< Temperatura ambiente do encapsulamento em C */

    bool  amg_ok;
    float amg_therm_c;              /*!< Termistor interno do AMG8833 em C */
    float amg_min_c;
    float amg_max_c;
    float amg_avg_c;
    float amg_px[THERMAL_PIXELS];   /*!< Pixels em C, linha a linha (indice = linha*8 + coluna) */

    leaf_source_t fonte;            /*!< Preenchido por thermal_select_leaf() */
    float         t_folha_c;        /*!< Temperatura da folha escolhida; NAN se fonte == NONE */
} thermal_t;

/** @brief Zera a estrutura: flags false, fonte NONE, t_folha_c = NAN. */
void thermal_reset(thermal_t *th);

/**
 * @brief Minimo, maximo e media de um frame de THERMAL_PIXELS valores.
 *
 * Ponteiros de saida podem ser NULL individualmente.
 */
void thermal_stats(const float px[THERMAL_PIXELS], float *min_c, float *max_c, float *avg_c);

/**
 * @brief Escolhe a temperatura da folha: MLX90614 Tobj se valido e plausivel;
 *        senao a media do AMG8833 se valida e plausivel; senao NONE.
 *
 * Preenche th->fonte e th->t_folha_c e devolve a fonte escolhida.
 */
leaf_source_t thermal_select_leaf(thermal_t *th);

/** @brief "mlx", "amg" ou "none". */
const char *thermal_source_str(leaf_source_t src);

#ifdef __cplusplus
}
#endif
