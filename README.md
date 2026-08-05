# esp-maju-vdp — Sensor de VPD para estufa

Firmware ESP-IDF para a **ESP32-DevKitC V4 (ESP32-WROOM-32UE)** com sensor
**Sensirion SHT35 (I²C)**. Implementa a parte local do planejamento: leitura de
temperatura e umidade, cálculo do VPD do ar e do VPD da folha, e exibição no
monitor serial.

> Escopo desta etapa: **sensor + cálculo + monitor serial**.
> O envio para o ThingSpeak (Wi-Fi + HTTP) entra em uma etapa posterior — a saída
> já imprime uma linha no formato `field1..field4` para facilitar essa transição.

## Comandos

```bash
cd ~/esp/esp-maju-vdp
. $HOME/esp/esp-idf/export.sh                     # ambiente do ESP-IDF (1x por terminal)
idf.py set-target esp32                           # só na primeira vez / ao trocar de alvo
idf.py menuconfig                                 # "Estufa VPD - Configuracao"
idf.py build                                      # compila
ls /dev/cu.*                                      # descobre a porta serial
idf.py -p /dev/cu.usbserial-0001 flash monitor    # grava e abre o monitor
idf.py -p /dev/cu.usbserial-0001 monitor          # só o monitor  (Ctrl+] para sair)
idf.py fullclean                                  # limpa a build
```

## Ligação elétrica

| Pino do SHT35 | Pino do ESP32       |
| ------------- | ------------------- |
| VDD (VCC)     | 3V3                 |
| GND           | GND                 |
| SDA           | GPIO21              |
| SCL           | GPIO22              |
| ADDR          | GND (endereço 0x44) |

- GPIO21/GPIO22 estão no conector J3 e **não** são pinos de strapping.
- O pino `ADDR` não pode ficar flutuando. Em GND → 0x44; em VDD → 0x45
  (nesse caso, habilite `MAJU_SHT35_ADDR_0X45` no `menuconfig`).
- A antena 2,4 GHz IPEX/U.FL é obrigatória na variante UE — ligue-a **antes** de
  energizar a placa (mesmo que o Wi-Fi ainda não seja usado nesta etapa).
- Alimente pela micro-USB com fonte de 5 V ≥ 1 A.

## Estrutura

| Arquivo                                                     | Conteúdo                                                  |
| ----------------------------------------------------------- | --------------------------------------------------------- |
| [main/esp_maju_vdp_main.c](main/esp_maju_vdp_main.c)        | Inicialização do I²C, laço de leitura e impressão         |
| [main/sht3x.c](main/sht3x.c) / [main/sht3x.h](main/sht3x.h) | Driver do SHT3x (single-shot, alta repetibilidade, CRC-8) |
| [main/vpd.c](main/vpd.c) / [main/vpd.h](main/vpd.h)         | Fórmulas de VPD (Tetens) e faixas de referência           |
| [main/Kconfig.projbuild](main/Kconfig.projbuild)            | Pinos, endereço, intervalo e offset de folha              |

## Configuração

Antes do build, crie o arquivo de credenciais locais (nao versionado):

```bash
cp .env.example .env
```

Edite o `.env` com os dados da sua rede:

```dotenv
MAJU_WIFI_SSID="seu_ssid"
MAJU_WIFI_PASSWORD="sua_senha"
```

```
idf.py menuconfig   # → "Estufa VPD - Configuracao"
```

| Opção                    | Padrão              |
| ------------------------ | ------------------- |
| GPIO do SDA / SCL        | 21 / 22             |
| Frequência do I²C        | 100 kHz             |
| Endereço do SHT35        | 0x44 (ADDR em GND)  |
| Intervalo entre leituras | 20 000 ms           |
| Offset de folha          | 20 décimos = 2,0 °C |

Na primeira gravação pode ser necessário instalar o driver USB-UART
(família CP210x/CH34x) para que a porta apareça em `ls /dev/cu.*`.

## Saída esperada

```
I (312) estufa: esp-maju-vdp - sensor de VPD para estufa
I (312) estufa: Intervalo de leitura: 20000 ms | offset de folha: 2.0 C
I (322) estufa: Barramento I2C pronto (SDA=GPIO21, SCL=GPIO22, 100000 Hz)
I (332) estufa: SHT35 encontrado no endereco 0x44
I (342) estufa: Status do sensor: 0x8010

+--------------------------------------------------------------+
| ESTUFA - LEITURA                                             |
+--------------------------------------------------------------+
| Temperatura do ar ..........  24.83  C                       |
| Umidade relativa ...........  62.14  %                       |
|                                                              |
| SVP (ar) ...................  3.136 kPa                      |
| AVP (vapor atual) ..........  1.949 kPa                      |
| VPD do ar ..................  1.187 kPa                      |
|                                                              |
| Temp. da folha (-2.0 C) ....  22.83  C                       |
| VPD da folha ...............  0.832 kPa                      |
+--------------------------------------------------------------+
  Faixa: vegetativo (0,8-1,2) - crescimento saudavel
I (2352) estufa: field1=24.83 field2=62.14 field3=1.187 field4=0.832
```

## Fórmulas

```
SVP      = 0.6108 * exp((17.27 * T) / (T + 237.3))     # Tetens, kPa
AVP      = SVP_ar * (UR / 100)
VPD_ar   = SVP_ar - AVP
T_folha  = T_ar - offset (2 °C no MVP)
VPD_folha= SVP(T_folha) - AVP
```

Faixas de referência do VPD da folha: `< 0,4` baixo · `0,4–0,8` propagação ·
`0,8–1,2` vegetativo · `1,2–1,6` floração · `> 1,6` alto.

## Diagnóstico

| Sintoma                                                            | Causa provável                                                                                                                                                                             |
| ------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `Nenhum sensor respondeu em 0x44`                                  | Fiação, alimentação 3V3 ou pino ADDR flutuando                                                                                                                                             |
| `ESP_ERR_INVALID_CRC`                                              | Ruído no barramento — encurte os fios ou baixe a frequência do I²C                                                                                                                         |
| Reinícios (brownout)                                               | Fonte fraca — use 5 V ≥ 1 A e cabo de qualidade                                                                                                                                            |
| Temperatura alta demais                                            | Sensor perto da placa; afaste alguns centímetros                                                                                                                                           |
| `Tool doesn't match supported version` ou erro em `picolibc.specs` | `sdkconfig`/`build` gerados com outro toolchain — apague os dois (`rm -rf build sdkconfig`) e refaça `set-target` + `build`. O `sdkconfig` é gerado; o versionado é o `sdkconfig.defaults` |

## Próximos passos

1. Wi-Fi + `esp_http_client` enviando `field1..field4` ao ThingSpeak a cada 20 s.
2. Dashboard e faixas coloridas no ThingSpeak.
3. Alertas fora de faixa.
