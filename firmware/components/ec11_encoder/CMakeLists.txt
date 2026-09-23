idf_component_register(SRCS "ec11.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver esp_pm esp_timer
                       PRIV_REQUIRES log esp_hw_support)
