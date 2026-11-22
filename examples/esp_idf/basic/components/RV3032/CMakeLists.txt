# Give the component a fixed name. EXTRA_COMPONENT_DIRS would derive it from
# the checkout or extracted package directory, breaking main's REQUIRES RV3032.
set(rv3032_root "${CMAKE_CURRENT_LIST_DIR}/../../../../..")
idf_component_register(
  SRCS "${rv3032_root}/src/RV3032.cpp"
  INCLUDE_DIRS "${rv3032_root}/include"
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
