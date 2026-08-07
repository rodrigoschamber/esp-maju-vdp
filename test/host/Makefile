IDF_PATH  ?= $(HOME)/esp/esp-idf
UNITY_DIR := $(IDF_PATH)/components/unity/unity/src

CC     := gcc
CFLAGS := -std=c11 -Wall -Wextra -g \
          -Istubs \
          -I../../main/hal \
          -I../../main/domain \
          -I../../main/telemetry \
          -I../../main/telemetry/thingspeak \
          -I$(UNITY_DIR)

SRCS := test_runner.c \
        test_vpd.c \
        test_sht3x.c \
        test_telemetry.c \
        test_thingspeak.c \
        i2c_stub.c \
        thingspeak_http_stub.c \
        ../../main/domain/vpd.c \
        ../../main/hal/sht3x.c \
        ../../main/telemetry/thingspeak/telemetry_thingspeak.c \
        $(UNITY_DIR)/unity.c

TARGET := run_tests

.PHONY: all clean

all: $(TARGET)
	./$(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

clean:
	rm -f $(TARGET)
