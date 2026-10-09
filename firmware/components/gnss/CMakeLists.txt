idf_component_register(
    SRCS "component.cpp" "ubx.cpp" "fix_validation.cpp"
    INCLUDE_DIRS "include"
    REQUIRES domain board
)
