IDF_PATH  ?= $(HOME)/esp/esp-idf
UNITY_DIR := $(IDF_PATH)/components/unity/unity/src

CC     := gcc
CFLAGS := -std=c11 -Wall -Wextra -g \
          -Istubs \
          -I../../main/hal \
          -I../../main/domain \
          -I../../main/telemetry \
          -I../../main/telemetry/thingspeak \
          -I../../main/telemetry/mqtt \
          -I$(UNITY_DIR)

SRCS := test_runner.c \
        test_vpd.c \
        test_thermal.c \
        test_sht3x.c \
        test_mlx90614.c \
        test_amg8833.c \
        test_telemetry.c \
        test_fields.c \
        test_thingspeak.c \
        test_mqtt.c \
        test_dispatch.c \
        i2c_stub.c \
        thingspeak_http_stub.c \
        mqtt_stub.c \
        ../../main/domain/vpd.c \
        ../../main/domain/thermal.c \
        ../../main/hal/sht3x.c \
        ../../main/hal/mlx90614.c \
        ../../main/hal/amg8833.c \
        ../../main/telemetry/thingspeak/telemetry_thingspeak.c \
        ../../main/telemetry/mqtt/telemetry_mqtt.c \
        ../../main/telemetry/telemetry_dispatch.c \
        ../../main/telemetry/telemetry_fields.c \
        $(UNITY_DIR)/unity.c

TARGET := run_tests

.PHONY: all clean

all: $(TARGET)
	./$(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

clean:
	rm -f $(TARGET)
