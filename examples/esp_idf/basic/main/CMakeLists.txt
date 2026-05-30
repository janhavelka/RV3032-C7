idf_component_register(
  SRCS
    "main.cpp"
  INCLUDE_DIRS
    "."
  REQUIRES
    RV3032-C7
    esp_common
    esp_driver_i2c
    esp_timer
    freertos
    log
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
