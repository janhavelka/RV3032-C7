idf_build_get_property(target IDF_TARGET)
if(target STREQUAL "esp32s3")
  set(console_component esp_driver_usb_serial_jtag)
else()
  set(console_component esp_usb_cdc_rom_console)
endif()
idf_component_register(
  SRCS "main.cpp" "Rv3032IdfI2c.cpp" "../../../common/Rv3032Cli.cpp"
  PRIV_INCLUDE_DIRS "../../../common"
  REQUIRES RV3032 esp_driver_i2c esp_driver_gpio esp_timer freertos
           spi_flash esp_psram ${console_component}
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

# Recheck HEAD and tracked changes on incremental builds too.
set(identity_header "${CMAKE_CURRENT_BINARY_DIR}/Rv3032BuildIdentity.h")
add_custom_target(rv3032_example_identity
  COMMAND "${CMAKE_COMMAND}"
    "-DSOURCE_ROOT=${CMAKE_CURRENT_LIST_DIR}/../../../.."
    "-DOUTPUT_HEADER=${identity_header}"
    -P "${CMAKE_CURRENT_LIST_DIR}/GenerateBuildIdentity.cmake"
  BYPRODUCTS "${identity_header}"
  VERBATIM
)
add_dependencies(${COMPONENT_LIB} rv3032_example_identity)
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
