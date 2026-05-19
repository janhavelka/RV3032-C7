idf_component_register(
  SRCS "src/RV3032.cpp"
  INCLUDE_DIRS "include"
  REQUIRES esp_timer
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
