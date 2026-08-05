set(MAJU_ENV_FILE "${PROJECT_DIR}/.env")
if(NOT EXISTS "${MAJU_ENV_FILE}")
     message(FATAL_ERROR "Arquivo .env nao encontrado em ${MAJU_ENV_FILE}. Copie .env.example para .env e preencha MAJU_WIFI_SSID e MAJU_WIFI_PASSWORD.")
endif()

file(STRINGS "${MAJU_ENV_FILE}" MAJU_ENV_LINES REGEX "^MAJU_WIFI_(SSID|PASSWORD)=")

foreach(line IN LISTS MAJU_ENV_LINES)
     if(line MATCHES "^MAJU_WIFI_SSID=(.*)$")
          set(MAJU_WIFI_SSID_ENV "${CMAKE_MATCH_1}")
     endif()
     if(line MATCHES "^MAJU_WIFI_PASSWORD=(.*)$")
          set(MAJU_WIFI_PASSWORD_ENV "${CMAKE_MATCH_1}")
     endif()
endforeach()

if(NOT DEFINED MAJU_WIFI_SSID_ENV)
     set(MAJU_WIFI_SSID_ENV "")
endif()

if(NOT DEFINED MAJU_WIFI_PASSWORD_ENV)
     set(MAJU_WIFI_PASSWORD_ENV "")
endif()

string(STRIP "${MAJU_WIFI_SSID_ENV}" MAJU_WIFI_SSID_ENV)
string(STRIP "${MAJU_WIFI_PASSWORD_ENV}" MAJU_WIFI_PASSWORD_ENV)
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_WIFI_SSID_ENV "${MAJU_WIFI_SSID_ENV}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" MAJU_WIFI_PASSWORD_ENV "${MAJU_WIFI_PASSWORD_ENV}")

if("${MAJU_WIFI_SSID_ENV}" STREQUAL "" OR "${MAJU_WIFI_PASSWORD_ENV}" STREQUAL "")
     message(FATAL_ERROR "MAJU_WIFI_SSID e MAJU_WIFI_PASSWORD devem estar definidos no .env.")
endif()

configure_file(${CMAKE_CURRENT_LIST_DIR}/wifi_env.h.in ${CMAKE_CURRENT_BINARY_DIR}/wifi_env.h @ONLY)

idf_component_register(SRCS "esp_maju_vdp_main.c"
                            "sht3x.c"
                            "vpd.c"
                            INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
                       PRIV_REQUIRES esp_driver_i2c esp_wifi esp_event esp_netif nvs_flash)
