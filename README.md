# Maju — Sensor de VPD Foliar

Firmware ESP-IDF para a **ESP32-DevKitC V4 (ESP32-WROOM-32UE)** com três sensores
I²C: **Sensirion SHT35** (temperatura e umidade do ar), **Melexis MLX90614**
(termômetro infravermelho pontual, temperatura da folha) e **Panasonic AMG8833**
(matriz térmica 8x8, mapa do dossel). Calcula o VPD do ar e da folha com a
temperatura foliar **medida**, exibe no monitor serial e publica em tempo real no
ThingSpeak e via MQTT (escalares e frame térmico).

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

Os três sensores compartilham o mesmo barramento I²C (SDA GPIO21, SCL GPIO22,
100 kHz). Os endereços não colidem: SHT35 0x44/0x45, MLX90614 0x5A, AMG8833 0x69/0x68.

| Pino do SHT35 | Pino do ESP32       |
| ------------- | ------------------- |
| VDD (VCC)     | 3V3                 |
| GND           | GND                 |
| SDA           | GPIO21              |
| SCL           | GPIO22              |
| ADDR          | GND (endereço 0x44) |

| Pino do MLX90614 (GY-614V3) | Pino do ESP32 |
| --------------------------- | ------------- |
| VIN (VCC)                   | 3V3           |
| GND                         | GND           |
| SDA                         | GPIO21        |
| SCL                         | GPIO22        |

| Pino do AMG8833 | Pino do ESP32                     |
| --------------- | --------------------------------- |
| VIN (VCC)       | 3V3                               |
| GND             | GND                               |
| SDA             | GPIO21                            |
| SCL             | GPIO22                            |
| AD_SELECT       | VDD ou solto (0x69); GND → 0x68   |

- GPIO21/GPIO22 estão no conector J3 e **não** são pinos de strapping.
- O pino `ADDR` do SHT35 não pode ficar flutuando. Em GND → 0x44; em VDD → 0x45
  (nesse caso, habilite `MAJU_SHT35_ADDR_0X45` no `menuconfig`).
- O AMG8833 em 0x68 exige `MAJU_AMG8833_ADDR_0X68` no `menuconfig`. O firmware
  também tenta o endereço alternativo e avisa no log se encontrar o sensor lá.
- O MLX90614 tem endereço fixo 0x5A e é um dispositivo **SMBus**: o barramento
  não pode passar de 100 kHz. A variante `MLX90614-AAA` é de 5 V; a `-BAA`/`-BCC`
  e o módulo GY-614V3 (com regulador) funcionam em 3V3.
- Aponte o MLX90614 para a folha a poucos centímetros: o campo de visão é de 90°
  e, longe do dossel, a leitura mistura o fundo (lâmpada, parede).
- Os breakouts GY-614V3 e AMG8833 já têm resistores de pull-up; somados aos
  pull-ups internos do ESP32, funcionam bem com fios curtos.
- Os três sensores são **obrigatórios**: se algum não responder, o firmware
  registra o erro, tenta de novo a cada ciclo e não publica até todos estarem
  presentes.
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
│   │   ├── sht3x.c / sht3x.h        # driver I²C do SHT3x (CRC-8 Sensirion, single-shot)
│   │   ├── mlx90614.c / mlx90614.h  # driver SMBus do MLX90614 (Read Word + PEC CRC-8)
│   │   ├── amg8833.c / amg8833.h    # driver I²C do AMG8833 (frame 8x8 em 1 transação)
│   ├── domain/
│   │   ├── vpd.c / vpd.h            # fórmulas de VPD e faixas de referência
│   │   ├── thermal.c / thermal.h    # leituras IR, estatísticas do frame, fonte da T folha
│   ├── telemetry/
│   │   ├── telemetry.h              # maju_reading_t e interface telemetry_backend_t
│   │   ├── telemetry_fields.c / .h  # formatação comum: field1..field8 e JSON do frame
│   │   ├── telemetry_dispatch.c / .h  # fila + task por backend (isolamento de timing)
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
| Telemetry   | `telemetry/`          | `esp_http_client`, `mqtt`, `vpd.h`, `thermal.h` |
| Application | `esp_maju_vdp_main.c` | todos os layers acima              |

---

## Design patterns

### Strategy — backend de telemetria intercambiável

A interface `telemetry_backend_t` (em `telemetry/telemetry.h`) desacopla o
protocolo de envio da lógica da aplicação:

```c
typedef struct {
    uint32_t     ts_ms;   /* ms desde o boot */
    float        t_ar;    /* SHT35 */
    float        rh;      /* SHT35 */
    vpd_result_t v;       /* VPD do ar e da folha */
    thermal_t    th;      /* MLX90614 + AMG8833 (64 pixels) e fonte da T folha */
} maju_reading_t;

typedef struct {
    esp_err_t (*init)(void);
    void      (*send)(const maju_reading_t *r);
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

`telemetry_dispatch_init()` cria uma fila e uma task FreeRTOS dedicada para
cada backend. A cada leitura, `telemetry_dispatch_send()` enfileira os dados em
todas as filas sem bloquear — cada backend processa de forma independente em sua
própria task. Um backend lento (ThingSpeak com timeout HTTP de 10 s) ou com
falha não atrasa nem interrompe os demais. Para adicionar um novo backend, basta
incluí-lo no array; nenhuma outra parte do código muda.

### HAL — driver de sensor isolado

**HAL** (_Hardware Abstraction Layer_) é uma camada que isola o restante do
código dos detalhes específicos do hardware.

`hal/sht3x`, `hal/mlx90614` e `hal/amg8833` sabem como falar I²C/SMBus com cada
sensor — endereços, comandos, CRC-8 (Sensirion no SHT3x, PEC SMBus no MLX90614),
timings e conversão bruto→°C. `domain/vpd`, `domain/thermal` e `app_main` não
sabem (e não precisam saber) nada disso — só chamam `sht3x_measure_single()`,
`mlx90614_read_object()` e `amg8833_read_frame()`.

`domain/thermal` decide a fonte da temperatura da folha (MLX90614 primário,
média do AMG8833 como fallback, janela de plausibilidade -10..70 °C) e calcula
mín/média/máx do frame. Por ser C puro, é testado no host sem stubs.

Se um sensor fosse trocado, apenas `hal/` mudaria; o restante do código
permaneceria intacto.

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
MAJU_MQTT_THERMAL_TOPIC="maju/thermal"
MAJU_MQTT_CLIENT_ID="maju_vpd"
```

- `MAJU_THINGSPEAK_ENABLE=1` ativa o envio; `0` desativa sem recompilar.
- `MAJU_MQTT_ENABLE=1` ativa o MQTT; `0` desativa sem recompilar.
- `MAJU_MQTT_THERMAL_TOPIC`: tópico do frame 8x8 do AMG8833 (JSON); deve ser
  diferente de `MAJU_MQTT_TOPIC`. Padrão `maju/thermal`.
- `MAJU_LEAF_OFFSET_C` **foi removido**: a temperatura da folha agora é medida
  pelo MLX90614 (fallback: média do AMG8833). Se a linha ainda existir no `.env`,
  o build emite um aviso e a ignora.

Opções adicionais via `menuconfig`:

```bash
idf.py menuconfig   # → "Sensor VPD - Configuracao"
```

| Opção                              | Padrão                    |
| ---------------------------------- | ------------------------- |
| GPIO do SDA / SCL                  | 21 / 22                   |
| Frequência do I²C                  | 100 kHz (máx.: SMBus)     |
| Endereço do SHT35                  | 0x44 (ADDR em GND)        |
| Endereço do AMG8833                | 0x69 (AD_SELECT em VDD)   |
| Imprimir mapa 8x8 do AMG8833       | sim                       |
| Intervalo entre leituras           | 20 000 ms                 |

O MLX90614 usa o endereço fixo 0x5A (sem opção no `menuconfig`).

Na primeira gravação pode ser necessário instalar o driver USB-UART
(família CP210x/CH34x) para que a porta apareça em `ls /dev/cu.*`.

---

## Integração MQTT

### Visão geral

O backend MQTT publica cada leitura em dois tópicos — os escalares (mesmo
formato `field=` do ThingSpeak) e o frame térmico 8x8 em JSON — em paralelo com a
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

### Formato da mensagem MQTT (tópico `MAJU_MQTT_TOPIC`)

```
field1=24.83&field2=62.14&field3=1.187&field4=0.832&field5=22.83&field6=21.10&field7=22.50&field8=24.90&src=mlx
```

Mesmo formato de payload do ThingSpeak (URL-encoded), com precisão de 2 casas
para temperaturas/umidade e 3 casas para VPD, mais o indicador `src` da fonte da
temperatura da folha (`mlx`, `amg` ou `none`). Campos sem valor válido são
**omitidos** (ex.: `field6..8` quando o AMG8833 falhou no ciclo); nunca é enviado
`nan`.

| Campo    | Dado                              | Unidade | Precisão |
| -------- | --------------------------------- | ------- | -------- |
| `field1` | Temperatura do ar (SHT35)         | °C      | 2 casas  |
| `field2` | Umidade relativa (SHT35)          | %       | 2 casas  |
| `field3` | VPD do ar                         | kPa     | 3 casas  |
| `field4` | VPD da folha (T folha medida)     | kPa     | 3 casas  |
| `field5` | Temperatura da folha usada        | °C      | 2 casas  |
| `field6` | AMG8833 — mínimo do frame         | °C      | 2 casas  |
| `field7` | AMG8833 — média do frame          | °C      | 2 casas  |
| `field8` | AMG8833 — máximo do frame         | °C      | 2 casas  |
| `src`    | Fonte de field5 (`mlx`/`amg`)     | —       | só MQTT  |

### Frame térmico (tópico `MAJU_MQTT_THERMAL_TOPIC`)

Publicado logo após os escalares, **somente quando o frame do AMG8833 é válido**,
com QoS 0. Valores indisponíveis saem como `null` (esquema fixo para o consumidor):

```json
{"ts_ms":123456,"src":"mlx","t_leaf":22.83,"mlx_tobj":22.83,"mlx_ta":24.90,
 "therm":25.10,"min":21.10,"avg":22.50,"max":24.90,
 "px":[22.12,22.31, ... 64 valores em °C, linha a linha (indice = linha*8 + coluna) ...]}
```

Para conferir no terminal:

```bash
mosquitto_sub -h broker.example.com -t 'maju/#' -v
mosquitto_sub -h broker.example.com -t maju/thermal -C 1 | jq '.px | length'   # 64
```

### Vari\u00e1veis de ambiente MQTT

| Vari\u00e1vel          | Obrigat\u00f3ria quando MQTT ativo | Descri\u00e7\u00e3o                                                   |
| ---------------------- | ---------------------------------- | --------------------------------------------------------------------- |
| `MAJU_MQTT_ENABLE`     | \u2014                             | `1`/`true`/`on`/`yes` para ativar; `0` para desativar                 |
| `MAJU_MQTT_BROKER_URI` | Sim                                | URI completa do broker (ex.: `mqtt://host:1883`, `mqtts://host:8883`) |
| `MAJU_MQTT_USERNAME`   | N\u00e3o                           | Usu\u00e1rio MQTT (deixe vazio se n\u00e3o necess\u00e1rio)           |
| `MAJU_MQTT_PASSWORD`   | N\u00e3o                           | Senha MQTT — n\u00e3o \u00e9 exibida em logs                          |
| `MAJU_MQTT_TOPIC`      | Sim                                | T\u00f3pico de publica\u00e7\u00e3o (ex.: `maju/vpd`)                 |
| `MAJU_MQTT_THERMAL_TOPIC` | N\u00e3o                             | T\u00f3pico do frame 8x8 em JSON (padr\u00e3o: `maju/thermal`; diferente de `MAJU_MQTT_TOPIC`) |
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

A temperatura da folha é **medida** por infravermelho, nesta ordem de prioridade:

$$
T_{folha} = \begin{cases}
T_{obj}\ (\text{MLX90614}) & \text{se a leitura for válida e plausível } (-10..70\ °C)\\
\overline{T}_{px}\ (\text{média dos 64 pixels do AMG8833}) & \text{caso contrário}
\end{cases}
$$

Se nenhuma das duas fontes estiver disponível no ciclo, a leitura é descartada
(nada é publicado). O ciclo é registrado no log com `src=mlx` ou `src=amg`.

$$
\text{VPD}_{folha} = \text{SVP}(T_{folha}) - \text{AVP}
$$

> A função `vpd_calculate(t, rh, offset)` (estimativa `T_folha = T_ar - offset`)
> continua disponível em `domain/vpd` para uso sem sensor IR, mas o firmware não
> a usa mais.

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
I (312) maju: Intervalo de leitura: 20000 ms | T folha: IR (MLX90614; fallback AMG8833)
I (322) maju: ThingSpeak ativo: enviando para https://api.thingspeak.com/update
I (322) maju: MQTT ativo: broker=mqtt://broker.example.com:1883, topico=maju/vpd, termico=maju/thermal
I (332) maju: Barramento I2C pronto (SDA=GPIO21, SCL=GPIO22, 100000 Hz)
I (342) maju: SHT35 encontrado no endereco 0x44
I (352) maju: Status do SHT35: 0x8010
I (362) maju: MLX90614 encontrado no endereco 0x5A
I (372) maju: Emissividade configurada no MLX90614: 1.000
I (382) maju: AMG8833 encontrado no endereco 0x69 (aguardando primeiro frame)

+--------------------------------------------------------------+
| LEITURA                                                      |
+--------------------------------------------------------------+
| Temperatura do ar ..........  24.83  C                       |
| Umidade relativa ...........  62.14  %                       |
|                                                              |
| SVP (ar) ...................  3.136 kPa                      |
| AVP (vapor atual) ..........  1.949 kPa                      |
| VPD do ar ..................  1.187 kPa                      |
|                                                              |
| Temp. da folha (IR mlx ) ...  22.83  C                       |
| VPD da folha ...............  0.832 kPa                      |
|                                                              |
| MLX90614 Tobj / Ta .........  22.83 /  24.90 C               |
| AMG8833 min/med/max ........  21.10/ 22.50/ 24.90 C  th  25.1 |
+--------------------------------------------------------------+
  Faixa: vegetativo (0,8-1,2) - crescimento saudavel
  Mapa termico AMG8833 (C):
     21.1  21.6  22.2  22.7  23.3  23.8  24.4  24.9
     ...  (8 linhas x 8 colunas)
I (2352) maju: field1=24.83 field2=62.14 field3=1.187 field4=0.832 field5=22.83 field6=21.10 field7=22.50 field8=24.90 src=mlx
I (2360) mqtt: MQTT publicado: topico=maju/vpd
I (2365) mqtt: MQTT publicado: topico=maju/thermal (64 px)
I (2472) thingspeak: ThingSpeak atualizado com sucesso (entry_id=123)
```

Ao desconectar o MLX90614 o firmware passa a usar o AMG8833 e avisa:

```
E (...) maju: Falha na leitura do MLX90614: ESP_ERR_INVALID_RESPONSE
W (...) maju: MLX90614 indisponivel; usando media do AMG8833 (22.50 C) como T folha.
```

Sem nenhuma fonte de temperatura da folha, a leitura do ciclo é descartada:

```
E (...) maju: Sem temperatura de folha (MLX90614 e AMG8833 falharam); leitura descartada neste ciclo.
```

---

## Dashboard ThingSpeak

Canal público: <https://thingspeak.mathworks.com/channels/3445364>

| Field  | Dado                                   | Unidade |
| ------ | -------------------------------------- | ------- |
| field1 | Temperatura do ar (SHT35)              | °C      |
| field2 | Umidade relativa (SHT35)               | %       |
| field3 | VPD do ar                              | kPa     |
| field4 | VPD da folha (T folha medida por IR)   | kPa     |
| field5 | Temperatura da folha (MLX90614 / AMG)  | °C      |
| field6 | AMG8833 — mínimo do frame              | °C      |
| field7 | AMG8833 — média do frame               | °C      |
| field8 | AMG8833 — máximo do frame              | °C      |

O canal exibe gráficos em tempo real (Temperatura, Umidade, VPD do Ar, VPD
Foliar e as temperaturas IR) e gauges com os valores instantâneos. Quando o
AMG8833 falha num ciclo, `field6..8` ficam vazios naquela entrada.
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
    esp_http_client.h         # tipos e assinaturas do esp_http_client (stub)
    esp_crt_bundle.h          # esp_crt_bundle_attach (stub)
    wifi_env.h                # valores de teste das variaveis MAJU_*_ENV
    freertos/FreeRTOS.h       # TickType_t, pdMS_TO_TICKS
    freertos/task.h           # vTaskDelay (no-op no host)
    freertos/queue.h          # filas FreeRTOS simuladas de forma sincrona
    driver/i2c_master.h       # tipos e assinaturas I2C
    mqtt_client.h             # tipos e assinaturas do esp-mqtt (stub)
  i2c_stub.h / i2c_stub.c    ← mock do barramento I2C (rx fixo + FIFO de respostas, captura de tx, erro configurável)
  thingspeak_http_stub.c      ← mock do esp_http_client para o backend ThingSpeak
  mqtt_stub.h / mqtt_stub.c   ← mock do esp_mqtt_client (histórico das últimas 4 publicações)
  test_fixtures.h             ← make_reading() / make_reading_ir(): maju_reading_t de referência
  test_vpd.c                  ← 17 testes: SVP (Tetens), vpd_calculate, vpd_calculate_leaf, classificar, faixa_str
  test_thermal.c              ← 9 testes: stats do frame, escolha da fonte (MLX/AMG/nenhuma), plausibilidade, reset
  test_sht3x.c                ← 16 testes: create/delete, conversão raw→float, CRC, erros I2C
  test_mlx90614.c             ← 17 testes: create/delete, PEC (vetores fixos, depende do endereço), Tobj/Ta, flag de erro, emissividade
  test_amg8833.c              ← 16 testes: create/delete, sequência de init, termistor, frame 128 B, sinal, erros I2C
  test_telemetry.c            ← 6 testes: dispatch, propagação de erro, troca de backend
  test_fields.c               ← 7 testes: field1..8 (string exata, omissão), JSON do frame (64 valores, null), buffer pequeno
  test_thingspeak.c           ← 11 testes: payload (field1..8), resposta HTTP, erros, URL
  test_mqtt.c                 ← 26 testes: init, send, src, frame térmico em tópico separado, desconexão, falhas, isolamento
  test_dispatch.c             ← 9 testes: fila por backend, roteamento, frame atravessa a fila, não bloqueio, deinit
  test_runner.c               ← main() com todos os RUN_TEST; setUp reseta os stubs
  Makefile
```

### Cobertura

| Módulo       | O que é testado                                                                                                                                                                                                                                                                      |
| ------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `domain/vpd` | `vpd_svp_kpa` (4 temperaturas), `vpd_calculate` (4 cenários), `vpd_calculate_leaf` (típico, folha = ar, equivalência com o wrapper), `vpd_classificar` (5 faixas + bordas), `vpd_faixa_str`                                                                                      |
| `domain/thermal` | `thermal_stats` (frame fixture e constante, ponteiros NULL), `thermal_select_leaf` (ambos ok → MLX; MLX falha → média AMG; ambos falham → NONE/NAN; MLX implausível → AMG), `thermal_source_str`, `thermal_reset`                                                                  |
| `hal/sht3x`  | criação/destruição de handle, conversão raw→°C/%, verificação CRC, falha de CRC em temperatura e umidade, erros I2C, argumento NULL                                                                                                                                                  |
| `hal/mlx90614` | criação/destruição, vetores de PEC fixos (0x78/0x6E/0xD6), Tobj/Ta típicos (0x3ABB → 27,55 °C), mín/máx, PEC inválido, flag de erro bit 15, PEC depende do endereço, emissividade (1,0 e 0,96), erro I2C, NULL                                                                         |
| `hal/amg8833` | criação/destruição, sequência de init (PCTL/RST/FPSC/INTC), frame rate, termistor positivo/negativo (sinal-magnitude), frame 128 B uniforme e misto (complemento de dois, little-endian, nibble alto ignorado), erro I2C, NULL                                                       |
| `telemetry`  | dispatch de `init`/`send`/`deinit` pelo ponteiro de interface, propagação de erro em `init`, troca de backend em runtime                                                                                                                                                             |
| `telemetry/fields` | string exata de field1..8, omissão de field6..8 sem AMG, omissão de field5 com NAN, JSON com 64 pixels e prefixo exato, `null` sem MLX, buffer pequeno → -1                                                                                                                       |
| `thingspeak` | payload HTTP, campos field1–8 (string exata), omissão dos campos AMG, sem `src`, resposta com entry_id, corpo vazio, erro HTTP 4xx/5xx, falha no perform, cliente NULL, URL                                                                                                            |
| `mqtt`       | struct populada, init (ok, cliente null, start fail, register fail), send (tópico, payload field1..8 + `src`, contagem, sem conexão, desconexão, falha no publish), frame térmico (2ª publicação no tópico próprio, 64 valores, ausente sem AMG ou sem conexão), deinit idempotente, erros de transporte TCP e conexão recusada, isolamento |
| `dispatch`   | `init` chama cada backend, `send` roteia para todas as filas com os valores corretos, o frame de 64 pixels e a fonte atravessam a fila, `send` não bloqueia antes do `process_pending`, backend lento não impede o outro, múltiplas leituras bufferizadas, `init` com erro não interrompe os demais, `deinit` zera o estado |

### Executar

```bash
cd test/host
make          # compila e executa (requer IDF_PATH ou . ~/esp/esp-idf/export.sh)
make clean    # remove o binário
```

Saída esperada:

```
134 Tests 0 Failures 0 Ignored
OK
```

No macOS, se `make` falhar com erro do `xcodebuild` (`Symbol not found`), o shim
`/usr/bin/make` está apontando para um Xcode.app quebrado. Use o `make` do Command
Line Tools diretamente ou corrija o `xcode-select`:

```bash
/Library/Developer/CommandLineTools/usr/bin/make            # alternativa imediata
sudo xcode-select --switch /Library/Developer/CommandLineTools   # correção definitiva
```

---

## Diagnóstico

| Sintoma                                                            | Causa provável                                                                                                                                                                             |
| ------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `SHT35 nao respondeu em 0x44 nem 0x45`                             | Fiação, alimentação 3V3 ou pino ADDR flutuando                                                                                                                                             |
| `MLX90614 nao respondeu em 0x5A`                                   | Fiação/3V3; variante `-AAA` (5 V) num barramento de 3V3; módulo com saída PWM habilitada na EEPROM (precisa de SCL em nível baixo ≥ 1,44 ms para entrar em SMBus — não implementado)          |
| `AMG8833 nao respondeu em 0x69 nem 0x68`                           | Fiação/3V3 ou AD_SELECT flutuando; confira `MAJU_AMG8833_ADDR_0X68` no `menuconfig`                                                                                                        |
| `Sensores pendentes: ...`                                          | Um ou mais sensores não inicializaram; o firmware repete a tentativa a cada ciclo e não publica até os três responderem (sensores obrigatórios)                                              |
| `Sem temperatura de folha (MLX90614 e AMG8833 falharam)`           | Ambos os sensores IR falharam ou devolveram valores fora de -10..70 °C neste ciclo; a leitura é descartada                                                                                  |
| `field5` muito acima de `field1` / T folha ≈ temperatura da lâmpada | MLX90614 apontado para o fundo (FOV 90°); aproxime-o do dossel ou incline-o                                                                                                                 |
| Frame do AMG8833 todo zerado logo após o boot                       | Normal nos primeiros ~200 ms após o reset; `amg8833_init` já espera. Se persistir, alimentação instável                                                                                     |
| `ESP_ERR_INVALID_CRC` (SHT35) / `PEC invalido` (MLX90614)          | Ruído no barramento — encurte os fios ou baixe a frequência do I²C                                                                                                                         |
| `ESP_ERR_INVALID_RESPONSE` no MLX90614                             | NACK no barramento ou flag de erro do sensor (bit 15); o handle é recriado no próximo ciclo                                                                                                 |
| Reinícios (brownout)                                               | Fonte fraca — use 5 V ≥ 1 A e cabo de qualidade                                                                                                                                            |
| Temperatura alta demais                                            | Sensor perto da placa; afaste alguns centímetros                                                                                                                                           |
| `MQTT erro de transporte (errno=202, tls_err=0x8001)`              | DNS falhou — `MAJU_MQTT_BROKER_URI` aponta para host inválido ou inacessível. O esp-mqtt reconecta automaticamente; verifique a URI no `.env` e a conectividade Wi-Fi                      |
| `Tool doesn't match supported version` ou erro em `picolibc.specs` | `sdkconfig`/`build` gerados com outro toolchain — apague os dois (`rm -rf build sdkconfig`) e refaça `set-target` + `build`. O `sdkconfig` é gerado; o versionado é o `sdkconfig.defaults` |

---

## Próximos passos

1. Alertas fora de faixa (e-mail / push via ThingSpeak React).
2. Integração com Home Assistant via MQTT Discovery (tópico `homeassistant/sensor/maju_vpd/config`).
3. Display OLED local com faixas coloridas de VPD e mapa térmico 8x8.
4. Ajuste da emissividade do MLX90614 na EEPROM (folhas: ~0,95–0,98; hoje o padrão 1,0
   subestima a folha em ~0,3–0,5 °C).
5. Mapa de calor do tópico `maju/thermal` em um dashboard (Home Assistant / Grafana).
6. Migrar `hal/`, `domain/` e `telemetry/` para `components/` ao reutilizar em outros projetos.
