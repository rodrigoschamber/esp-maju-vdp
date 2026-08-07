# Maju — Sensor de VPD Foliar

Firmware ESP-IDF para a **ESP32-DevKitC V4 (ESP32-WROOM-32UE)** com sensor
**Sensirion SHT35 (I²C)**. Lê temperatura e umidade, calcula o VPD do ar e da
folha, exibe no monitor serial e publica em tempo real no ThingSpeak.

📊 **Dashboard público:** <https://thingspeak.mathworks.com/channels/3445364>

---

## Índice

1. [Ligação elétrica](#ligação-elétrica)
2. [Estrutura do código](#estrutura-do-código)
3. [Design patterns](#design-patterns)
4. [Expansão](#expansão)
5. [Configuração](#configuração)
6. [Comandos](#comandos)
7. [Fórmulas e cálculos](#fórmulas-e-cálculos)
8. [Saída esperada](#saída-esperada)
9. [Dashboard ThingSpeak](#dashboard-thingspeak)
10. [Testes unitários](#testes-unitários)
11. [Diagnóstico](#diagnóstico)
12. [Próximos passos](#próximos-passos)

---

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

---

## Estrutura do código

```
esp-maju-vdp/
├── main/
│   ├── esp_maju_vdp_main.c          # app_main: orquestração geral
│   ├── hal/
│   │   ├── sht3x.c / sht3x.h        # driver I²C do SHT3x (CRC-8, single-shot)
│   ├── domain/
│   │   ├── vpd.c / vpd.h            # fórmulas de VPD e faixas de referência
│   ├── telemetry/
│   │   ├── telemetry.h              # interface genérica telemetry_backend_t
│   │   └── thingspeak/
│   │       ├── telemetry_thingspeak.c / .h  # backend ThingSpeak (HTTP POST)
│   ├── Kconfig.projbuild            # configuração via menuconfig
│   ├── wifi_env.h.in                # template gerado em build time
│   └── CMakeLists.txt
├── .env                             # credenciais locais (não versionado)
├── .env.example
├── sdkconfig.defaults
└── CMakeLists.txt
```

### Separação de responsabilidades

| Layer       | Pasta                 | Depende de                 |
| ----------- | --------------------- | -------------------------- |
| HAL         | `hal/`                | `esp_driver_i2c`           |
| Domain      | `domain/`             | nada (C puro)              |
| Telemetry   | `telemetry/`          | `esp_http_client`, `vpd.h` |
| Application | `esp_maju_vdp_main.c` | todos os layers acima      |

---

## Design patterns

### Strategy — backend de telemetria intercambiável

A interface `telemetry_backend_t` (em `telemetry/telemetry.h`) desacopla o
protocolo de envio da lógica da aplicação:

```c
typedef struct {
    esp_err_t (*init)(void);
    void      (*send)(float t, float rh, const vpd_result_t *v);
    void      (*deinit)(void);
} telemetry_backend_t;
```

`app_main` usa apenas um ponteiro para essa interface:

```c
static const telemetry_backend_t *s_telemetry = &thingspeak_backend;
```

Para trocar de plataforma, basta apontar para outro backend — sem tocar no
restante da aplicação.

### HAL — driver de sensor isolado

**HAL** (_Hardware Abstraction Layer_) é uma camada que isola o restante do
código dos detalhes específicos do hardware.

`hal/sht3x` sabe como falar I²C com o SHT35 — endereços, comandos, CRC-8,
timings. `domain/vpd` e `app_main` não sabem (e não precisam saber) nada disso
— só chamam `sht3x_measure_single()`.

Se o sensor fosse trocado por um DHT22 ou BME280, apenas `hal/` mudaria; o
restante do código permaneceria intacto.

---

## Expansão

### Adicionar um novo backend de telemetria (ex.: MQTT)

1. Criar `main/telemetry/mqtt/telemetry_mqtt.c` e `telemetry_mqtt.h`.
2. Implementar as três funções (`init`, `send`, `deinit`) e exportar `mqtt_backend`.
3. Adicionar o `.c` ao `SRCS` e o diretório ao `INCLUDE_DIRS` em `CMakeLists.txt`.
4. Em `esp_maju_vdp_main.c`, alterar apenas uma linha:

```c
static const telemetry_backend_t *s_telemetry = &mqtt_backend;
```

### Migrar para componentes ESP-IDF (`components/`)

Quando `hal/`, `domain/` ou `telemetry/` precisarem ser reutilizados em
outros projetos, mova cada pasta para `components/<nome>/` com seu próprio
`CMakeLists.txt` e declare a dependência em `main/CMakeLists.txt` via
`PRIV_REQUIRES`.

---

## Configuração

Antes do build, crie o arquivo de credenciais locais (não versionado):

```bash
cp .env.example .env
```

Edite o `.env`:

```dotenv
MAJU_WIFI_SSID="ssid"
MAJU_WIFI_PASSWORD="password"
MAJU_LEAF_OFFSET_C="+2.0"
MAJU_THINGSPEAK_ENABLE="1"
MAJU_THINGSPEAK_WRITE_API_KEY="SUA_WRITE_API_KEY"
MAJU_THINGSPEAK_URL="https://api.thingspeak.com/update"
```

- `MAJU_LEAF_OFFSET_C`: diferença de temperatura entre ar e folha em °C.
  - `+2.0` → folha 2 °C mais fria que o ar (típico em ambientes controlados).
  - `-1.0` → folha mais quente (sob luz intensa sem transpiração suficiente).
- `MAJU_THINGSPEAK_ENABLE=1` ativa o envio; `0` desativa sem recompilar.

Opções adicionais via `menuconfig`:

```bash
idf.py menuconfig   # → "Sensor VPD - Configuracao"
```

| Opção                    | Padrão             |
| ------------------------ | ------------------ |
| GPIO do SDA / SCL        | 21 / 22            |
| Frequência do I²C        | 100 kHz            |
| Endereço do SHT35        | 0x44 (ADDR em GND) |
| Intervalo entre leituras | 20 000 ms          |

Na primeira gravação pode ser necessário instalar o driver USB-UART
(família CP210x/CH34x) para que a porta apareça em `ls /dev/cu.*`.

---

## Comandos

```bash
cd ~/esp/esp-maju-vdp
. $HOME/esp/esp-idf/export.sh                     # ambiente do ESP-IDF (1x por terminal)
idf.py set-target esp32                           # só na primeira vez / ao trocar de alvo
idf.py menuconfig                                 # "Sensor VPD - Configuracao"
idf.py build                                      # compila
ls /dev/cu.*                                      # descobre a porta serial
idf.py -p /dev/cu.usbserial-0001 flash monitor    # grava e abre o monitor
idf.py -p /dev/cu.usbserial-0001 monitor          # só o monitor  (Ctrl+] para sair)
idf.py fullclean                                  # limpa a build quando algo ficar inconsistente
```

---

## Fórmulas e cálculos

### Pressão de vapor de saturação (SVP) — Tetens

$$
\text{SVP}(T) = 0{,}6108 \times e^{\dfrac{17{,}27 \times T}{T + 237{,}3}} \quad [\text{kPa}]
$$

### Pressão de vapor atual (AVP)

$$
\text{AVP} = \text{SVP}_{ar} \times \frac{UR}{100}
$$

### VPD do ar

$$
\text{VPD}_{ar} = \text{SVP}_{ar} - \text{AVP}
$$

### Temperatura da folha e VPD foliar

$$
T_{folha} = T_{ar} - \text{offset} \qquad \text{(offset positivo = folha mais fria)}
$$

$$
\text{VPD}_{folha} = \text{SVP}(T_{folha}) - \text{AVP}
$$

O VPD foliar é o indicador primário de manejo: representa a força de sucção
de água que a folha exerce — muito baixo → fungos; muito alto → estresse hídrico.

### Faixas de referência (VPD da folha)

| Faixa (kPa) | Fase             | Indicação                            |
| ----------- | ---------------- | ------------------------------------ |
| < 0,4       | Baixo            | Risco de doenças fúngicas            |
| 0,4 – 0,8   | Propagação/clone | Ideal para enraizamento              |
| 0,8 – 1,2   | Vegetativo       | Crescimento saudável                 |
| 1,2 – 1,6   | Floração         | Transpiração ativa, produção elevada |
| > 1,6       | Alto             | Estresse hídrico, fechar estômatos   |

---

## Saída esperada

```
I (312) maju: maju - sensor de VPD
I (312) maju: Intervalo de leitura: 20000 ms | offset de folha: +2.0 C
I (322) maju: Barramento I2C pronto (SDA=GPIO21, SCL=GPIO22, 100000 Hz)
I (332) maju: SHT35 encontrado no endereco 0x44
I (342) maju: Status do sensor: 0x8010
I (352) maju: ThingSpeak ativo: enviando para https://api.thingspeak.com/update

+--------------------------------------------------------------+
| LEITURA                                             |
+--------------------------------------------------------------+
| Temperatura do ar ..........  24.83  C                       |
| Umidade relativa ...........  62.14  %                       |
|                                                              |
| SVP (ar) ...................  3.136 kPa                      |
| AVP (vapor atual) ..........  1.949 kPa                      |
| VPD do ar ..................  1.187 kPa                      |
|                                                              |
| Temp. da folha (+2.0 C) ....  22.83  C                       |
| VPD da folha ...............  0.832 kPa                      |
+--------------------------------------------------------------+
  Faixa: vegetativo (0,8-1,2) - crescimento saudavel
I (2352) maju: field1=24.83 field2=62.14 field3=1.187 field4=0.832
I (2472) maju: ThingSpeak atualizado com sucesso (entry_id=123)
```

---

## Dashboard ThingSpeak

Canal público: <https://thingspeak.mathworks.com/channels/3445364>

| Field  | Dado                       | Unidade |
| ------ | -------------------------- | ------- |
| field1 | Temperatura do ar          | °C      |
| field2 | Umidade relativa           | %       |
| field3 | VPD do ar                  | kPa     |
| field4 | VPD da folha (offset +2°C) | kPa     |

O canal exibe quatro gráficos em tempo real (Temperatura, Umidade, VPD do Ar e
VPD Foliar) e dois gauges com os valores instantâneos de temperatura e umidade.
O intervalo de atualização mínimo do ThingSpeak é 15 s; o firmware usa 20 s por
padrão para respeitar essa limitação.

---

## Testes unitários

Testes de host compilados com GCC nativo (sem hardware) usando o framework
**Unity** já incluso no ESP-IDF.

### Estrutura

```
test/host/
  stubs/                      ← cabeçalhos ESP-IDF mínimos para compilação no macOS
    esp_err.h                 # esp_err_t e códigos de erro
    esp_log.h                 # ESP_LOGI/LOGE/LOGW → printf
    esp_check.h               # ESP_RETURN_ON_FALSE / ESP_RETURN_ON_ERROR
    freertos/FreeRTOS.h       # TickType_t, pdMS_TO_TICKS
    freertos/task.h           # vTaskDelay (no-op no host)
    driver/i2c_master.h       # tipos e assinaturas I2C
  i2c_stub.h / i2c_stub.c    ← mock do barramento I2C (rx injetável, erro configurável)
  test_vpd.c                  ← 14 testes: SVP (Tetens), vpd_calculate, classificar, faixa_str
  test_sht3x.c                ← 16 testes: create/delete, conversão raw→float, CRC, erros I2C
  test_telemetry.c            ← 6 testes: dispatch, propagação de erro, troca de backend
  test_runner.c               ← main() com todos os RUN_TEST; setUp reseta o stub I2C
  Makefile
```

### Cobertura

| Módulo       | O que é testado                                                                                                                     |
| ------------ | ----------------------------------------------------------------------------------------------------------------------------------- |
| `domain/vpd` | `vpd_svp_kpa` (4 temperaturas), `vpd_calculate` (4 cenários), `vpd_classificar` (5 faixas + bordas), `vpd_faixa_str`                |
| `hal/sht3x`  | criação/destruição de handle, conversão raw→°C/%, verificação CRC, falha de CRC em temperatura e umidade, erros I2C, argumento NULL |
| `telemetry`  | dispatch de `init`/`send`/`deinit` pelo ponteiro de interface, propagação de erro em `init`, troca de backend em runtime            |

### Executar

```bash
cd test/host
make          # compila e executa (requer IDF_PATH ou . ~/esp/esp-idf/export.sh)
make clean    # remove o binário
```

Saída esperada:

```
36 Tests 0 Failures 0 Ignored
OK
```

---

## Diagnóstico

| Sintoma                                                            | Causa provável                                                                                                                                                                             |
| ------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `Nenhum sensor respondeu em 0x44`                                  | Fiação, alimentação 3V3 ou pino ADDR flutuando                                                                                                                                             |
| `ESP_ERR_INVALID_CRC`                                              | Ruído no barramento — encurte os fios ou baixe a frequência do I²C                                                                                                                         |
| Reinícios (brownout)                                               | Fonte fraca — use 5 V ≥ 1 A e cabo de qualidade                                                                                                                                            |
| Temperatura alta demais                                            | Sensor perto da placa; afaste alguns centímetros                                                                                                                                           |
| `Tool doesn't match supported version` ou erro em `picolibc.specs` | `sdkconfig`/`build` gerados com outro toolchain — apague os dois (`rm -rf build sdkconfig`) e refaça `set-target` + `build`. O `sdkconfig` é gerado; o versionado é o `sdkconfig.defaults` |

---

## Próximos passos

1. Alertas fora de faixa (e-mail / push via ThingSpeak React).
2. Backend MQTT (`telemetry/mqtt/`) para integração com Home Assistant.
3. Display OLED local com faixas coloridas de VPD.
4. Migrar `hal/sht3x` e `domain/vpd` para `components/` ao reutilizar em outros projetos.
