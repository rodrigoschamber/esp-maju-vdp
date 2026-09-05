set(MAJU_ENV_FILE "${PROJECT_DIR}/.env")
if(NOT EXISTS "${MAJU_ENV_FILE}")
     message(FATAL_ERROR "Arquivo .env nao encontrado em ${MAJU_ENV_FILE}. Copie .env.example para .env e preencha as variaveis obrigatorias.")
endif()

file(STRINGS "${MAJU_ENV_FILE}" MAJU_ENV_LINES REGEX "^MAJU_(WIFI_(SSID|PASSWORD)|LEAF_OFFSET_C|THINGSPEAK_(ENABLE|WRITE_API_KEY|URL)|MQTT_(ENABLE|BROKER_URI|USERNAME|PASSWORD|TOPIC|THERMAL_TOPIC|CLIENT_ID))=")

foreach(line IN LISTS MAJU_ENV_LINES)
     if(line MATCHES "^MAJU_WIFI_SSID=(.*)$")
          set(MAJU_WIFI_SSID_ENV "${CMAKE_MATCH_1}")
     endif()
     if(line MATCHES "^MAJU_WIFI_PASSWORD=(.*)$")
          set(MAJU_WIFI_PASSWORD_ENV "${CMAKE_MATCH_1}")
     endif()
     if(line MATCHES "^MAJU_LEAF_OFFSET_C=(.*)$")
          set(MAJU_LEAF_OFFSET_C_ENV "${CMAKE_MATCH_1}")
     endif()
     if(line MATCHES "^MAJU_THINGSPEAK_ENABLE=(.*)$")
          set(MAJU_THINGSPEAK_ENABLE_ENV "${CMAKE_MATCH_1}")
     endif()
     if(line MATCHES "^MAJU_THINGSPEAK_WRITE_API_KEY=(.*)$")
          set(MAJU_THINGSPEAK_WRITE_API_KEY_ENV "${CMAKE_MATCH_1}")
     endif()
     if(line MATCHES "^MAJU_THINGSPEAK_URL=(.*)$")
          set(MAJU_THINGSPEAK_URL_ENV "${CMAKE_MATCH_1}")
     endif()
     if(line MATCHES "^MAJU_MQTT_ENABLE=(.*)$")
          set(MAJU_MQTT_ENABLE_ENV "${CMAKE_MATCH_1}")
     endif()
     if(line MATCHES "^MAJU_MQTT_BROKER_URI=(.*)$")
          set(MAJU_MQTT_BROKER_URI_ENV "${CMAKE_MATCH_1}")
     endif()
     if(line MATCHES "^MAJU_MQTT_USERNAME=(.*)$")
          set(MAJU_MQTT_USERNAME_ENV "${CMAKE_MATCH_1}")
     endif()
     if(line MATCHES "^MAJU_MQTT_PASSWORD=(.*)$")
          set(MAJU_MQTT_PASSWORD_ENV "${CMAKE_MATCH_1}")
     endif()
     if(line MATCHES "^MAJU_MQTT_TOPIC=(.*)$")
          set(MAJU_MQTT_TOPIC_ENV "${CMAKE_MATCH_1}")
     endif()
     if(line MATCHES "^MAJU_MQTT_THERMAL_TOPIC=(.*)$")
          set(MAJU_MQTT_THERMAL_TOPIC_ENV "${CMAKE_MATCH_1}")
     endif()
     if(line MATCHES "^MAJU_MQTT_CLIENT_ID=(.*)$")
          set(MAJU_MQTT_CLIENT_ID_ENV "${CMAKE_MATCH_1}")
     endif()
endforeach()

if(NOT DEFINED MAJU_WIFI_SSID_ENV)
     set(MAJU_WIFI_SSID_ENV "")
endif()

if(NOT DEFINED MAJU_WIFI_PASSWORD_ENV)
     set(MAJU_WIFI_PASSWORD_ENV "")
endif()

# MAJU_LEAF_OFFSET_C ficou obsoleto: a temperatura da folha e medida pelo MLX90614/AMG8833.
if(DEFINED MAJU_LEAF_OFFSET_C_ENV)
     message(WARNING "MAJU_LEAF_OFFSET_C esta obsoleto e e ignorado: a temperatura da folha agora vem do MLX90614 (fallback AMG8833). Remova a linha do .env.")
endif()

if(NOT DEFINED MAJU_THINGSPEAK_ENABLE_ENV)
     set(MAJU_THINGSPEAK_ENABLE_ENV "0")
endif()

if(NOT DEFINED MAJU_THINGSPEAK_WRITE_API_KEY_ENV)
     set(MAJU_THINGSPEAK_WRITE_API_KEY_ENV "")
endif()

if(NOT DEFINED MAJU_THINGSPEAK_URL_ENV)
     set(MAJU_THINGSPEAK_URL_ENV "https://api.thingspeak.com/update")
endif()

if(NOT DEFINED MAJU_MQTT_ENABLE_ENV)
     set(MAJU_MQTT_ENABLE_ENV "0")
endif()
if(NOT DEFINED MAJU_MQTT_BROKER_URI_ENV)
     set(MAJU_MQTT_BROKER_URI_ENV "")
endif()
if(NOT DEFINED MAJU_MQTT_USERNAME_ENV)
     set(MAJU_MQTT_USERNAME_ENV "")
endif()
if(NOT DEFINED MAJU_MQTT_PASSWORD_ENV)
     set(MAJU_MQTT_PASSWORD_ENV "")
endif()
if(NOT DEFINED MAJU_MQTT_TOPIC_ENV)
     set(MAJU_MQTT_TOPIC_ENV "maju/vpd")
endif()
if(NOT DEFINED MAJU_MQTT_THERMAL_TOPIC_ENV)
     set(MAJU_MQTT_THERMAL_TOPIC_ENV "maju/thermal")
endif()
if(NOT DEFINED MAJU_MQTT_CLIENT_ID_ENV)
     set(MAJU_MQTT_CLIENT_ID_ENV "maju_vpd")
endif()

string(STRIP "${MAJU_WIFI_SSID_ENV}" MAJU_WIFI_SSID_ENV)
string(STRIP "${MAJU_WIFI_PASSWORD_ENV}" MAJU_WIFI_PASSWORD_ENV)
string(STRIP "${MAJU_THINGSPEAK_ENABLE_ENV}" MAJU_THINGSPEAK_ENABLE_ENV)
string(STRIP "${MAJU_THINGSPEAK_WRITE_API_KEY_ENV}" MAJU_THINGSPEAK_WRITE_API_KEY_ENV)
string(STRIP "${MAJU_THINGSPEAK_URL_ENV}" MAJU_THINGSPEAK_URL_ENV)
string(STRIP "${MAJU_MQTT_ENABLE_ENV}" MAJU_MQTT_ENABLE_ENV)
string(STRIP "${MAJU_MQTT_BROKER_URI_ENV}" MAJU_MQTT_BROKER_URI_ENV)
string(STRIP "${MAJU_MQTT_USERNAME_ENV}" MAJU_MQTT_USERNAME_ENV)
string(STRIP "${MAJU_MQTT_PASSWORD_ENV}" MAJU_MQTT_PASSWORD_ENV)
string(STRIP "${MAJU_MQTT_TOPIC_ENV}" MAJU_MQTT_TOPIC_ENV)
string(STRIP "${MAJU_MQTT_THERMAL_TOPIC_ENV}" MAJU_MQTT_THERMAL_TOPIC_ENV)
string(STRIP "${MAJU_MQTT_CLIENT_ID_ENV}" MAJU_MQTT_CLIENT_ID_ENV)
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_WIFI_SSID_ENV "${MAJU_WIFI_SSID_ENV}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_WIFI_PASSWORD_ENV "${MAJU_WIFI_PASSWORD_ENV}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_THINGSPEAK_ENABLE_ENV "${MAJU_THINGSPEAK_ENABLE_ENV}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_THINGSPEAK_WRITE_API_KEY_ENV "${MAJU_THINGSPEAK_WRITE_API_KEY_ENV}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_THINGSPEAK_URL_ENV "${MAJU_THINGSPEAK_URL_ENV}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_MQTT_ENABLE_ENV "${MAJU_MQTT_ENABLE_ENV}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_MQTT_BROKER_URI_ENV "${MAJU_MQTT_BROKER_URI_ENV}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_MQTT_USERNAME_ENV "${MAJU_MQTT_USERNAME_ENV}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_MQTT_PASSWORD_ENV "${MAJU_MQTT_PASSWORD_ENV}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_MQTT_TOPIC_ENV "${MAJU_MQTT_TOPIC_ENV}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_MQTT_THERMAL_TOPIC_ENV "${MAJU_MQTT_THERMAL_TOPIC_ENV}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_MQTT_CLIENT_ID_ENV "${MAJU_MQTT_CLIENT_ID_ENV}")

if("${MAJU_WIFI_SSID_ENV}" STREQUAL "" OR "${MAJU_WIFI_PASSWORD_ENV}" STREQUAL "")
     message(FATAL_ERROR "MAJU_WIFI_SSID e MAJU_WIFI_PASSWORD devem estar definidos no .env.")
endif()

string(TOLOWER "${MAJU_THINGSPEAK_ENABLE_ENV}" MAJU_THINGSPEAK_ENABLE_NORMALIZED)
if(MAJU_THINGSPEAK_ENABLE_NORMALIZED STREQUAL "1" OR
   MAJU_THINGSPEAK_ENABLE_NORMALIZED STREQUAL "true" OR
   MAJU_THINGSPEAK_ENABLE_NORMALIZED STREQUAL "yes" OR
   MAJU_THINGSPEAK_ENABLE_NORMALIZED STREQUAL "on")
     set(MAJU_THINGSPEAK_ENABLE_LITERAL 1)
else()
     set(MAJU_THINGSPEAK_ENABLE_LITERAL 0)
endif()

if(MAJU_THINGSPEAK_ENABLE_LITERAL EQUAL 1 AND "${MAJU_THINGSPEAK_WRITE_API_KEY_ENV}" STREQUAL "")
     message(FATAL_ERROR "MAJU_THINGSPEAK_ENABLE esta ativo, mas MAJU_THINGSPEAK_WRITE_API_KEY esta vazio no .env.")
endif()

string(TOLOWER "${MAJU_MQTT_ENABLE_ENV}" MAJU_MQTT_ENABLE_NORMALIZED)
if(MAJU_MQTT_ENABLE_NORMALIZED STREQUAL "1" OR
   MAJU_MQTT_ENABLE_NORMALIZED STREQUAL "true" OR
   MAJU_MQTT_ENABLE_NORMALIZED STREQUAL "yes" OR
   MAJU_MQTT_ENABLE_NORMALIZED STREQUAL "on")
     set(MAJU_MQTT_ENABLE_LITERAL 1)
else()
     set(MAJU_MQTT_ENABLE_LITERAL 0)
endif()

if(MAJU_MQTT_ENABLE_LITERAL EQUAL 1 AND "${MAJU_MQTT_BROKER_URI_ENV}" STREQUAL "")
     message(FATAL_ERROR "MAJU_MQTT_ENABLE esta ativo, mas MAJU_MQTT_BROKER_URI esta vazio no .env.")
endif()

if(MAJU_MQTT_ENABLE_LITERAL EQUAL 1 AND "${MAJU_MQTT_TOPIC_ENV}" STREQUAL "")
     message(FATAL_ERROR "MAJU_MQTT_ENABLE esta ativo, mas MAJU_MQTT_TOPIC esta vazio no .env.")
endif()

if(MAJU_MQTT_ENABLE_LITERAL EQUAL 1 AND "${MAJU_MQTT_THERMAL_TOPIC_ENV}" STREQUAL "")
     message(FATAL_ERROR "MAJU_MQTT_ENABLE esta ativo, mas MAJU_MQTT_THERMAL_TOPIC esta vazio no .env.")
endif()

if(MAJU_MQTT_ENABLE_LITERAL EQUAL 1 AND "${MAJU_MQTT_THERMAL_TOPIC_ENV}" STREQUAL "${MAJU_MQTT_TOPIC_ENV}")
     message(FATAL_ERROR "MAJU_MQTT_THERMAL_TOPIC deve ser diferente de MAJU_MQTT_TOPIC no .env.")
endif()

configure_file(${CMAKE_CURRENT_LIST_DIR}/wifi_env.h.in ${CMAKE_CURRENT_BINARY_DIR}/wifi_env.h @ONLY)

idf_component_register(SRCS "esp_maju_vdp_main.c"
                            "hal/sht3x.c"
                            "hal/mlx90614.c"
                            "hal/amg8833.c"
                            "domain/vpd.c"
                            "domain/thermal.c"
                            "telemetry/telemetry_dispatch.c"
                            "telemetry/telemetry_fields.c"
                            "telemetry/thingspeak/telemetry_thingspeak.c"
                            "telemetry/mqtt/telemetry_mqtt.c"
                       INCLUDE_DIRS "."
                                    "hal"
                                    "domain"
                                    "telemetry"
                                    "telemetry/thingspeak"
                                    "telemetry/mqtt"
                                    "${CMAKE_CURRENT_BINARY_DIR}"
                       PRIV_REQUIRES esp_driver_i2c esp_wifi esp_event esp_netif nvs_flash esp_http_client mbedtls mqtt)
