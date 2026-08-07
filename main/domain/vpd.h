/*
 * Calculo do VPD (deficit de pressao de vapor) a partir de temperatura e
 * umidade relativa. Formulas conforme o planejamento do sensor de VPD.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Estagio/faixa em que o VPD da folha se encontra. */
typedef enum {
    VPD_FAIXA_BAIXA,        /*!< < 0,4 kPa  - risco de fungos, baixa transpiracao */
    VPD_FAIXA_PROPAGACAO,   /*!< 0,4 - 0,8 kPa - mudas / clones / propagacao */
    VPD_FAIXA_VEGETATIVO,   /*!< 0,8 - 1,2 kPa - crescimento saudavel */
    VPD_FAIXA_FLORACAO,     /*!< 1,2 - 1,6 kPa - maior transpiracao */
    VPD_FAIXA_ALTA,         /*!< > 1,6 kPa  - risco de estresse hidrico */
} vpd_faixa_t;

/** Resultado completo de um calculo de VPD. */
typedef struct {
    float svp_ar;       /*!< Pressao de saturacao de vapor no ar (kPa) */
    float avp;          /*!< Pressao atual de vapor (kPa) */
    float vpd_ar;       /*!< VPD do ar (kPa) */
    float t_folha_c;    /*!< Temperatura estimada da folha (°C) */
    float svp_folha;    /*!< Pressao de saturacao a temperatura da folha (kPa) */
    float vpd_folha;    /*!< VPD da folha (kPa) */
} vpd_result_t;

/**
 * @brief Pressao de saturacao de vapor pela equacao de Tetens.
 *
 * SVP = 0.6108 * exp((17.27 * T) / (T + 237.3))
 *
 * @param temperature_c Temperatura em °C.
 * @return Pressao de saturacao em kPa.
 */
float vpd_svp_kpa(float temperature_c);

/**
 * @brief Calcula VPD do ar e VPD da folha.
 *
 * @param temperature_c    Temperatura do ar em °C.
 * @param humidity_rh      Umidade relativa em % (0-100).
 * @param leaf_offset_c    Quanto a folha esta mais fria que o ar, em °C (tipico 1-2).
 * @param[out] out         Resultado do calculo.
 */
void vpd_calculate(float temperature_c, float humidity_rh, float leaf_offset_c, vpd_result_t *out);

/** @brief Classifica um valor de VPD (kPa) nas faixas de referencia. */
vpd_faixa_t vpd_classificar(float vpd_kpa);

/** @brief Texto descritivo da faixa, para exibicao no monitor. */
const char *vpd_faixa_str(vpd_faixa_t faixa);

#ifdef __cplusplus
}
#endif
