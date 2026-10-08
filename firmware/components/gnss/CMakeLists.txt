idf_component_register(
    SRCS "component.cpp" "ubx.cpp"
    INCLUDE_DIRS "include"
    REQUIRES domain board
)
