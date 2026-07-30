idf_component_register(SRCS "esp_maju_vdp_main.c"
                            "sht3x.c"
                            "vpd.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_i2c)
