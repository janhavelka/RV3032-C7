idf_component_register(
  SRCS "main.cpp" "Rv3032IdfI2c.cpp"
  REQUIRES RV3032 esp_driver_i2c esp_driver_gpio esp_timer freertos
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
