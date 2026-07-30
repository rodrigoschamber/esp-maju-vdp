/*
 * Calculo do VPD (deficit de pressao de vapor).
 */
#include <math.h>
#include "vpd.h"

float vpd_svp_kpa(float temperature_c)
{
    return 0.6108f * expf((17.27f * temperature_c) / (temperature_c + 237.3f));
}

void vpd_calculate(float temperature_c, float humidity_rh, float leaf_offset_c, vpd_result_t *out)
{
    out->svp_ar    = vpd_svp_kpa(temperature_c);
    out->avp       = out->svp_ar * (humidity_rh / 100.0f);
    out->vpd_ar    = out->svp_ar - out->avp;

    out->t_folha_c = temperature_c - leaf_offset_c;
    out->svp_folha = vpd_svp_kpa(out->t_folha_c);
    out->vpd_folha = out->svp_folha - out->avp;
}

vpd_faixa_t vpd_classificar(float vpd_kpa)
{
    if (vpd_kpa < 0.4f) {
        return VPD_FAIXA_BAIXA;
    }
    if (vpd_kpa < 0.8f) {
        return VPD_FAIXA_PROPAGACAO;
    }
    if (vpd_kpa < 1.2f) {
        return VPD_FAIXA_VEGETATIVO;
    }
    if (vpd_kpa <= 1.6f) {
        return VPD_FAIXA_FLORACAO;
    }
    return VPD_FAIXA_ALTA;
}

const char *vpd_faixa_str(vpd_faixa_t faixa)
{
    switch (faixa) {
    case VPD_FAIXA_BAIXA:      return "ABAIXO (<0,4) - risco de fungos, baixa transpiracao";
    case VPD_FAIXA_PROPAGACAO: return "propagacao/mudas (0,4-0,8) - ambiente umido";
    case VPD_FAIXA_VEGETATIVO: return "vegetativo (0,8-1,2) - crescimento saudavel";
    case VPD_FAIXA_FLORACAO:   return "floracao (1,2-1,6) - maior transpiracao";
    case VPD_FAIXA_ALTA:       return "ACIMA (>1,6) - risco de estresse hidrico";
    default:                   return "desconhecida";
    }
}
