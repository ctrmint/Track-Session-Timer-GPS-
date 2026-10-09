idf_component_register(
    SRCS "component.cpp" "ubx.cpp" "fix_validation.cpp" "pipeline.cpp" "i2c_transport.cpp" "i2c_transport_esp.cpp"
    INCLUDE_DIRS "include"
    REQUIRES domain board
)
