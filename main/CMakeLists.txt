set(MAJU_ENV_FILE "${PROJECT_DIR}/.env")
if(NOT EXISTS "${MAJU_ENV_FILE}")
     message(FATAL_ERROR "Arquivo .env nao encontrado em ${MAJU_ENV_FILE}. Copie .env.example para .env e preencha as variaveis obrigatorias.")
endif()

file(STRINGS "${MAJU_ENV_FILE}" MAJU_ENV_LINES REGEX "^MAJU_(WIFI_(SSID|PASSWORD)|LEAF_OFFSET_C|THINGSPEAK_(ENABLE|WRITE_API_KEY|URL))=")

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
endforeach()

if(NOT DEFINED MAJU_WIFI_SSID_ENV)
     set(MAJU_WIFI_SSID_ENV "")
endif()

if(NOT DEFINED MAJU_WIFI_PASSWORD_ENV)
     set(MAJU_WIFI_PASSWORD_ENV "")
endif()

if(NOT DEFINED MAJU_LEAF_OFFSET_C_ENV)
     set(MAJU_LEAF_OFFSET_C_ENV "")
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

string(STRIP "${MAJU_WIFI_SSID_ENV}" MAJU_WIFI_SSID_ENV)
string(STRIP "${MAJU_WIFI_PASSWORD_ENV}" MAJU_WIFI_PASSWORD_ENV)
string(STRIP "${MAJU_LEAF_OFFSET_C_ENV}" MAJU_LEAF_OFFSET_C_ENV)
string(STRIP "${MAJU_THINGSPEAK_ENABLE_ENV}" MAJU_THINGSPEAK_ENABLE_ENV)
string(STRIP "${MAJU_THINGSPEAK_WRITE_API_KEY_ENV}" MAJU_THINGSPEAK_WRITE_API_KEY_ENV)
string(STRIP "${MAJU_THINGSPEAK_URL_ENV}" MAJU_THINGSPEAK_URL_ENV)
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_WIFI_SSID_ENV "${MAJU_WIFI_SSID_ENV}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_WIFI_PASSWORD_ENV "${MAJU_WIFI_PASSWORD_ENV}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_LEAF_OFFSET_C_ENV "${MAJU_LEAF_OFFSET_C_ENV}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_THINGSPEAK_ENABLE_ENV "${MAJU_THINGSPEAK_ENABLE_ENV}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_THINGSPEAK_WRITE_API_KEY_ENV "${MAJU_THINGSPEAK_WRITE_API_KEY_ENV}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_THINGSPEAK_URL_ENV "${MAJU_THINGSPEAK_URL_ENV}")

if("${MAJU_WIFI_SSID_ENV}" STREQUAL "" OR "${MAJU_WIFI_PASSWORD_ENV}" STREQUAL "" OR "${MAJU_LEAF_OFFSET_C_ENV}" STREQUAL "")
     message(FATAL_ERROR "MAJU_WIFI_SSID, MAJU_WIFI_PASSWORD e MAJU_LEAF_OFFSET_C devem estar definidos no .env.")
endif()

if(NOT MAJU_LEAF_OFFSET_C_ENV MATCHES "^[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)$")
     message(FATAL_ERROR "MAJU_LEAF_OFFSET_C invalido no .env. Use formato numerico com sinal opcional, por exemplo: +2.0, -1.5, 0.0")
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

set(MAJU_LEAF_OFFSET_C_LITERAL "${MAJU_LEAF_OFFSET_C_ENV}f")

configure_file(${CMAKE_CURRENT_LIST_DIR}/wifi_env.h.in ${CMAKE_CURRENT_BINARY_DIR}/wifi_env.h @ONLY)

idf_component_register(SRCS "esp_maju_vdp_main.c"
                            "hal/sht3x.c"
                            "domain/vpd.c"
                            "telemetry/thingspeak/telemetry_thingspeak.c"
                       INCLUDE_DIRS "."
                                    "hal"
                                    "domain"
                                    "telemetry"
                                    "telemetry/thingspeak"
                                    "${CMAKE_CURRENT_BINARY_DIR}"
                       PRIV_REQUIRES esp_driver_i2c esp_wifi esp_event esp_netif nvs_flash esp_http_client mbedtls)
