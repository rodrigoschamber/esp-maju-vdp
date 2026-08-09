# Maju — Sensor de VPD Foliar

Firmware ESP-IDF para a **ESP32-DevKitC V4 (ESP32-WROOM-32UE)** com sensor
**Sensirion SHT35 (I²C)**. Lê temperatura e umidade, calcula o VPD do ar e da
folha, exibe no monitor serial e publica em tempo real no ThingSpeak e via MQTT.

📊 **Dashboard público:** <https://thingspeak.mathworks.com/channels/3445364>

---

## Índice

1. [Ligação elétrica](#ligação-elétrica)
2. [Estrutura do código](#estrutura-do-código)
3. [Design patterns](#design-patterns)
4. [Expansão](#expansão)
5. [Configuração](#configuração)
6. [Integração MQTT](#integração-mqtt)
7. [Comandos](#comandos)
8. [Fórmulas e cálculos](#fórmulas-e-cálculos)
9. [Saída esperada](#saída-esperada)
10. [Dashboard ThingSpeak](#dashboard-thingspeak)
11. [Testes unitários](#testes-unitários)
12. [Diagnóstico](#diagnóstico)
13. [Próximos passos](#próximos-passos)

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
│   ├── idf_component.yml            # dependência: espressif/mqtt (IDF Component Manager)
│   ├── hal/
│   │   ├── sht3x.c / sht3x.h        # driver I²C do SHT3x (CRC-8, single-shot)
│   ├── domain/
│   │   ├── vpd.c / vpd.h            # fórmulas de VPD e faixas de referência
│   ├── telemetry/
│   │   ├── telemetry.h              # interface genérica telemetry_backend_t
│   │   ├── thingspeak/
│   │   │   ├── telemetry_thingspeak.c / .h  # backend ThingSpeak (HTTP POST)
│   │   └── mqtt/
│   │       ├── telemetry_mqtt.c / .h        # backend MQTT (esp-mqtt, QoS 0)
│   ├── Kconfig.projbuild            # configuração via menuconfig
│   ├── wifi_env.h.in                # template gerado em build time
│   └── CMakeLists.txt
├── .env                             # credenciais locais (não versionado)
├── .env.example
├── sdkconfig.defaults
└── CMakeLists.txt
```

### Separação de responsabilidades

| Layer       | Pasta                 | Depende de                         |
| ----------- | --------------------- | ---------------------------------- |
| HAL         | `hal/`                | `esp_driver_i2c`                   |
| Domain      | `domain/`             | nada (C puro)                      |
| Telemetry   | `telemetry/`          | `esp_http_client`, `mqtt`, `vpd.h` |
| Application | `esp_maju_vdp_main.c` | todos os layers acima              |

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

`app_main` mantém um array terminado em `NULL` com todos os backends ativos:

```c
static const telemetry_backend_t *const s_backends[] = {
    &mqtt_backend,
    &thingspeak_backend,
    NULL,
};
```

Cada leitura itera o array chamando `send()` em sequência — MQTT retorna
imediatamente (QoS 0, fire-and-forget) enquanto o ThingSpeak processa em
parallel via sua task interna. Para adicionar um novo backend, basta incluí-lo
no array; nenhuma outra parte do código muda.

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

### Adicionar um novo backend de telemetria

O projeto já contém dois backends em produção:

- `thingspeak_backend` — HTTP POST para a API do ThingSpeak
- `mqtt_backend` — publicação MQTT via esp-mqtt (QoS 0, fire-and-forget)

Ambos são registrados no array `s_backends[]` em `esp_maju_vdp_main.c`. Para
adicionar um terceiro backend (ex.: InfluxDB):

1. Criar `main/telemetry/influxdb/telemetry_influxdb.c` e `.h`.
2. Implementar `init`, `send`, `deinit` e exportar `influxdb_backend`.
3. Adicionar `.c` ao `SRCS` e o diretório ao `INCLUDE_DIRS` em `CMakeLists.txt`.
4. Incluir `&influxdb_backend` no array `s_backends[]` em `app_main`.

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

# ThingSpeak
MAJU_THINGSPEAK_ENABLE="1"
MAJU_THINGSPEAK_WRITE_API_KEY="SUA_WRITE_API_KEY"
MAJU_THINGSPEAK_URL="https://api.thingspeak.com/update"

# MQTT
MAJU_MQTT_ENABLE="1"
MAJU_MQTT_BROKER_URI="mqtt://broker.example.com:1883"
MAJU_MQTT_USERNAME=""
MAJU_MQTT_PASSWORD=""
MAJU_MQTT_TOPIC="maju/vpd"
MAJU_MQTT_CLIENT_ID="maju_vpd"
```

- `MAJU_LEAF_OFFSET_C`: diferença de temperatura entre ar e folha em °C.
  - `+2.0` → folha 2 °C mais fria que o ar (típico em ambientes controlados).
  - `-1.0` → folha mais quente (sob luz intensa sem transpiração suficiente).
- `MAJU_THINGSPEAK_ENABLE=1` ativa o envio; `0` desativa sem recompilar.
- `MAJU_MQTT_ENABLE=1` ativa o MQTT; `0` desativa sem recompilar.

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

## Integração MQTT

### Visão geral

O backend MQTT publica cada leitura como uma mensagem JSON em paralelo com a
chamada HTTP ao ThingSpeak. A implementação usa o componente **esp-mqtt**
(`espressif/mqtt` via IDF Component Manager) com TLS automático via
`esp_crt_bundle_attach`.

### Arquitetura e fluxo de envio

```
loop de leitura (SAMPLE_INTERVAL_MS)
│
├─── mqtt_backend.send()     ← QoS 0, fire-and-forget; retorna imediatamente
│       └─ mqtt_task (interno do esp-mqtt) envia a mensagem em background
│
└─── thingspeak_backend.send() ← HTTP POST bloqueante (timeout 10 s)
```

As duas opera\u00e7\u00f5es ocorrem **em paralelo**: `mqtt_backend.send()` encaminha a
mensagem para a task interna do esp-mqtt e retorna antes de o ThingSpeak
terminar. Uma falha em qualquer um dos backends **n\u00e3o interrompe o outro**.

### Formato da mensagem MQTT

```
field1=24.83&field2=62.14&field3=1.187&field4=0.832
```

Mesmo formato de payload do ThingSpeak (URL-encoded), com precisão de 2 casas
para temperatura/umidade e 3 casas para VPD.

| Campo    | Dado              | Unidade | Precisão |
| -------- | ----------------- | ------- | -------- |
| `field1` | Temperatura do ar | °C      | 2 casas  |
| `field2` | Umidade relativa  | %       | 2 casas  |
| `field3` | VPD do ar         | kPa     | 3 casas  |
| `field4` | VPD da folha      | kPa     | 3 casas  |

### Vari\u00e1veis de ambiente MQTT

| Vari\u00e1vel          | Obrigat\u00f3ria quando MQTT ativo | Descri\u00e7\u00e3o                                                   |
| ---------------------- | ---------------------------------- | --------------------------------------------------------------------- |
| `MAJU_MQTT_ENABLE`     | \u2014                             | `1`/`true`/`on`/`yes` para ativar; `0` para desativar                 |
| `MAJU_MQTT_BROKER_URI` | Sim                                | URI completa do broker (ex.: `mqtt://host:1883`, `mqtts://host:8883`) |
| `MAJU_MQTT_USERNAME`   | N\u00e3o                           | Usu\u00e1rio MQTT (deixe vazio se n\u00e3o necess\u00e1rio)           |
| `MAJU_MQTT_PASSWORD`   | N\u00e3o                           | Senha MQTT — n\u00e3o \u00e9 exibida em logs                          |
| `MAJU_MQTT_TOPIC`      | Sim                                | T\u00f3pico de publica\u00e7\u00e3o (ex.: `maju/vpd`)                 |
| `MAJU_MQTT_CLIENT_ID`  | N\u00e3o                           | Client ID (padr\u00e3o: `maju_vpd`)                                   |

### Comportamento em caso de falha

| Cen\u00e1rio                           | Comportamento                                                                    |
| -------------------------------------- | -------------------------------------------------------------------------------- |
| Broker MQTT indispon\u00edvel          | `send()` loga aviso e descarta a leitura; ThingSpeak n\u00e3o \u00e9 afetado     |
| Broker MQTT fica acess\u00edvel depois | O esp-mqtt reconecta automaticamente; pr\u00f3ximas leituras s\u00e3o publicadas |
| ThingSpeak retorna erro HTTP           | `send()` loga o erro; MQTT n\u00e3o \u00e9 afetado                               |
| ThingSpeak excede timeout (10 s)       | MQTT j\u00e1 terminou antes; aguarda o ThingSpeak retornar                       |

### TLS

Use o esquema `mqtts://` na URI para habilitar TLS autom\u00e1tico:

```dotenv
MAJU_MQTT_BROKER_URI="mqtts://broker.example.com:8883"
```

O bundle de certificados do ESP-IDF (`esp_crt_bundle_attach`) \u00e9 inclu\u00eddo
automaticamente e valida a cadeia de certificados do broker.

### Depend\u00eancia

O componente esp-mqtt \u00e9 declarado em `main/idf_component.yml` e baixado
automaticamente pelo IDF Component Manager na primeira compila\u00e7\u00e3o:

```bash
idf.py build   # baixa espressif/mqtt >= 1.0.0 automaticamente
```

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
I (322) maju: ThingSpeak ativo: enviando para https://api.thingspeak.com/update
I (322) maju: MQTT ativo: broker=mqtt://broker.example.com:1883, topico=maju/vpd
I (332) maju: Barramento I2C pronto (SDA=GPIO21, SCL=GPIO22, 100000 Hz)
I (342) maju: SHT35 encontrado no endereco 0x44
I (352) maju: Status do sensor: 0x8010

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
I (2360) mqtt: MQTT publicado: topico=maju/vpd
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
    mqtt_client.h             # tipos e assinaturas do esp-mqtt (stub)
  i2c_stub.h / i2c_stub.c    ← mock do barramento I2C (rx injetável, erro configurável)
  thingspeak_http_stub.c      ← mock do esp_http_client para o backend ThingSpeak
  mqtt_stub.c                 ← mock do esp_mqtt_client para o backend MQTT
  test_vpd.c                  ← 14 testes: SVP (Tetens), vpd_calculate, classificar, faixa_str
  test_sht3x.c                ← 16 testes: create/delete, conversão raw→float, CRC, erros I2C
  test_telemetry.c            ← 6 testes: dispatch, propagação de erro, troca de backend
  test_thingspeak.c           ← 9 testes: payload, resposta HTTP, erros, URL
  test_mqtt.c                 ← 20 testes: init, send, desconexão, falhas, erros de transporte/recusa, isolamento
  test_runner.c               ← main() com todos os RUN_TEST; setUp reseta os stubs
  Makefile
```

### Cobertura

| Módulo       | O que é testado                                                                                                                                                                                                                                                                      |
| ------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `domain/vpd` | `vpd_svp_kpa` (4 temperaturas), `vpd_calculate` (4 cenários), `vpd_classificar` (5 faixas + bordas), `vpd_faixa_str`                                                                                                                                                                 |
| `hal/sht3x`  | criação/destruição de handle, conversão raw→°C/%, verificação CRC, falha de CRC em temperatura e umidade, erros I2C, argumento NULL                                                                                                                                                  |
| `telemetry`  | dispatch de `init`/`send`/`deinit` pelo ponteiro de interface, propagação de erro em `init`, troca de backend em runtime                                                                                                                                                             |
| `thingspeak` | payload HTTP, campos field1–4, resposta com entry_id, corpo vazio, erro HTTP 4xx/5xx, falha no perform, cliente NULL, URL                                                                                                                                                            |
| `mqtt`       | struct populada, init (ok, cliente null, start fail, register fail), send (tópico, payload JSON, contagem, sem conexão, desconexão, falha no publish), deinit idempotente, erros de transporte TCP e conexão recusada, isolamento (falha MQTT ≠ ThingSpeak; falha ThingSpeak ≠ MQTT) |

### Executar

```bash
cd test/host
make          # compila e executa (requer IDF_PATH ou . ~/esp/esp-idf/export.sh)
make clean    # remove o binário
```

Saída esperada:

```
65 Tests 0 Failures 0 Ignored
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
| `MQTT erro de transporte (errno=202, tls_err=0x8001)`              | DNS falhou — `MAJU_MQTT_BROKER_URI` aponta para host inválido ou inacessível. O esp-mqtt reconecta automaticamente; verifique a URI no `.env` e a conectividade Wi-Fi                      |
| `Tool doesn't match supported version` ou erro em `picolibc.specs` | `sdkconfig`/`build` gerados com outro toolchain — apague os dois (`rm -rf build sdkconfig`) e refaça `set-target` + `build`. O `sdkconfig` é gerado; o versionado é o `sdkconfig.defaults` |

---

## Próximos passos

1. Alertas fora de faixa (e-mail / push via ThingSpeak React).
2. Integração com Home Assistant via MQTT Discovery (tópico `homeassistant/sensor/maju_vpd/config`).
3. Display OLED local com faixas coloridas de VPD.
4. Migrar `hal/sht3x` e `domain/vpd` para `components/` ao reutilizar em outros projetos.
